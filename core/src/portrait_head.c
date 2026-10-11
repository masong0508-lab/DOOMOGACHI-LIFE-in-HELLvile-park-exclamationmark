/* portrait_head.c - heads, faces and species traits (owner: portrait).
 *
 * Everything here is drawn in normalised head space: (0,0) is the head
 * centre, 256 is one head radius (rx horizontally, ry vertically).
 * phs.s mirrors symmetric parts (-1 = viewer's left), phs.z is the depth
 * used to rotate a feature for 3/4 and profile views.
 */
#include "portrait_int.h"
#include "doomo/resident.h"

PHSpace phs;

/* ---- head-space helpers ----------------------------------------------------- */
int hx(int n)
{
    PHead *h = phs.h;
    int x = phs.s * n;
    if (h->yaw) x = (x * h->cosv - phs.z * h->sinv) / 256;
    return h->cx + x * h->rx / 256;
}
int hy(int n) { return phs.h->cy + n * phs.h->ry / 256; }
int hr(int n) { return n * phs.h->rx / 256; }
int hry(int n) { return n * phs.h->ry / 256; }

void hE(int x, int y, int rx, int ry) { pc_ellipse(hx(x), hy(y), hr(rx), hry(ry)); }
void hT(int ax, int ay, int bx, int by, int cx, int cy)
{
    pc_tri(hx(ax), hy(ay), hx(bx), hy(by), hx(cx), hy(cy));
}
void hQ(int ax, int ay, int bx, int by, int cx, int cy, int dx, int dy)
{
    pc_quad(hx(ax), hy(ay), hx(bx), hy(by), hx(cx), hy(cy), hx(dx), hy(dy));
}
void hC(int ax, int ay, int bx, int by, int r0, int r1)
{
    pc_capsule(hx(ax), hy(ay), hx(bx), hy(by), hr(r0), hr(r1));
}
void hB(int ax, int ay, int bx, int by, int cx, int cy, int r0, int r1, int segs)
{
    pc_bezier(hx(ax), hy(ay), hx(bx), hy(by), hx(cx), hy(cy), hr(r0), hr(r1), segs);
}
void hL(int ax, int ay, int bx, int by, uint8_t c)
{
    pc_lineq(hx(ax), hy(ay), hx(bx), hy(by), c);
}
void hP(int x, int y, uint8_t c) { pc_px(hx(x) >> 4, hy(y) >> 4, c); }
void hFrame(int x, int y, int rx, int ry) { pc_frame(hx(x), hy(y), hr(rx), hry(ry)); }
/* head shading frame (slightly bigger than the head so hair shades along) */
void hHeadFrame(int grow) { pc_frame(phs.h->cx, phs.h->cy, hr(256 + grow), hry(256 + grow)); }

void phead_set(PHead *h, int cx, int cy, int rx, int ry, int yaw, int lod)
{
    static const int16_t cs[5] = { 256, 181, 0, -181, -256 };
    static const int16_t sn[5] = { 0, 181, 256, 181, 0 };
    h->cx = cx; h->cy = cy; h->rx = rx; h->ry = ry;
    yaw = DOOMO_CLAMP(yaw, 0, 4);
    h->yaw = yaw; h->lod = lod;
    h->cosv = cs[yaw];
    h->sinv = sn[yaw];
}

int ph_proj(const PHead *h, int nx, int ny, int front, int *ox, int *oy)
{
    int z = 0, x = nx;
    if (front) {
        int z2 = 65536 - nx * nx - ny * ny;
        z = z2 > 0 ? p_isqrt((uint32_t)z2) : 0;
        if (front < 0) z = -z;
    }
    if (h->yaw) x = (nx * h->cosv - z * h->sinv) / 256;
    *ox = h->cx + x * h->rx / 256;
    *oy = h->cy + ny * h->ry / 256;
    return h->yaw ? (nx * h->sinv + z * h->cosv) / 256 : (front ? z : 256);
}

int ph_is_critter(int sp)
{
    switch (sp) {
    case SPECIES_CATFOLK: case SPECIES_FOXKIN: case SPECIES_BEARKIN:
    case SPECIES_PUPKIN: case SPECIES_MOUSEKIN: case SPECIES_SHEEPKIN:
    case SPECIES_RACCOONKIN: case SPECIES_OWLKIN: case SPECIES_BUNNYKIN:
    case SPECIES_HELLHOUND: case SPECIES_BATLING:
        return 1;
    default:
        return 0;
    }
}

/* ---- face layout ------------------------------------------------------------------- */
typedef struct Face {
    int ex, ey;         /* eye centre (normalised; ex = +/- offset)        */
    int erx, ery;       /* eye radii (Q4)                                   */
    int by;             /* brow y (normalised)                              */
    int ny;             /* nose y                                           */
    int my;             /* mouth y                                          */
    int mw;             /* mouth half width (Q4)                            */
    int muzzle;         /* muzzle type                                      */
    int lid;            /* eyelid colour                                    */
} Face;

static Face F;

enum { MZ_NONE, MZ_SMALL, MZ_ROUND, MZ_SNOUT, MZ_DISC, MZ_DARK };

static int species_muzzle(int sp)
{
    switch (sp) {
    case SPECIES_CATFOLK: case SPECIES_MOUSEKIN: case SPECIES_BUNNYKIN:
        return MZ_SMALL;
    case SPECIES_BEARKIN: case SPECIES_PUPKIN: case SPECIES_SHEEPKIN:
        return MZ_ROUND;
    case SPECIES_FOXKIN: case SPECIES_RACCOONKIN:
        return MZ_SNOUT;
    case SPECIES_OWLKIN:
        return MZ_DISC;
    case SPECIES_HELLHOUND:
        return MZ_DARK;
    default:
        return MZ_NONE;
    }
}

static void face_layout(PCtx *c)
{
    const DoomoLook *l = c->l;
    PHead *h = &c->h;
    int big = h->lod == LOD_BIG;
    F.ex = 60 + l->eye_spacing * 6;                          /* 24%..59% */
    F.ey = -14 + l->eye_height * 6;                          /* -5%..+30% */
    F.ery = h->ry * (17 + l->eye_size * 15 / 10) / 100;      /* 17%..39% */
    F.erx = F.ery * 80 / 100;
    if (l->eye_count == 1) { F.erx = F.erx * 5 / 4; F.ery = F.ery * 5 / 4; }
    if (!big) {
        F.ery = DOOMO_MAX(F.ery, 20);
        F.erx = DOOMO_MAX(F.erx, 16);
    }
    F.by = F.ey - (F.ery * 256 / h->ry) - 26 - l->brow_height * 4;
    F.my = 120 + (l->eye_height > 10 ? (l->eye_height - 10) * 5 : 0);
    F.ny = (F.ey + F.my) / 2 + 12;
    F.mw = h->rx * (6 + l->mouth_size * 15 / 10) / 100;
    if (F.mw < 16) F.mw = 16;
    F.muzzle = species_muzzle(l->species);
    if (F.my > 200) F.my = 200;
}

/* face point -> Q4 with yaw; returns visibility (<= 0 hidden) */
static int fpt(int nx, int ny, int *X, int *Y)
{
    return ph_proj(phs.h, nx, ny, 1, X, Y);
}

/* foreshortened width for a feature with visibility v */
static int fshort(int w, int v)
{
    if (v >= 256) return w;
    if (v < 64) v = 64;
    return w * v / 256;
}

/* ---- colours ------------------------------------------------------------------------ */
#define INK      PAL_RAMP(RAMP_PASTEL_PINK, 0)   /* warm near-black for lines */
#define MOUTHIN  PAL_RAMP(RAMP_PASTEL_PINK, 1)
#define TONGUE   PAL_RAMP(RAMP_PASTEL_PINK, 4)
#define BLUSH    PAL_RAMP(RAMP_PASTEL_PINK, 5)
#define BLUSH_D  PAL_RAMP(RAMP_PASTEL_PINK, 4)
#define TEETH    PAL_WHITE
#define NOSEPINK PAL_RAMP(RAMP_PASTEL_PINK, 4)

static uint8_t face_ink(PCtx *c)
{
    /* line colour for mouths / closed eyes: dark local tone */
    uint8_t k = c->skin.line;
    if (palette_luma(k) > 90) k = palette_darker(k);
    if (palette_luma(k) > 60 || c->creepy) k = INK;
    return k;
}

/* ---- head shapes ----------------------------------------------------------------------- */
static void head_shape(PCtx *c)
{
    int shape = c->l->head_shape;
    hHeadFrame(0);
    switch (shape) {
    case HEAD_ROUND:
    default:
        hE(0, 0, 256, 256);
        break;
    case HEAD_OVAL:
        hE(0, -6, 226, 270);
        break;
    case HEAD_SQUARE:
        pc_rrect(hx(0), hy(4), hr(244), hry(238), hr(110));
        break;
    case HEAD_HEART:
        hE(-104, -64, 160, 190);
        hE(104, -64, 160, 190);
        hE(0, 6, 236, 196);
        hT(-236, 40, 236, 40, 0, 262);
        break;
    case HEAD_TRIANGLE:      /* soft onigiri */
        hE(0, -150, 120, 110);
        hE(-168, 150, 96, 100);
        hE(168, 150, 96, 100);
        hT(0, -248, -262, 176, 262, 176);
        hE(0, 140, 200, 112);
        break;
    case HEAD_PEAR:
        hE(0, -60, 196, 196);
        hE(0, 92, 262, 170);
        break;
    case HEAD_BEAN:
        hE(-40, -30, 232, 226);
        hE(70, 70, 208, 186);
        break;
    case HEAD_SKULL:
        hE(0, -40, 256, 224);
        pc_rrect(hx(0), hy(150), hr(150), hry(96), hr(60));
        hE(-150, 70, 96, 92);
        hE(150, 70, 96, 92);
        break;
    case HEAD_STAR: {
        int k;
        static const int16_t sx[5] = { 0, 246, 152, -152, -246 };
        static const int16_t sy[5] = { -282, -88, 236, 236, -88 };
        hE(0, 10, 224, 222);
        for (k = 0; k < 5; k++) {
            int ax = sx[(k + 4) % 5] * 45 / 100, ay = sy[(k + 4) % 5] * 45 / 100;
            int bx = sx[(k + 1) % 5] * 45 / 100, by = sy[(k + 1) % 5] * 45 / 100;
            hT(ax, ay + 10, bx, by + 10, sx[k], sy[k] + 10);
        }
        break; }
    case HEAD_BLOB:
        hE(-30, 10, 236, 230);
        hE(90, -70, 170, 170);
        hE(-120, -90, 140, 150);
        hE(120, 110, 150, 140);
        hE(-140, 120, 130, 120);
        break;
    case HEAD_DIAMOND:
        hQ(0, -276, 268, 0, 0, 268, -268, 0);
        hE(0, 0, 196, 210);
        break;
    case HEAD_MOCHI:
        hE(0, 40, 272, 214);
        pc_rrect(hx(0), hy(120), hr(262), hry(130), hr(120));
        break;
    }
}

/* fur tufts poking out of the cheeks (part of the head silhouette) */
static void cheek_tufts(PCtx *c, int big)
{
    int sp = c->l->species;
    int n = 2, y0 = 80, len = 70;
    if (sp == SPECIES_FOXKIN || sp == SPECIES_RACCOONKIN) { n = 3; len = 92; y0 = 60; }
    else if (sp == SPECIES_CATFOLK || sp == SPECIES_HELLHOUND || sp == SPECIES_BATLING) n = 2;
    else if (sp == SPECIES_IMP) { n = 1; len = 70; y0 = 70; }
    else if (sp == SPECIES_SHEEPKIN || sp == SPECIES_OWLKIN || sp == SPECIES_BEARKIN ||
             sp == SPECIES_PUPKIN || sp == SPECIES_MOUSEKIN || sp == SPECIES_BUNNYKIN) n = 2;
    if (!big && n > 2) n = 2;
    for (phs.s = -1; phs.s <= 1; phs.s += 2) {
        int k;
        for (k = 0; k < n; k++) {
            int y = y0 + k * 56;
            int bx = 218 - k * 26 - (y > 140 ? (y - 140) : 0);
            int tip = bx + len - k * 14;
            hT(bx - 70, y - 44, bx - 60, y + 42, tip, y + 16 + k * 10);
        }
    }
    phs.s = 1;
}

/* small fluffy tuft on the crown of the head */
static void crown_tuft(void)
{
    hT(-70, -226, 10, -236, -40, -318);
    hT(-10, -236, 70, -226, 34, -300);
}

static int has_hair_cover(int style)
{
    return style != HAIR_BALD && style != HAIR_BUZZ && style != HAIR_SLICKED &&
           style != HAIR_MOHAWK && style != HAIR_SIDE_SHAVE;
}

/* species decorations that live on the head after the base fill */
static void species_head(PCtx *c, int big)
{
    const DoomoLook *l = c->l;
    int sp = l->species;
    PMat m;
    uint8_t ink = face_ink(c);
    switch (sp) {
    case SPECIES_SKELETON:
        /* cheekbone shading + jaw seam */
        pc_decal(&c->skin, SH_FLAT);
        if (big) {
            for (phs.s = -1; phs.s <= 1; phs.s += 2) {
                hL(150, 120, 200, 80, c->skin.c[1]);
                hL(60, 196, 150, 160, c->skin.c[1]);
            }
            phs.s = 1;
            hL(-60, 200, 60, 200, c->skin.c[1]);
        }
        break;
    case SPECIES_SLIME:
        /* inner bubbles + a big glossy streak */
        pm_flat(&m, c->skin.c[4]);
        pc_decal(&m, SH_FLAT);
        hE(-120, -150, 40, 30);
        hE(150, 40, 22, 22);
        hE(110, 110, 14, 14);
        pm_flat(&m, PAL_WHITE);
        pc_decal(&m, SH_FLAT);
        hC(-170, -110, -110, -190, 10, 10);
        break;
    case SPECIES_ROBOT:
        /* panel seam, rivets and a visor glare */
        pc_decal(&c->skin, SH_FLAT);
        hL(-230, -110, 230, -110, c->skin.c[0]);
        if (big) {
            hL(-230, -96, 230, -96, c->skin.c[3]);
            for (phs.s = -1; phs.s <= 1; phs.s += 2) {
                hP(200, -170, c->skin.c[0]);
                hP(196, -186, c->skin.c[4]);
                hP(214, 120, c->skin.c[0]);
            }
            phs.s = 1;
        }
        break;
    case SPECIES_PLUSHIE:
        /* stitched seam down the crown */
        if (!has_hair_cover(l->hair_style) || l->hair_style == HAIR_BUZZ) {
            int y;
            for (y = -250; y < -60; y += 44) hL(0, y, 0, y + 20, c->skin.c[0]);
        }
        /* a little fabric tag on the side */
        break;
    case SPECIES_MUMMY: {
        int k;
        pm_paint(&m, PAINT_BONE);
        pc_decal(&m, SH_FLAT);
        for (k = 0; k < 3; k++) {
            int y = -210 + k * 72;
            int y2 = y + 40 + (k & 1) * 30;
            if (y > F.ey * 1 - 90 && y < F.ey + 50) continue;
            hQ(-260, y, 260, y2, 260, y2 + 34, -260, y + 34);
        }
        hQ(-260, 150, 260, 120, 260, 154, -260, 184);
        pc_decal(&m, SH_FLAT);
        hL(-200, -178, 200, -156, m.c[1]);
        hL(-200, 182, 200, 152, m.c[1]);
        break; }
    case SPECIES_FROGLING: {
        /* pale throat */
        PMat t;
        pm_flat(&t, c->skin.c[3]);
        pc_decal(&t, SH_FLAT);
        hE(0, 200, 150, 60);
        break; }
    case SPECIES_GHOUL:
        /* sunken eye rings */
        pc_decal(&c->skin, SH_FLAT);
        if (F.erx > 0 && phs.h->yaw < 3) {
            for (phs.s = -1; phs.s <= 1; phs.s += 2) {
                int X, Y, v = fpt(phs.s * F.ex, F.ey + 30, &X, &Y);
                if (v > 0 && l->eye_count != 1) {
                    PMat r;
                    pm_flat(&r, c->skin.c[1]);
                    pc_decal(&r, SH_FLAT);
                    pc_ellipse(X, Y + 10, fshort(F.erx + 20, v), F.ery + 14);
                }
            }
            phs.s = 1;
        }
        break;
    case SPECIES_RACCOONKIN:
        /* forehead stripe */
        pm_flat(&m, c->skin.c[0]);
        pc_decal(&m, SH_FLAT);
        hT(-40, -250, 40, -250, 0, -110);
        break;
    case SPECIES_OWLKIN: {
        /* feather chevrons on the forehead */
        if (big) {
            hL(-60, -190, 0, -160, c->skin.c[1]);
            hL(0, -160, 60, -190, c->skin.c[1]);
            hL(-40, -130, 0, -110, c->skin.c[1]);
            hL(0, -110, 40, -130, c->skin.c[1]);
        }
        break; }
    case SPECIES_CHERUB:
        /* one perfect curl on the forehead */
        if (!has_hair_cover(l->hair_style)) {
            pc_decal(&c->hair, SH_BALL);
            hC(0, -246, 30, -200, 26, 18);
            hP(10, -224, c->hair.c[1]);
        }
        break;
    case SPECIES_IMP:
        /* widow's peak shading */
        pm_flat(&m, c->skin.c[1]);
        pc_decal(&m, SH_FLAT);
        hT(-60, -250, 60, -250, 0, -170);
        break;
    case SPECIES_GHOST:
        break;
    default:
        break;
    }
    (void)ink;
}

/* muzzle / facial disc / snout (critter faces) */
static void muzzle(PCtx *c)
{
    PMat m;
    const DoomoLook *l = c->l;
    int X, Y, v;
    int light = (l->marking == MARK_FUR_MUZZLE) ? 1 : 0;
    if (F.muzzle == MZ_NONE || phs.h->yaw >= 3) return;
    if (light) pm_paint(&m, l->marking_color);
    else pm_from3(&m, c->skin.c[2], c->skin.c[3], c->skin.c[4]);
    switch (F.muzzle) {
    case MZ_SMALL:
        v = fpt(0, F.my - 30, &X, &Y);
        if (v <= 0) break;
        pc_decal(&m, SH_SOFT);
        pc_frame(X, Y, fshort(hr(100), v), hry(70));
        pc_ellipse(X - fshort(hr(40), v), Y, fshort(hr(58), v), hry(52));
        pc_ellipse(X + fshort(hr(40), v), Y, fshort(hr(58), v), hry(52));
        break;
    case MZ_ROUND:
        v = fpt(0, F.my - 34, &X, &Y);
        if (v <= 0) break;
        pc_part(&m, SH_SOFT, PF_NOCAST);
        pc_ellipse(X, Y, fshort(hr(96), v), hry(70));
        break;
    case MZ_SNOUT:
        v = fpt(0, F.my - 40, &X, &Y);
        if (v <= 0) break;
        pc_decal(&m, SH_SOFT);
        pc_frame(X, Y, fshort(hr(110), v), hry(80));
        pc_ellipse(X - fshort(hr(50), v), Y + hry(6), fshort(hr(62), v), hry(56));
        pc_ellipse(X + fshort(hr(50), v), Y + hry(6), fshort(hr(62), v), hry(56));
        pc_tri(X - fshort(hr(110), v), Y, X + fshort(hr(110), v), Y, X, Y - hry(110));
        break;
    case MZ_DISC: {
        /* owl facial disc around both eyes */
        int Xl, Yl, Xr, Yr;
        pc_decal(&m, SH_SOFT);
        fpt(-F.ex, F.ey, &Xl, &Yl);
        fpt(F.ex, F.ey, &Xr, &Yr);
        pc_frame((Xl + Xr) / 2, Yl, hr(200), hry(150));
        pc_ellipse(Xl, Yl + 4, F.erx + hr(56), F.ery + hry(50));
        pc_ellipse(Xr, Yr + 4, F.erx + hr(56), F.ery + hry(50));
        fpt(0, F.ey + 60, &X, &Y);
        pc_tri(Xl - hr(30), Y - hry(40), Xr + hr(30), Y - hry(40), X, Y + hry(80));
        break; }
    case MZ_DARK:
        v = fpt(0, F.my - 34, &X, &Y);
        if (v <= 0) break;
        pm_from3(&m, c->skin.c[0], c->skin.c[1], c->skin.c[2]);
        if (light) pm_paint(&m, l->marking_color);
        pc_part(&m, SH_BALL, PF_NOCAST);
        pc_ellipse(X, Y, fshort(hr(100), v), hry(72));
        break;
    }
}

/* ---- eyes ---------------------------------------------------------------------------- */
static void lid_arc(int ex, int ey, int erx, int ery, int shear, uint8_t col, int thick)
{
    /* upper arc of the eye ellipse */
    int x;
    int xa = (ex - erx + 8) >> 4, xb = (ex + erx - 8) >> 4;
    for (x = xa; x <= xb; x++) {
        int dx = x * PQ + 8 - ex;
        int t = 65536 - ((dx * dx) / DOOMO_MAX(erx * erx / 256, 1)) * 256 / 256;
        int u = (dx << 8) / DOOMO_MAX(erx, 1);
        int rem = 65536 - u * u, yy;
        (void)t;
        if (rem < 0) rem = 0;
        yy = ey - ((p_isqrt((uint32_t)rem) * ery) >> 8) + ((dx * shear) >> 8);
        pc_px(x, (yy + 8) >> 4, col);
        if (thick > 1) pc_px(x, ((yy + 8) >> 4) + 1, col);
    }
}

static void eye_glint(int ex, int ey, int erx, int ery, int big)
{
    int gx = (ex - erx * 40 / 100) >> 4, gy = (ey - ery * 45 / 100) >> 4;
    pc_px(gx, gy, PAL_WHITE);
    if (big && erx >= 40) {
        pc_px(gx + 1, gy, PAL_WHITE);
        pc_px(gx, gy + 1, PAL_WHITE);
        if (erx >= 56) pc_px(gx + 1, gy + 1, PAL_WHITE);
    }
    if (big && ery >= 40) pc_px((ex + erx * 38 / 100) >> 4, (ey + ery * 42 / 100) >> 4, PAL_WHITE);
}

/* the default glossy critter eye */
static void eye_glossy(PCtx *c, int ex, int ey, int erx, int ery, int shear, int big, int pupil)
{
    PMat dark;
    pc_decal(&c->eye, SH_VGRAD);
    pc_ellipse_sh(ex, ey, erx, ery, shear);
    pm_flat(&dark, PAL_PAINT(PAINT_JET, 1));
    pc_decal(&dark, SH_FLAT);
    if (pupil) pc_ellipse_sh(ex, ey + ery / 10, erx * pupil / 100, ery * pupil / 100 + 4, shear);
    lid_arc(ex, ey, erx, ery, shear, F.lid, big && ery >= 44 ? 2 : 1);
    eye_glint(ex, ey, erx, ery, big);
}

static void eye_closed(int ex, int ey, int erx, int happy, uint8_t col)
{
    int x, w = erx >> 4;
    if (w < 1) w = 1;
    for (x = -w; x <= w; x++) {
        int d = (x * x * 3) / (w * w + 1);
        int y = happy ? (ey >> 4) - 1 + d - 1 : (ey >> 4) + 1 - d + (d > 1);
        if (happy) y = (ey >> 4) + d - 1;
        else y = (ey >> 4) + 1 - d;
        pc_px((ex >> 4) + x, y, col);
        if (w >= 3 && p_abs(x) < w) pc_px((ex >> 4) + x, y + (happy ? -1 : 1) * 0, col);
    }
}

void ph_heart(int cx, int cy, int r, uint8_t c, uint8_t hi)
{
    /* crisp heart, r = half width in px */
    int x, y;
    for (y = -r; y <= r + 1; y++)
        for (x = -r; x <= r; x++) {
            int in;
            if (y < 0) {
                int lx = p_abs(x) - (r + 1) / 2, ly = y;
                in = lx * lx + ly * ly * 2 <= ((r + 1) / 2) * ((r + 1) / 2) * 2 + 1;
            } else {
                in = p_abs(x) + y <= r;
            }
            if (in) pc_px(cx + x, cy + y, c);
        }
    if (hi && r >= 2) pc_px(cx - r / 2 - (r > 2), cy - (r > 2), hi);
}

void ph_star(int cx, int cy, int r, uint8_t c)
{
    int k;
    pc_px(cx, cy, c);
    for (k = 1; k <= r; k++) {
        pc_px(cx + k, cy, c); pc_px(cx - k, cy, c);
        if (k < r) { pc_px(cx, cy + k, c); pc_px(cx, cy - k, c); }
    }
    if (r >= 2) {
        pc_px(cx - 1, cy - 1, c); pc_px(cx + 1, cy - 1, c);
        pc_px(cx - 1, cy + 1, c); pc_px(cx + 1, cy + 1, c);
        pc_px(cx, cy - r, c); pc_px(cx - 1, cy + r - 1, c); pc_px(cx + 1, cy + r - 1, c);
    }
}

/* creepy possessed eye: hollow socket with a red pin of light */
static void eye_creepy(int ex, int ey, int erx, int ery, int level)
{
    PMat m;
    int cx = ex >> 4, cy = ey >> 4;
    pm_flat(&m, PAL_PAINT(PAINT_JET, 0));
    pc_decal(&m, SH_FLAT);
    pc_ellipse(ex, ey, erx + 6, ery + 8);
    pc_px(cx, cy, PAL_PAINT(PAINT_CHERRY, 2));
    pc_px(cx, cy - 1, PAL_PAINT(PAINT_CHERRY, 1));
    if (erx >= 32) {
        pc_px(cx - 1, cy, PAL_PAINT(PAINT_BLOOD, 2));
        pc_px(cx + 1, cy, PAL_PAINT(PAINT_BLOOD, 2));
        pc_px(cx, cy + 1, PAL_PAINT(PAINT_BLOOD, 2));
    }
    if (level > 1) pc_px(cx, cy, PAL_WHITE);
}

static int style_blinks(int st)
{
    switch (st) {
    case EYE_X: case EYE_BUTTON: case EYE_HOLLOW: case EYE_VOID: case EYE_GLOW:
    case EYE_COMPOUND: case EYE_SPIRAL: case EYE_HAPPY_ARC: case EYE_SQUINT:
        return 0;
    default:
        return 1;
    }
}

static void one_eye(PCtx *c, int st, int ex, int ey, int erx, int ery, int shear, int side)
{
    int big = phs.h->lod == LOD_BIG;
    int cx = ex >> 4, cy = ey >> 4;
    uint8_t ink = F.lid;
    PMat m, w;
    pm_flat(&w, PAL_WHITE);
    w.c[1] = PAL_GRAY_L; w.c[0] = PAL_GRAY_L;

    if (c->creepy) { eye_creepy(ex, ey, erx, ery, c->creepy); return; }
    if (c->dead || c->expr == EXPR_DEAD) st = EYE_X;
    else if (c->expr == EXPR_LOVE) st = EYE_HEART;
    else if (c->expr == EXPR_HAPPY && style_blinks(st)) st = EYE_HAPPY_ARC;
    else if (c->expr == EXPR_SHOCK && st != EYE_VOID && st != EYE_HOLLOW) st = EYE_WIDE;
    else if (c->expr == EXPR_SLEEPY) { eye_closed(ex, ey, erx, 0, ink); return; }
    if (c->blink && style_blinks(st) && c->expr != EXPR_SHOCK) {
        eye_closed(ex, ey + ery / 3, erx, 0, ink);
        if (big && erx >= 36) pc_px(cx + side * ((erx >> 4) + 1), cy + ery / 48 - 0, ink);
        return;
    }
    if (phs.h->lod == LOD_TINY) {
        /* two-pixel bean eyes with a glint */
        switch (st) {
        case EYE_X: pc_px(cx - 1, cy - 1, ink); pc_px(cx + 1, cy + 1, ink);
            pc_px(cx + 1, cy - 1, ink); pc_px(cx - 1, cy + 1, ink); pc_px(cx, cy, ink); return;
        case EYE_HAPPY_ARC: pc_px(cx - 1, cy, ink); pc_px(cx, cy - 1, ink); pc_px(cx + 1, cy, ink); return;
        case EYE_SQUINT: case EYE_SLEEPY: pc_px(cx - 1, cy, ink); pc_px(cx, cy, ink); pc_px(cx + 1, cy, ink); return;
        case EYE_HEART: pc_px(cx - 1, cy, PAL_PAINT(PAINT_CHERRY, 1)); pc_px(cx + 1, cy, PAL_PAINT(PAINT_CHERRY, 1));
            pc_px(cx, cy + 1, PAL_PAINT(PAINT_CHERRY, 1)); pc_px(cx, cy, PAL_PAINT(PAINT_CHERRY, 1)); return;
        case EYE_GLOW: case EYE_DEMON_SLIT:
            pc_px(cx, cy, c->eye.c[4]); pc_px(cx, cy + 1, c->eye.c[3]); return;
        case EYE_HOLLOW: case EYE_VOID:
            pc_px(cx, cy, PAL_PAINT(PAINT_JET, 1)); pc_px(cx, cy + 1, PAL_PAINT(PAINT_JET, 1));
            pc_px(cx + (st == EYE_HOLLOW ? 0 : -1), cy, st == EYE_HOLLOW ? c->eye.c[3] : PAL_PAINT(PAINT_JET, 1));
            return;
        case EYE_WIDE: pc_px(cx, cy, PAL_WHITE); pc_px(cx, cy + 1, PAL_WHITE); pc_px(cx + side, cy, ink);
            pc_px(cx + side, cy + 1, ink); return;
        default:
            pc_px(cx, cy, ink); pc_px(cx, cy + 1, c->eye.c[1]);
            if (erx >= 24) { pc_px(cx + side, cy, ink); pc_px(cx + side, cy + 1, c->eye.c[2]); }
            pc_px(cx - (erx >= 24 && side < 0 ? 1 : 0), cy, PAL_WHITE);
            return;
        }
    }
    switch (st) {
    case EYE_DOT:
        pm_flat(&m, PAL_PAINT(PAINT_JET, 1));
        pc_decal(&m, SH_FLAT);
        pc_ellipse(ex, ey + ery / 4, erx * 45 / 100 + 6, ery * 55 / 100 + 6);
        pc_px((ex - erx / 5) >> 4, (ey) >> 4, PAL_WHITE);
        break;
    case EYE_ROUND:
    default:
        eye_glossy(c, ex, ey, erx, ery, shear, big, 52);
        break;
    case EYE_ANIME:
        pc_decal(&w, SH_FLAT);
        pc_ellipse_sh(ex, ey, erx + 4, ery, shear);
        pc_decal(&c->eye, SH_VGRAD);
        pc_ellipse_sh(ex + side * 4, ey + ery / 8, erx * 78 / 100, ery * 84 / 100, shear);
        pm_flat(&m, PAL_PAINT(PAINT_JET, 1));
        pc_decal(&m, SH_FLAT);
        pc_ellipse_sh(ex + side * 4, ey + ery / 6, erx * 36 / 100 + 4, ery * 44 / 100 + 4, shear);
        lid_arc(ex, ey - 4, erx + 8, ery + 4, shear, ink, 2);
        pc_px(cx + side * ((erx >> 4) + 1), (ey - ery) >> 4, ink);
        pc_px(cx + side * ((erx >> 4) + 2), ((ey - ery) >> 4) - 1, ink);
        eye_glint(ex, ey, erx, ery, big);
        break;
    case EYE_SLEEPY:
        pc_clip(0, (ey >> 4), pcv.w, pcv.h);
        eye_glossy(c, ex, ey, erx, ery, shear, big, 52);
        pc_noclip();
        pc_line((ex - erx) >> 4, ey >> 4, (ex + erx) >> 4, ey >> 4, ink);
        pc_line((ex - erx) >> 4, (ey >> 4) - 1, (ex + erx) >> 4, (ey >> 4) - 1, c->skin.c[1]);
        break;
    case EYE_ANGRY: {
        int k;
        eye_glossy(c, ex, ey, erx, ery, shear, big, 52);
        /* slanted lid: inner corner pulled down */
        for (k = 0; k <= (erx >> 3); k++) {
            int x = cx - side * (erx >> 4) + side * k;
            int y0 = ((ey - ery) >> 4) - 1, y1 = y0 + ((erx >> 3) - k) * (ery >> 4) / DOOMO_MAX(erx >> 3, 1) + 1;
            int y;
            for (y = y0; y <= y1; y++) pc_px(x, y, y == y1 ? ink : c->skin.c[2]);
        }
        break; }
    case EYE_HAPPY_ARC: {
        int x, wdt = DOOMO_MAX(erx >> 4, 1), hgt = DOOMO_MAX(ery >> 5, 1);
        for (x = -wdt; x <= wdt; x++) {
            int y = cy - hgt + (x * x * (hgt + 1)) / (wdt * wdt + 1);
            pc_px(cx + x, y, ink);
            if (big) pc_px(cx + x, y + 1, ink);
        }
        break; }
    case EYE_CAT:
        pc_decal(&c->eye, SH_VGRAD);
        pc_ellipse_sh(ex, ey, erx + 6, ery * 85 / 100, shear - side * 40);
        pm_flat(&m, PAL_PAINT(PAINT_JET, 1));
        pc_decal(&m, SH_FLAT);
        pc_ellipse(ex, ey, DOOMO_MAX(erx / 5, 10), ery * 75 / 100);
        lid_arc(ex, ey, erx + 6, ery * 85 / 100, shear - side * 40, ink, big ? 2 : 1);
        pc_px((ex - erx / 2) >> 4, (ey - ery / 3) >> 4, PAL_WHITE);
        break;
    case EYE_GOAT:
        pc_decal(&c->eye, SH_VGRAD);
        pc_ellipse_sh(ex, ey, erx + 4, ery * 80 / 100, shear);
        pm_flat(&m, PAL_PAINT(PAINT_JET, 1));
        pc_decal(&m, SH_FLAT);
        pc_rrect(ex, ey, erx * 70 / 100, DOOMO_MAX(ery / 6, 10), 8);
        lid_arc(ex, ey, erx + 4, ery * 80 / 100, shear, ink, 1);
        pc_px((ex - erx / 2) >> 4, (ey - ery / 2) >> 4, PAL_WHITE);
        break;
    case EYE_SPIRAL: {
        int k, r = DOOMO_MAX(erx >> 4, 2);
        pc_decal(&w, SH_FLAT);
        pc_ellipse(ex, ey, erx + 4, erx + 4);
        for (k = 0; k < r * 10; k++) {
            int a = k * 26, rr = (k * 16) / 10;
            pc_px(((ex + (p_cos(a) * rr * side) / 256) + 4) >> 4, ((ey + (p_sin(a) * rr) / 256) + 4) >> 4, ink);
        }
        break; }
    case EYE_STAR: {
        int r = DOOMO_MAX(erx >> 4, 2) + 1;
        ph_star(cx, cy, r, c->eye.c[1]);
        ph_star(cx, cy, r - 1, c->eye.c[3]);
        pc_px(cx, cy, PAL_WHITE);
        break; }
    case EYE_HEART: {
        int r = DOOMO_MAX(erx >> 4, 2);
        uint8_t hc = c->expr == EXPR_LOVE ? PAL_PAINT(PAINT_CHERRY, 1) : c->eye.c[2];
        ph_heart(cx, cy - 1, r + 1, palette_darker(hc), 0);
        ph_heart(cx, cy - 1, r, hc, PAL_WHITE);
        break; }
    case EYE_X: {
        int r = DOOMO_MAX(erx >> 4, 1);
        pc_line(cx - r, cy - r, cx + r, cy + r, ink);
        pc_line(cx - r, cy + r, cx + r, cy - r, ink);
        if (big) {
            pc_line(cx - r + 1, cy - r, cx + r, cy + r - 1, ink);
            pc_line(cx - r + 1, cy + r, cx + r, cy - r + 1, ink);
        }
        break; }
    case EYE_BUTTON:
        pm_paint(&m, c->l->eye_color);
        pc_decal(&m, SH_BALL);
        pc_ellipse(ex, ey, erx + 6, erx + 6);
        pc_px(cx, cy, m.c[0]);
        pc_px(cx - 1, cy - 1, m.c[0]); pc_px(cx + 1, cy - 1, m.c[0]);
        pc_px(cx - 1, cy + 1, m.c[0]); pc_px(cx + 1, cy + 1, m.c[0]);
        pc_px(cx - 1, cy, PAL_WHITE); pc_px(cx + 1, cy, PAL_WHITE);
        pc_px(cx, cy - 1, PAL_WHITE); pc_px(cx, cy + 1, PAL_WHITE);
        if (erx >= 40) pc_px(cx - (erx >> 4), cy - (erx >> 4) + 1, m.c[4]);
        break;
    case EYE_VOID:
        pm_flat(&m, PAL_PAINT(PAINT_JET, 0));
        pc_decal(&m, SH_FLAT);
        pc_ellipse_sh(ex, ey, erx + 4, ery + 6, shear);
        pc_px((ex - erx / 3) >> 4, (ey - ery / 3) >> 4, PAL_WHITE);
        break;
    case EYE_LASHES:
        eye_glossy(c, ex, ey, erx, ery, shear, big, 52);
        pc_px(cx + side * ((erx >> 4) + 1), ((ey - ery / 2) >> 4), ink);
        pc_px(cx + side * ((erx >> 4) + 2), ((ey - ery / 2) >> 4) - 1, ink);
        pc_px(cx + side * ((erx >> 4)), ((ey - ery) >> 4) - 1, ink);
        pc_px(cx + side * ((erx >> 4) + 1), ((ey - ery) >> 4) - 2, ink);
        pc_px(cx, ((ey - ery) >> 4) - 1, ink);
        break;
    case EYE_DROOPY:
        eye_glossy(c, ex, ey, erx, ery, shear + side * 50, big, 52);
        pc_line(cx - side * (erx >> 4), ((ey - ery) >> 4) + 0, cx + side * ((erx >> 4) + 1), ((ey - ery / 3) >> 4) + 1, ink);
        break;
    case EYE_WIDE:
        pc_decal(&w, SH_FLAT);
        pc_ellipse_sh(ex, ey, erx + 8, ery + 6, shear);
        pc_decal(&c->eye, SH_FLAT);
        pc_ellipse(ex, ey + 4, erx * 38 / 100 + 6, erx * 38 / 100 + 6);
        pc_px(cx, (ey + 4) >> 4, PAL_PAINT(PAINT_JET, 1));
        lid_arc(ex, ey, erx + 8, ery + 6, shear, ink, 1);
        break;
    case EYE_SQUINT: {
        int wdt = DOOMO_MAX(erx >> 4, 1);
        pc_line(cx - wdt, cy, cx + wdt, cy, ink);
        if (big) pc_line(cx - wdt + 1, cy + 1, cx + wdt - 1, cy + 1, ink);
        pc_px(cx - side * (wdt + 1), cy - 1, ink);
        break; }
    case EYE_SPARKLE:
        eye_glossy(c, ex, ey, erx, ery, shear, big, 40);
        ph_star((ex - erx / 3) >> 4, (ey - ery / 3) >> 4, 1, PAL_WHITE);
        pc_px((ex + erx / 3) >> 4, (ey + ery / 4) >> 4, PAL_WHITE);
        pc_px((ex - erx / 4) >> 4, (ey + ery / 2) >> 4, c->eye.c[4]);
        break;
    case EYE_DEMON_SLIT:
        pm_paint(&m, c->l->eye_color);
        m.c[1] = m.c[2]; m.c[2] = m.c[3]; m.c[3] = m.c[4];
        pc_decal(&m, SH_VGRAD);
        pc_ellipse_sh(ex, ey + 4, erx + 6, ery * 80 / 100, shear - side * 60);
        pm_flat(&m, PAL_PAINT(PAINT_JET, 1));
        pc_decal(&m, SH_FLAT);
        pc_ellipse(ex, ey + 4, 10, ery * 70 / 100);
        lid_arc(ex, ey + 4, erx + 6, ery * 80 / 100, shear - side * 60, PAL_PAINT(PAINT_BLOOD, 0), big ? 2 : 1);
        break;
    case EYE_CROSS: {
        int r = DOOMO_MAX(erx >> 4, 2);
        pc_decal(&w, SH_FLAT);
        pc_ellipse_sh(ex, ey, erx + 4, ery, shear);
        pc_line(cx - r + 1, cy, cx + r - 1, cy, c->eye.c[1]);
        pc_line(cx, cy - r + 1, cx, cy + r - 1, c->eye.c[1]);
        pc_px(cx, cy, PAL_PAINT(PAINT_JET, 1));
        lid_arc(ex, ey, erx + 4, ery, shear, ink, 1);
        break; }
    case EYE_HOLLOW:
        pm_flat(&m, PAL_PAINT(PAINT_CHARCOAL, 0));
        pc_decal(&m, SH_FLAT);
        pc_ellipse_sh(ex, ey, erx + 6, ery + 4, shear);
        pc_px(cx, cy, c->eye.c[4]);
        if (big) { pc_px(cx, cy + 1, c->eye.c[2]); pc_px(cx - 1, cy, c->eye.c[3]); }
        break;
    case EYE_GLOW:
        pm_paint(&m, c->l->eye_color);
        m.c[1] = m.c[3]; m.c[2] = m.c[4]; m.c[3] = PAL_WHITE;
        pc_decal(&m, SH_VGRAD);
        pc_ellipse_sh(ex, ey, erx, ery, shear);
        lid_arc(ex, ey, erx, ery, shear, c->eye.c[1], 1);
        break;
    case EYE_COMPOUND: {
        int x, y;
        pc_decal(&c->eye, SH_BALL);
        pc_ellipse(ex, ey, erx + 10, ery + 4);
        for (y = (ey - ery) >> 4; y <= (ey + ery) >> 4; y++)
            for (x = (ex - erx) >> 4; x <= (ex + erx) >> 4; x++)
                if (((x + (y & 1)) % 2 == 0) && (y & 1) == 0 &&
                    pc_get(x, y) != 0 && pc_get(x, y) != c->skin.c[2])
                    pc_pxs(x, y, c->eye.c[0]);
        pc_px((ex - erx / 2) >> 4, (ey - ery / 2) >> 4, PAL_WHITE);
        break; }
    }
}

static void eyes(PCtx *c)
{
    const DoomoLook *l = c->l;
    int st = l->eye_style % EYE_COUNT;
    int shear = (l->eye_tilt - 8) * 14;
    int X, Y, v, n = DOOMO_CLAMP(l->eye_count, 1, 3);
    int erx = F.erx, ery = F.ery;
    if (c->expr == EXPR_SICK || c->expr == EXPR_SAD) shear += (c->expr == EXPR_SAD ? 30 : 0);
    if (n == 1) {
        v = fpt(0, F.ey, &X, &Y);
        if (v > 0) { phs.s = 1; one_eye(c, st, X, Y, fshort(erx, v), ery, 0, 1); }
    } else {
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            v = fpt(phs.s * F.ex, F.ey, &X, &Y);
            if (v > 40) one_eye(c, st, X, Y, fshort(erx, v), ery, -phs.s * shear, phs.s);
        }
        phs.s = 1;
    }
    if (n == 3) {
        v = fpt(0, F.ey - (ery * 256 / phs.h->ry) - 90, &X, &Y);
        if (v > 0) one_eye(c, st, X, Y, fshort(erx * 3 / 4, v), ery * 3 / 4, 0, 1);
    }
}

/* ---- brows --------------------------------------------------------------------------- */
static void brows(PCtx *c)
{
    const DoomoLook *l = c->l;
    int st = l->brow_style, big = phs.h->lod == LOD_BIG;
    uint8_t col = c->hair.c[0], col2 = c->hair.c[1];
    int X, Y, v, w, k;
    if (st == BROW_NONE || phs.h->lod == LOD_TINY) {
        if (phs.h->lod != LOD_TINY || c->expr != EXPR_ANGRY) return;
    }
    if (l->hair_style == HAIR_BALD && c->critter) { col = c->skin.c[0]; col2 = c->skin.c[1]; }
    if (c->creepy) { col = PAL_PAINT(PAINT_JET, 1); col2 = col; }
    if (c->expr == EXPR_ANGRY) st = st == BROW_NONE ? BROW_ANGRY : (st == BROW_UNIBROW ? st : BROW_ANGRY);
    else if (c->expr == EXPR_SAD || c->expr == EXPR_SICK) st = st == BROW_NONE ? BROW_NONE : BROW_WORRIED;
    else if (c->expr == EXPR_SHOCK) st = st == BROW_NONE ? BROW_NONE : BROW_RAISED;
    if (st == BROW_NONE) return;
    w = DOOMO_MAX(F.erx >> 4, 2) + (big ? 1 : 0);
    if (st == BROW_UNIBROW) {
        int Xl, Xr, Yl;
        fpt(-F.ex, F.by, &Xl, &Yl);
        fpt(F.ex, F.by, &Xr, &Y);
        pc_line((Xl >> 4) - w, Yl >> 4, (Xr >> 4) + w, Yl >> 4, col);
        if (big) pc_line((Xl >> 4) - w + 1, (Yl >> 4) + 1, (Xr >> 4) + w - 1, (Yl >> 4) + 1, col2);
        return;
    }
    for (phs.s = -1; phs.s <= 1; phs.s += 2) {
        int s = phs.s, cx, cy, in, out;
        v = fpt(s * F.ex, F.by, &X, &Y);
        if (v <= 40) continue;
        cx = X >> 4; cy = Y >> 4;
        in = cx - s * w; out = cx + s * w;
        switch (st) {
        case BROW_THIN:
            pc_line(in, cy, out, cy - 1, col);
            break;
        case BROW_THICK:
            pc_line(in, cy, out, cy - 1, col);
            pc_line(in, cy + 1, out, cy, col);
            pc_line(in + s, cy - 1, out - s, cy - 2, col2);
            break;
        case BROW_ANGRY:
            pc_line(in, cy + 1, out, cy - 2, col);
            if (big) pc_line(in, cy + 2, out, cy - 1, col);
            break;
        case BROW_WORRIED:
            pc_line(in, cy - 2, out, cy + 1, col);
            if (big) pc_line(in + s, cy - 1, out, cy + 2, col2);
            break;
        case BROW_ARCHED:
            pc_line(in, cy, cx, cy - 2, col);
            pc_line(cx, cy - 2, out, cy, col);
            break;
        case BROW_BUSHY:
            for (k = -w; k <= w; k++) {
                pc_px(cx + k, cy, col); pc_px(cx + k, cy - 1, col);
                if ((k & 1) == 0) pc_px(cx + k, cy - 2, col2);
                if (big) pc_px(cx + k, cy + 1, (k & 1) ? col : col2);
            }
            break;
        case BROW_DOTS:
            pc_px(cx - s, cy, col); pc_px(cx, cy, col);
            pc_px(cx - s, cy - 1, col2); pc_px(cx, cy - 1, col);
            break;
        case BROW_SCARRED:
            pc_line(in, cy, out, cy - 1, col);
            pc_line(in, cy + 1, out, cy, col);
            pc_px(cx, cy, c->skin.c[3]); pc_px(cx, cy + 1, c->skin.c[3]);
            pc_px(cx + s, cy - 1, c->skin.c[3]);
            break;
        case BROW_FLAT:
            pc_line(in, cy, out, cy, col);
            if (big) pc_line(in, cy + 1, out, cy + 1, col2);
            break;
        case BROW_RAISED:
            pc_line(in, cy - 1, cx, cy - 3, col);
            pc_line(cx, cy - 3, out, cy - 2, col);
            break;
        default:
            break;
        }
    }
    phs.s = 1;
}

/* ---- nose ------------------------------------------------------------------------------ */
static void nose(PCtx *c)
{
    const DoomoLook *l = c->l;
    int st = l->nose_style, big = phs.h->lod == LOD_BIG;
    int X, Y, v, sz = 4 + l->nose_size, r, cx, cy;
    PMat m;
    uint8_t ink = face_ink(c);
    int ny = F.ny;
    if (F.muzzle != MZ_NONE) ny = F.my - 70;
    v = fpt(0, ny, &X, &Y);
    if (v <= 0 || st == NOSE_NONE) return;
    cx = X >> 4; cy = Y >> 4;
    r = hr(sz * 5);          /* Q4 radius */
    if (!big) r = r * 3 / 4;
    if (phs.h->lod == LOD_TINY) {
        if (st == NOSE_CLOWN || st == NOSE_PIG || st == NOSE_LONG || st == NOSE_CAT || st == NOSE_BUTTON)
            pc_px(cx, cy, st == NOSE_CLOWN ? PAL_PAINT(PAINT_CHERRY, 1) :
                          (st == NOSE_PIG ? NOSEPINK : (st == NOSE_CAT ? NOSEPINK : c->skin.c[0])));
        return;
    }
    switch (st) {
    case NOSE_DOT:
        pc_px(cx, cy, c->skin.c[0]);
        if (sz > 12) pc_px(cx + 1, cy, c->skin.c[0]);
        break;
    case NOSE_BUTTON:
        pm_from3(&m, c->critter ? PAL_PAINT(PAINT_JET, 0) : c->skin.c[0],
                 c->critter ? PAL_PAINT(PAINT_JET, 1) : c->skin.c[1],
                 c->critter ? PAL_PAINT(PAINT_JET, 2) : c->skin.c[2]);
        pc_decal(&m, SH_GLOSS);
        pc_ellipse(X, Y, r + 8, r * 3 / 4 + 6);
        pc_px((X - r / 3) >> 4, (Y - r / 3) >> 4, c->critter ? PAL_GRAY : c->skin.c[4]);
        break;
    case NOSE_POINTY:
        pc_line(cx, cy - (sz >> 2), cx + 1 + (sz >> 3), cy + 1, c->skin.c[0]);
        pc_px(cx, cy + 1, c->skin.c[0]);
        break;
    case NOSE_ROUND:
        pm_from3(&m, c->skin.c[1], c->skin.c[2], c->skin.c[4]);
        pc_part(&m, SH_BALL, 0);
        pc_ellipse(X, Y, r + 12, r + 4);
        break;
    case NOSE_PIG:
        pm_ramp(&m, RAMP_PASTEL_PINK, 4);
        pc_part(&m, SH_BALL, 0);
        pc_ellipse(X, Y, r + 16, r * 3 / 4 + 8);
        pc_px(cx - 1, cy, m.c[0]); pc_px(cx + 1, cy, m.c[0]);
        break;
    case NOSE_SKULL:
        pm_flat(&m, PAL_PAINT(PAINT_JET, 1));
        pc_decal(&m, SH_FLAT);
        pc_tri(X - r - 8, Y - r, X + r + 8, Y - r, X, Y + r + 4);
        break;
    case NOSE_HOOK:
        pc_line(cx, cy - (sz >> 2) - 1, cx + 1 + (sz >> 3), cy, c->skin.c[0]);
        pc_line(cx + 1 + (sz >> 3), cy, cx, cy + 1, c->skin.c[0]);
        pc_px(cx - 1, cy + 1, c->skin.c[1]);
        break;
    case NOSE_WIDE:
        pm_from3(&m, c->skin.c[1], c->skin.c[2], c->skin.c[3]);
        pc_part(&m, SH_BALL, 0);
        pc_ellipse(X, Y, r * 2 + 10, r * 2 / 3 + 6);
        pc_px(cx - (r >> 4), cy + 1, c->skin.c[0]); pc_px(cx + (r >> 4), cy + 1, c->skin.c[0]);
        break;
    case NOSE_LONG: {
        pm_from3(&m, c->skin.c[1], c->skin.c[2], c->skin.c[3]);
        pc_part(&m, SH_BALL, 0);
        pc_capsule(X, Y - 8, X + r * 3 + 24, Y + 8, r / 2 + 8, r / 2 + 6);
        break; }
    case NOSE_CAT:
        pm_ramp(&m, RAMP_PASTEL_PINK, 4);
        pc_decal(&m, SH_FLAT);
        pc_tri(X - r - 10, Y - r / 2 - 6, X + r + 10, Y - r / 2 - 6, X, Y + r / 2 + 6);
        pc_px(cx, cy + (r >> 4) + 1, ink);
        pc_px(cx - 1, (Y - r / 2) >> 4, m.c[4]);
        break;
    case NOSE_CLOWN:
        pm_paint(&m, PAINT_CHERRY);
        pc_part(&m, SH_GLOSS, 0);
        pc_ellipse(X, Y, r + 18, r + 18);
        break;
    default:
        break;
    }
}

/* ---- mouth ----------------------------------------------------------------------------- */
static void curve(int cx, int cy, int w, int depth, uint8_t col, int thick)
{
    int x, py = 0;
    for (x = -w; x <= w; x++) {
        int y = cy + (depth * (w * w - x * x)) / (w * w > 0 ? w * w : 1);
        if (x > -w && p_abs(y - py) > 1) {
            int k, s = y > py ? 1 : -1;
            for (k = py + s; k != y; k += s) pc_px(cx + x - (s * (y - py) > 0 ? 0 : 0), k, col);
        }
        pc_px(cx + x, y, col);
        if (thick) pc_px(cx + x, y + 1, col);
        py = y;
    }
}

static void fangs(PCtx *c, int cx, int cy, int w, int n)
{
    int big = phs.h->lod == LOD_BIG;
    uint8_t f = PAL_WHITE, fs = PAL_GRAY_L;
    if (n <= 0) return;
    if (n == 1 || n >= 2) {
        pc_px(cx + w / 2 + 1, cy + 1, f);
        if (n >= 3 && big) { pc_px(cx + w / 2 + 1, cy + 2, fs); pc_px(cx + w / 2 + 2, cy + 1, f); }
    }
    if (n >= 2) {
        pc_px(cx - w / 2 - 1, cy + 1, f);
        if (n >= 3 && big) { pc_px(cx - w / 2 - 1, cy + 2, fs); pc_px(cx - w / 2 - 2, cy + 1, f); }
    }
    (void)c;
}

static void mouth(PCtx *c)
{
    const DoomoLook *l = c->l;
    int st = l->mouth_style, big = phs.h->lod == LOD_BIG;
    int X, Y, v, w, cx, cy, k;
    uint8_t ink = face_ink(c);
    PMat m;
    v = fpt(0, F.my, &X, &Y);
    if (v <= 0) return;
    w = DOOMO_MAX(fshort(F.mw, v) >> 4, 1);
    cx = X >> 4; cy = Y >> 4;
    /* expression overrides */
    if (c->creepy) {
        st = c->creepy > 1 ? MOUTH_VOID : MOUTH_SHARK;
        w += 1;
    } else switch (c->expr) {
    case EXPR_HAPPY: st = (st == MOUTH_BEAK || st == MOUTH_CAT) ? st : MOUTH_OPEN; break;
    case EXPR_SAD: st = st == MOUTH_BEAK ? st : MOUTH_FROWN; break;
    case EXPR_ANGRY: st = st == MOUTH_BEAK ? st : (l->fangs ? MOUTH_FANGY : MOUTH_TEETH); break;
    case EXPR_LOVE: st = st == MOUTH_BEAK ? st : MOUTH_CAT; break;
    case EXPR_SHOCK: st = st == MOUTH_BEAK ? st : MOUTH_O; break;
    case EXPR_SLEEPY: st = MOUTH_DROOL; break;
    case EXPR_SICK: st = MOUTH_WAVY; break;
    case EXPR_DEAD: st = MOUTH_TONGUE; break;
    case EXPR_TALK: if (c->anim & 4) st = st == MOUTH_BEAK ? st : MOUTH_O; break;
    default: break;
    }
    if (phs.h->lod == LOD_TINY) {
        switch (st) {
        case MOUTH_OPEN: case MOUTH_O: case MOUTH_BIG_GRIN: case MOUTH_VOID:
            pc_px(cx, cy, MOUTHIN); break;
        case MOUTH_BEAK: pc_px(cx, cy, PAL_PAINT(PAINT_HONEY, 1)); break;
        case MOUTH_FROWN: pc_px(cx, cy, ink); pc_px(cx - 1, cy + 1, ink); pc_px(cx + 1, cy + 1, ink); break;
        default: if (w >= 2) { pc_px(cx - 1, cy, ink); pc_px(cx + 1, cy, ink); } else pc_px(cx, cy, ink); break;
        }
        return;
    }
    switch (st) {
    case MOUTH_SMILE:
        curve(cx, cy - 1, w, 2, ink, 0);
        break;
    case MOUTH_GRIN:
        pm_flat(&m, TEETH);
        pc_decal(&m, SH_FLAT);
        for (k = -w; k <= w; k++) {
            int d = (2 * (w * w - k * k)) / (w * w + 1);
            pc_px(cx + k, cy - 1 + d, ink);
            if (p_abs(k) < w) { int y; for (y = cy - 1; y < cy - 1 + d; y++) pc_px(cx + k, y, TEETH); }
        }
        pc_line(cx - w, cy - 1, cx + w, cy - 1, ink);
        break;
    case MOUTH_FLAT:
        pc_line(cx - w, cy, cx + w, cy, ink);
        break;
    case MOUTH_FROWN:
        curve(cx, cy + 1, w, -2, ink, 0);
        break;
    case MOUTH_OPEN: {
        int hgt = DOOMO_MAX(w * 2 / 3, 2);
        pm_flat(&m, MOUTHIN);
        pc_decal(&m, SH_FLAT);
        pc_ellipse(X, Y + PX(hgt) / 2 - 8, PX(w), PX(hgt) / 2 + 8);
        pc_clip(0, 0, pcv.w, cy);
        pc_ellipse(X, Y - 8, PX(w) + 8, PX(hgt) + 8);
        pc_noclip();
        pc_line(cx - w, cy - 1, cx + w, cy - 1, ink);
        pm_flat(&m, TONGUE);
        pc_decal(&m, SH_FLAT);
        pc_ellipse(X, Y + PX(hgt) - 12, PX(w) * 2 / 3, PX(1));
        break; }
    case MOUTH_CAT:
        /* the little "w" */
        pc_px(cx, cy - 1, ink);
        pc_px(cx - 1, cy, ink); pc_px(cx + 1, cy, ink);
        for (k = 2; k <= w; k++) {
            pc_px(cx - k, cy - (k == w ? 1 : 0), ink);
            pc_px(cx + k, cy - (k == w ? 1 : 0), ink);
        }
        break;
    case MOUTH_TEETH:
        pm_flat(&m, TEETH);
        pc_decal(&m, SH_FLAT);
        pc_rect(PX(cx - w), PX(cy - 1), PX(cx + w + 1), PX(cy + 2));
        pc_line(cx - w, cy - 2, cx + w, cy - 2, ink);
        pc_line(cx - w, cy + 2, cx + w, cy + 2, ink);
        pc_px(cx - w - 1, cy, ink); pc_px(cx + w + 1, cy, ink);
        for (k = -w + 2; k < w; k += 2) pc_px(cx + k, cy, PAL_GRAY_L);
        pc_line(cx - w, cy, cx + w, cy, PAL_GRAY_L);
        break;
    case MOUTH_SHARK:
        pm_flat(&m, MOUTHIN);
        pc_decal(&m, SH_FLAT);
        pc_ellipse(X, Y + 8, PX(w) + 8, PX(2));
        for (k = -w; k <= w; k++) {
            pc_px(cx + k, cy - 1 + ((k + w) & 1), TEETH);
            pc_px(cx + k, cy + 2 - ((k + w) & 1), TEETH);
        }
        pc_line(cx - w - 1, cy - 1, cx + w + 1, cy - 1, ink);
        break;
    case MOUTH_TONGUE:
        curve(cx, cy - 1, w, 1, ink, 0);
        pm_flat(&m, TONGUE);
        pc_decal(&m, SH_FLAT);
        pc_ellipse(X + PX(1), Y + PX(2), PX(1) + 10, PX(2));
        pc_px(cx + 1, cy + 1, PAL_RAMP(RAMP_PASTEL_PINK, 3));
        pc_px(cx + 1, cy + 2, PAL_RAMP(RAMP_PASTEL_PINK, 3));
        break;
    case MOUTH_SMIRK:
        pc_line(cx - w, cy, cx + w - 1, cy, ink);
        pc_px(cx + w, cy - 1, ink);
        pc_px(cx + w + 1, cy - 2, ink);
        break;
    case MOUTH_POUT:
        pm_ramp(&m, RAMP_PASTEL_PINK, 4);
        pc_decal(&m, SH_BALL);
        pc_ellipse(X, Y, PX(1) + 14, PX(1) + 4);
        pc_px(cx, cy, MOUTHIN);
        break;
    case MOUTH_O:
        pm_flat(&m, ink);
        pc_decal(&m, SH_FLAT);
        pc_ellipse(X, Y + 8, PX(w / 2 + 1) + 6, PX(w / 2 + 1) + 14);
        pm_flat(&m, MOUTHIN);
        pc_decal(&m, SH_FLAT);
        pc_ellipse(X, Y + 8, PX(w / 2 + 1) - 8, PX(w / 2 + 1));
        break;
    case MOUTH_WAVY:
        for (k = -w; k <= w; k++) pc_px(cx + k, cy + (((k + w) >> 1) & 1), ink);
        break;
    case MOUTH_STITCHED:
        pc_line(cx - w, cy, cx + w, cy, ink);
        for (k = -w + 1; k < w; k += 2) { pc_px(cx + k, cy - 1, ink); pc_px(cx + k, cy + 1, ink); }
        break;
    case MOUTH_BEAK:
        pm_paint(&m, PAINT_HONEY);
        pc_part(&m, SH_BALL, 0);
        pc_tri(X - PX(w) - 8, Y - PX(2), X + PX(w) + 8, Y - PX(2), X, Y + PX(w) + 8);
        pc_line(cx - w + 1, cy - 1, cx + w - 1, cy - 1, m.c[0]);
        break;
    case MOUTH_FANGY:
        curve(cx, cy - 1, w, 2, ink, 0);
        pc_px(cx - w / 2, cy + 1, TEETH); pc_px(cx + w / 2, cy + 1, TEETH);
        pc_px(cx - w / 2, cy + 2, TEETH); pc_px(cx + w / 2, cy + 2, TEETH);
        break;
    case MOUTH_DROOL:
        curve(cx, cy - 1, w, 1, ink, 0);
        pc_px(cx + w - 1, cy + 1, PAL_PAINT(PAINT_CYAN, 2));
        pc_px(cx + w - 1, cy + 2, PAL_PAINT(PAINT_CYAN, 2));
        pc_px(cx + w - 1, cy + 3, PAL_PAINT(PAINT_SKY, 1));
        break;
    case MOUTH_LIPSTICK:
        pm_paint(&m, PAINT_CHERRY);
        pc_decal(&m, SH_BALL);
        pc_ellipse(X - PX(w) / 3, Y - 8, PX(w) / 2 + 8, PX(1) + 2);
        pc_ellipse(X + PX(w) / 3, Y - 8, PX(w) / 2 + 8, PX(1) + 2);
        pc_ellipse(X, Y + 12, PX(w) * 3 / 4 + 4, PX(1) + 4);
        pc_line(cx - w + 1, cy, cx + w - 1, cy, m.c[0]);
        break;
    case MOUTH_BIG_GRIN: {
        int hgt = DOOMO_MAX(w * 3 / 4, 2) + 1;
        pm_flat(&m, MOUTHIN);
        pc_decal(&m, SH_FLAT);
        pc_clip(0, cy - 1, pcv.w, pcv.h);
        pc_ellipse(X, Y - 16, PX(w + 1), PX(hgt));
        pc_noclip();
        pc_line(cx - w, cy - 1, cx + w, cy - 1, TEETH);
        pc_line(cx - w + 1, cy, cx + w - 1, cy, TEETH);
        pm_flat(&m, TONGUE);
        pc_decal(&m, SH_FLAT);
        pc_ellipse(X, Y + PX(hgt) - 28, PX(w) / 2, PX(1));
        break; }
    case MOUTH_VOID: {
        pm_flat(&m, PAL_PAINT(PAINT_JET, 0));
        pc_decal(&m, SH_FLAT);
        pc_ellipse(X, Y + PX(1), PX(w) * 3 / 4 + 8, PX(w) + 8);
        if (c->creepy) {
            pc_px(cx - 1, cy + w + 1, PAL_PAINT(PAINT_JET, 0));
            pc_px(cx + 1, cy + w + 2, PAL_PAINT(PAINT_JET, 0));
        }
        break; }
    default:
        curve(cx, cy - 1, w, 2, ink, 0);
        break;
    }
    if (l->fangs && st != MOUTH_BEAK && st != MOUTH_FANGY && st != MOUTH_VOID)
        fangs(c, cx, cy, w * 2, l->fangs);
    if (st == MOUTH_CAT && big && (c->l->species == SPECIES_BUNNYKIN || c->l->species == SPECIES_MOUSEKIN)) {
        pc_px(cx, cy + 1, TEETH); pc_px(cx + 1, cy + 1, TEETH);
    }
}

/* ---- cheeks, markings ------------------------------------------------------------------- */
static void blush(PCtx *c, int big)
{
    int X, Y, v;
    const DoomoLook *l = c->l;
    int ch = l->cheeks;
    int force = (l->species == SPECIES_CHERUB) || c->expr == EXPR_LOVE || c->expr == EXPR_HAPPY;
    int cy = F.ey + (F.ery * 256 / phs.h->ry) + 26;
    if (c->creepy) return;
    for (phs.s = -1; phs.s <= 1; phs.s += 2) {
        int bx = F.ex + 46;
        v = fpt(phs.s * bx, cy, &X, &Y);
        if (v <= 30) continue;
        if ((ch & CHEEK_BLUSH) || force) {
            PMat m;
            int rx = hr(l->species == SPECIES_CHERUB ? 66 : 52), ry = hry(26);
            if (!big) { pc_px(X >> 4, Y >> 4, BLUSH); if (phs.h->lod == LOD_MID) pc_px((X >> 4) + phs.s, Y >> 4, BLUSH); continue; }
            pm_flat(&m, BLUSH);
            pc_decal(&m, SH_FLAT);
            pc_ellipse(X, Y, fshort(rx, v), ry + 6);
            if (c->expr == EXPR_LOVE || l->species == SPECIES_CHERUB) {
                pm_flat(&m, BLUSH_D);
                pc_decal(&m, SH_FLAT);
                pc_ellipse(X, Y + 4, fshort(rx, v) * 2 / 3, ry / 2 + 4);
            }
            pc_px((X - rx / 2) >> 4, (Y - ry / 2) >> 4, PAL_WHITE);
        }
        if (!big) continue;
        if (ch & CHEEK_FRECKLES) {
            pc_px((X >> 4) - 1, (Y >> 4) - 1, c->skin.c[0]);
            pc_px((X >> 4) + 1, (Y >> 4), c->skin.c[0]);
            pc_px((X >> 4) - 2, (Y >> 4) + 1, c->skin.c[0]);
        }
        if ((ch & CHEEK_SCAR) && phs.s > 0) {
            pc_line((X >> 4) - 2, (Y >> 4) - 3, (X >> 4) + 2, (Y >> 4) + 2, PAL_RAMP(RAMP_FLESH, 3));
            pc_px((X >> 4) - 1, (Y >> 4) - 1, PAL_RAMP(RAMP_FLESH, 5));
            pc_px((X >> 4) + 1, (Y >> 4) + 0, PAL_RAMP(RAMP_FLESH, 5));
        }
        if ((ch & CHEEK_BANDAGE) && phs.s < 0) {
            PMat b;
            pm_paint(&b, PAINT_PEACH);
            pc_part(&b, SH_SOFT, PF_NOCAST);
            pc_rrect(X, Y, PX(3), PX(1) + 10, 10);
            pc_px((X >> 4) - 1, Y >> 4, b.c[1]); pc_px((X >> 4) + 1, Y >> 4, b.c[1]);
        }
        if ((ch & CHEEK_BEAUTY_MARK) && phs.s > 0)
            pc_px((X >> 4) + 1, (Y >> 4) + 3, PAL_PAINT(PAINT_JET, 2));
        if ((ch & CHEEK_TEAR_TATTOO) && phs.s < 0) {
            int tx = (X >> 4) + 2, ty = (Y >> 4) - 2;
            pc_px(tx, ty, PAL_PAINT(PAINT_NAVY, 1));
            pc_px(tx, ty + 1, PAL_PAINT(PAINT_NAVY, 1));
            pc_px(tx - 1, ty + 1, PAL_PAINT(PAINT_NAVY, 1));
            pc_px(tx + 1, ty + 1, PAL_PAINT(PAINT_NAVY, 1));
            pc_px(tx, ty + 2, PAL_PAINT(PAINT_NAVY, 1));
        }
        if (ch & CHEEK_DIMPLES) {
            int mx, my;
            fpt(phs.s * (F.mw * 256 / phs.h->rx + 40), F.my - 10, &mx, &my);
            pc_px(mx >> 4, my >> 4, c->skin.c[1]);
            pc_px((mx >> 4) + phs.s, (my >> 4) + 1, c->skin.c[0]);
        }
        if (ch & CHEEK_SPARKLES) {
            int sx = (X >> 4) + phs.s * 3, sy = (Y >> 4) - 4;
            uint8_t sc = (c->anim & 8) ? PAL_WHITE : PAL_UI_GOLD;
            pc_px(sx, sy, sc); pc_px(sx - 1, sy, sc); pc_px(sx + 1, sy, sc);
            pc_px(sx, sy - 1, sc); pc_px(sx, sy + 1, sc);
        }
    }
    phs.s = 1;
}

static void whiskers(PCtx *c, int n, int len)
{
    int X, Y, v, k;
    uint8_t col = c->skin.c[0];
    if (palette_luma(col) > 170) col = palette_darker(col);
    for (phs.s = -1; phs.s <= 1; phs.s += 2) {
        v = fpt(phs.s * 130, F.my - 40, &X, &Y);
        if (v <= 30) continue;
        for (k = 0; k < n; k++) {
            int x0 = X >> 4, y0 = (Y >> 4) + k - 1;
            pc_line(x0, y0, x0 + phs.s * len, y0 - 1 + k, col);
        }
    }
    phs.s = 1;
}

/* face markings; most are decals masked to the head */
static void markings(PCtx *c, int big)
{
    const DoomoLook *l = c->l;
    PMat m;
    int X, Y, v, k;
    int mk = l->marking;
    if (mk == MARK_NONE || phs.h->yaw >= 3) return;
    pm_paint(&m, l->marking_color);
    pc_mask_none();
    pc_mask_add(c->head_id);
    switch (mk) {
    case MARK_STRIPES:
        pc_decal(&m, SH_FLAT);
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            hT(256, -60, 256, 0, 150, -24);
            hT(256, 40, 256, 100, 160, 74);
            if (big) hT(256, 140, 256, 180, 186, 160);
        }
        phs.s = 1;
        hT(-30, -256, 30, -256, 0, -170);
        break;
    case MARK_SPOTS:
        pc_decal(&m, SH_FLAT);
        hE(-170, -120, 36, 34); hE(150, -170, 28, 26); hE(190, 60, 30, 30);
        hE(-200, 90, 24, 24); hE(40, -220, 22, 20);
        break;
    case MARK_TRIBAL:
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            fpt(phs.s * (F.ex + 40), F.ey + 90, &X, &Y);
            pc_line(X >> 4, Y >> 4, (X >> 4) + phs.s * 3, (Y >> 4) + 3, m.c[1]);
            pc_line((X >> 4) - phs.s * 2, (Y >> 4) - 1, (X >> 4) - phs.s * 1, (Y >> 4) + 3, m.c[1]);
            pc_px((X >> 4) + phs.s * 3, (Y >> 4) + 3, m.c[2]);
        }
        phs.s = 1;
        hL(-20, -230, 20, -190, m.c[1]);
        hL(20, -230, -20, -190, m.c[1]);
        break;
    case MARK_SKULL_PAINT: {
        PMat w, d;
        pm_paint(&w, PAINT_WHITE);
        pm_flat(&d, PAL_PAINT(PAINT_JET, 1));
        pc_decal(&w, SH_FLAT);
        hE(0, 30, 236, 220);
        pc_decal(&d, SH_FLAT);
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            fpt(phs.s * F.ex, F.ey, &X, &Y);
            pc_ellipse(X, Y, F.erx + 14, F.ery + 14);
        }
        phs.s = 1;
        fpt(0, F.my + 24, &X, &Y);
        for (k = -3; k <= 3; k++) pc_px((X >> 4) + k, Y >> 4, PAL_PAINT(PAINT_JET, 1));
        for (k = -3; k <= 3; k += 2) pc_px((X >> 4) + k, (Y >> 4) - 1, PAL_PAINT(PAINT_JET, 1));
        break; }
    case MARK_CHEEK_HEARTS:
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            fpt(phs.s * (F.ex + 60), F.ey + 110, &X, &Y);
            ph_heart(X >> 4, Y >> 4, big ? 2 : 1, m.c[2], m.c[4]);
        }
        phs.s = 1;
        break;
    case MARK_STITCHES:
        fpt(80, -60, &X, &Y);
        pc_line((X >> 4) - 4, (Y >> 4) + 6, (X >> 4) + 4, (Y >> 4) - 2, m.c[0]);
        for (k = -3; k <= 3; k += 2) {
            int sx = (X >> 4) + k, sy = (Y >> 4) + 2 - k;
            pc_px(sx - 1, sy - 1, m.c[0]); pc_px(sx + 1, sy + 1, m.c[0]);
        }
        break;
    case MARK_CRACKS:
        fpt(-90, -200, &X, &Y);
        pc_line(X >> 4, Y >> 4, (X >> 4) + 2, (Y >> 4) + 4, m.c[0]);
        pc_line((X >> 4) + 2, (Y >> 4) + 4, (X >> 4) + 1, (Y >> 4) + 8, m.c[0]);
        pc_line((X >> 4) + 2, (Y >> 4) + 4, (X >> 4) + 5, (Y >> 4) + 6, m.c[0]);
        fpt(170, 120, &X, &Y);
        pc_line(X >> 4, Y >> 4, (X >> 4) - 2, (Y >> 4) + 3, m.c[0]);
        pc_line((X >> 4) - 2, (Y >> 4) + 3, (X >> 4) - 1, (Y >> 4) + 5, m.c[0]);
        break;
    case MARK_STAR_FRECKLES:
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            fpt(phs.s * (F.ex + 30), F.ey + 100, &X, &Y);
            ph_star(X >> 4, Y >> 4, 1, m.c[3]);
            pc_px((X >> 4) + phs.s * 3, (Y >> 4) + 2, m.c[3]);
        }
        phs.s = 1;
        break;
    case MARK_CLOWN: {
        PMat w;
        pm_paint(&w, PAINT_WHITE);
        pc_decal(&w, SH_SOFT);
        hE(0, 40, 200, 190);
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            fpt(phs.s * F.ex, F.ey, &X, &Y);
            pc_decal(&m, SH_FLAT);
            pc_tri(X, Y - F.ery - 40, X - 28, Y, X + 28, Y);
            pc_tri(X, Y + F.ery + 40, X - 28, Y, X + 28, Y);
        }
        phs.s = 1;
        fpt(0, F.my, &X, &Y);
        pc_decal(&m, SH_FLAT);
        pc_ellipse(X, Y, F.mw + 24, 28);
        break; }
    case MARK_EYELINER:
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            fpt(phs.s * F.ex, F.ey, &X, &Y);
            pc_line((X + phs.s * F.erx) >> 4, (Y - F.ery / 3) >> 4,
                    ((X + phs.s * F.erx) >> 4) + phs.s * 3, ((Y - F.ery) >> 4) - 1, m.c[0]);
            pc_line((X + phs.s * F.erx) >> 4, (Y - F.ery / 3 + 16) >> 4,
                    ((X + phs.s * F.erx) >> 4) + phs.s * 2, ((Y - F.ery) >> 4), m.c[0]);
        }
        phs.s = 1;
        break;
    case MARK_THIRD_EYE: {
        int ey = F.ey - (F.ery * 256 / phs.h->ry) - 100;
        PMat w;
        v = fpt(0, ey, &X, &Y);
        if (v <= 0) break;
        pm_flat(&w, PAL_WHITE);
        pc_decal(&w, SH_FLAT);
        pc_ellipse_sh(X, Y, F.erx * 2 / 3 + 10, F.ery / 2 + 6, 0);
        pc_decal(&m, SH_FLAT);
        pc_ellipse(X, Y, F.erx / 3 + 8, F.ery / 3 + 6);
        pc_px(X >> 4, Y >> 4, PAL_PAINT(PAINT_JET, 1));
        pc_line((X - F.erx * 2 / 3) >> 4, (Y - F.ery / 2) >> 4, (X + F.erx * 2 / 3) >> 4, (Y - F.ery / 2) >> 4, m.c[0]);
        break; }
    case MARK_PENTAGRAM: {
        int cx, cy, r = big ? 4 : 2;
        static const int8_t px5[5] = { 0, 10, 6, -6, -10 };
        static const int8_t py5[5] = { -10, -3, 8, 8, -3 };
        v = fpt(0, -170, &X, &Y);
        cx = X >> 4; cy = Y >> 4;
        for (k = 0; k < 5; k++) {
            int a = (k + 2) % 5;
            pc_line(cx + px5[k] * r / 10, cy + py5[k] * r / 10, cx + px5[a] * r / 10, cy + py5[a] * r / 10, m.c[1]);
        }
        break; }
    case MARK_TEARS:
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            fpt(phs.s * F.ex, F.ey, &X, &Y);
            pc_line(X >> 4, (Y + F.ery) >> 4, X >> 4, ((Y + F.ery) >> 4) + 5, m.c[1]);
            pc_px(X >> 4, ((Y + F.ery) >> 4) + 6, m.c[2]);
        }
        phs.s = 1;
        break;
    case MARK_PATCHES: {
        PMat p;
        pm_paint(&p, l->marking_color);
        pc_part(&p, SH_SOFT, PF_NOCAST);
        fpt(-150, 80, &X, &Y);
        pc_rrect(X, Y, PX(3), PX(3) - 8, 16);
        pc_mask_none();
        pc_mask_add(pcv.part);
        for (k = -3; k <= 3; k += 2) {
            pc_px((X >> 4) + k, (Y >> 4) - 3, p.c[0]);
            pc_px((X >> 4) + k, (Y >> 4) + 2, p.c[0]);
        }
        break; }
    case MARK_SCALES: {
        int x, y;
        pc_decal(&m, SH_FLAT);
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            fpt(phs.s * 196, 20, &X, &Y);
            for (y = -3; y <= 3; y += 2)
                for (x = -2; x <= 2; x += 2) {
                    pc_px((X >> 4) + x + ((y >> 1) & 1), (Y >> 4) + y, m.c[1]);
                    pc_px((X >> 4) + x + 1 + ((y >> 1) & 1), (Y >> 4) + y, m.c[2]);
                }
        }
        phs.s = 1;
        break; }
    case MARK_FUR_MUZZLE:
        /* lighter muzzle / chin patch */
        pc_decal(&m, SH_SOFT);
        pc_frame(hx(0), hy(F.my), hr(150), hry(90));
        hE(-56, F.my - 16, 92, 84);
        hE(56, F.my - 16, 92, 84);
        hE(0, F.my + 30, 120, 80);
        break;
    case MARK_FUR_BELLY:
        /* chest is on the body; a pale chin echo on the face */
        pc_decal(&m, SH_SOFT);
        hE(0, 236, 110, 50);
        break;
    case MARK_FUR_SOCKS:
        /* paws are on the body; a little pale brow spot on the face */
        pc_decal(&m, SH_SOFT);
        hE(-F.ex, F.by - 30, 34, 22);
        hE(F.ex, F.by - 30, 34, 22);
        break;
    case MARK_FUR_MASK:
        pc_decal(&m, SH_SOFT);
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            fpt(phs.s * F.ex, F.ey + 10, &X, &Y);
            pc_ellipse(X + phs.s * 12, Y + 6, F.erx + hr(52), F.ery + hry(30));
        }
        phs.s = 1;
        fpt(0, F.ey - 10, &X, &Y);
        pc_ellipse(X, Y, hr(60), hry(30));
        break;
    case MARK_FUR_TABBY:
        pc_decal(&m, SH_FLAT);
        /* forehead "M" and cheek stripes */
        hL(-70, -150, -40, -210, m.c[1]); hL(-40, -210, 0, -160, m.c[1]);
        hL(0, -160, 40, -210, m.c[1]); hL(40, -210, 70, -150, m.c[1]);
        hL(0, -240, 0, -190, m.c[1]);
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            hT(258, 0, 258, 36, 180, 14);
            hT(258, 80, 258, 112, 196, 94);
        }
        phs.s = 1;
        break;
    case MARK_FUR_CALICO: {
        PMat o;
        pm_paint(&o, PAINT_TANGERINE);
        if (l->marking_color == PAINT_TANGERINE) pm_paint(&o, PAINT_CHOCOLATE);
        pc_decal(&m, SH_SOFT);
        hE(-150, -150, 120, 110);
        pc_decal(&o, SH_SOFT);
        hE(170, -120, 100, 90);
        hE(-200, 120, 70, 60);
        break; }
    case MARK_FUR_TIPPED_EARS:
        /* ear tips are drawn with the ears; mark the crown too */
        pc_decal(&m, SH_FLAT);
        hE(0, -238, 70, 30);
        break;
    default:
        break;
    }
    pc_mask_none();
}

/* ---- facial hair ---------------------------------------------------------------------- */
static void facial_hair(PCtx *c, int big)
{
    const DoomoLook *l = c->l;
    int fh = l->facial_hair, X, Y, v;
    if (fh == FHAIR_NONE || phs.h->yaw >= 3) return;
    v = fpt(0, F.my, &X, &Y);
    if (v <= 0) return;
    switch (fh) {
    case FHAIR_STUBBLE: {
        int x, y;
        pc_mask_none();
        pc_mask_add(c->head_id);
        for (y = 0; y < 4; y++)
            for (x = -6; x <= 6; x++)
                if (((x + y) & 1) == 0) pc_px((X >> 4) + x, (Y >> 4) + y + (p_abs(x) > 4 ? -2 : 0), c->hair.c[1]);
        pc_mask_none();
        break; }
    case FHAIR_MUSTACHE:
        pc_part(&c->hair, SH_BALL, 0);
        pc_ellipse(X - PX(2), Y - PX(2), PX(2) + 8, PX(1) + 2);
        pc_ellipse(X + PX(2), Y - PX(2), PX(2) + 8, PX(1) + 2);
        break;
    case FHAIR_GOATEE:
        pc_part(&c->hair, SH_BALL, 0);
        pc_tri(X - PX(2), Y + PX(2), X + PX(2), Y + PX(2), X, Y + PX(6));
        pc_ellipse(X, Y + PX(3), PX(2), PX(1) + 8);
        break;
    case FHAIR_FULL_BEARD:
        pc_part(&c->hair, SH_BALL, PF_FUZZ);
        hHeadFrame(20);
        hE(0, 170, 210, 120);
        hT(-250, 0, -180, 200, -250, 120);
        hT(250, 0, 180, 200, 250, 120);
        if (big) {
            hT(-120, 230, -40, 230, -80, 320);
            hT(40, 230, 120, 230, 80, 320);
            hT(-40, 250, 40, 250, 0, 330);
        }
        /* mouth hole */
        {
            PMat in;
            pm_flat(&in, c->hair.c[0]);
            pc_decal(&in, SH_FLAT);
            pc_line((X >> 4) - 2, Y >> 4, (X >> 4) + 2, Y >> 4, MOUTHIN);
        }
        break;
    case FHAIR_HANDLEBAR:
        pc_part(&c->hair, SH_BALL, 0);
        pc_capsule(X - PX(1), Y - PX(2), X - PX(5), Y - PX(1), PX(1) + 4, 12);
        pc_capsule(X + PX(1), Y - PX(2), X + PX(5), Y - PX(1), PX(1) + 4, 12);
        pc_capsule(X - PX(5), Y - PX(1), X - PX(6), Y - PX(4), 12, 10);
        pc_capsule(X + PX(5), Y - PX(1), X + PX(6), Y - PX(4), 12, 10);
        break;
    case FHAIR_CHOPS:
        pc_part(&c->hair, SH_BALL, PF_FUZZ);
        hHeadFrame(10);
        for (phs.s = -1; phs.s <= 1; phs.s += 2) {
            hQ(200, -60, 262, -40, 236, 150, 170, 150);
            hE(186, 150, 50, 46);
        }
        phs.s = 1;
        break;
    case FHAIR_WIZARD:
        pc_part(&c->hair, SH_SOFT, PF_FUZZ);
        hHeadFrame(30);
        hE(0, 160, 190, 110);
        hT(-200, 150, 200, 150, 30, 560);
        hT(-150, 200, 60, 200, -40, 470);
        break;
    default:
        break;
    }
}

/* ---- the head ------------------------------------------------------------------------- */
void ph_head_base(PCtx *c)
{
    const DoomoLook *l = c->l;
    PHead *h = &c->h;
    int big = h->lod == LOD_BIG;
    int sp = l->species, flags = 0;
    int shade = SH_BALL;
    phs.c = c; phs.h = h; phs.s = 1; phs.z = 0;
    face_layout(c);
    F.lid = face_ink(c);
    if (c->critter) flags |= PF_FUZZ;
    if (sp == SPECIES_SLIME || sp == SPECIES_ROBOT) shade = SH_GLOSS;
    if (sp == SPECIES_PLUSHIE || sp == SPECIES_SHEEPKIN || sp == SPECIES_GHOST) shade = SH_SOFT;
    pc_part(&c->skin, shade, flags);
    c->head_id = pcv.part;
    head_shape(c);
    /* fur silhouette extras on the same part */
    if (h->yaw < 4) {
        hHeadFrame(0);
        if (c->critter || sp == SPECIES_IMP) cheek_tufts(c, big);
        if (sp == SPECIES_SLIME) {
            /* drips off the chin */
            hC(-120, 200, -120, 290, 34, 30);
            hC(90, 220, 90, 270, 26, 22);
        }
        if (sp == SPECIES_FROGLING) {
            /* eye bumps */
            hE(-F.ex, -200, 110, 90);
            hE(F.ex, -200, 110, 90);
        }
        if (sp == SPECIES_GHOST) hT(-60, -230, 60, -230, 30, -330);
        if (sp == SPECIES_IMP) hT(-50, 220, 50, 220, 0, 300);
        if ((c->critter || sp == SPECIES_MOTHKIN) && !has_hair_cover(l->hair_style) &&
            l->hair_style != HAIR_MOHAWK)
            crown_tuft();
    }
}

void ph_face(PCtx *c)
{
    PHead *h = &c->h;
    int big = h->lod == LOD_BIG;
    const DoomoLook *l = c->l;
    phs.c = c; phs.h = h; phs.s = 1; phs.z = 0;
    species_head(c, big);
    if (h->yaw >= 3) return;
    muzzle(c);
    markings(c, big);
    blush(c, big);
    eyes(c);
    brows(c);
    nose(c);
    mouth(c);
    if (big || h->lod == LOD_MID) {
        switch (l->species) {
        case SPECIES_CATFOLK: whiskers(c, big ? 3 : 2, big ? 5 : 3); break;
        case SPECIES_MOUSEKIN: whiskers(c, 2, big ? 7 : 4); break;
        case SPECIES_RACCOONKIN: whiskers(c, 2, big ? 4 : 2); break;
        default: break;
        }
    }
    facial_hair(c, big);
}
