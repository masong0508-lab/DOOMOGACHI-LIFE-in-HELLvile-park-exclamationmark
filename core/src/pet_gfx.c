/* pet_gfx.c - chunky LCD pixel art for the Doomogachi (owner: life).
 *
 * Every form is generated in code into a 32x32 canvas of LCD "dots":
 * 0 empty, 1 ink, 2 mid tone (shading, blush), 3 lit (opaque background
 * colour, used for the inside of bodies and eye glints). A form is a
 * small recipe: body shape and size, ears or horns, extras (halo, wings,
 * tails, mane, sprinkles, flames, drips ...), eyes and mouth. Silhouettes
 * are filled, then auto-outlined in ink, shaded with a soft dither on the
 * lower right, and the face is drawn per animation frame (idle bob with
 * squash, happy jump, eating, sad, sleeping, sick, angry).
 */
#include "doomo/doomo.h"

#define N PET_SPR

enum { SH_BLOB, SH_DROP, SH_BEAN, SH_CLOUD, SH_CUPCAKE, SH_MUFFIN, SH_BEAST };
enum { EA_NONE, EA_CAT, EA_BUNNY, EA_TUFT, EA_HORN_S, EA_HORN_L, EA_HORN_CURL, EA_POINTY };
enum { EY_DOT, EY_BIG, EY_SERENE, EY_GRUMPY, EY_MISCHIEF, EY_CYCLOPS };
enum { MO_SMILE, MO_CAT, MO_FANG, MO_GRIN, MO_O };
enum {
    F_HALO = 1, F_WINGS_S = 2, F_WINGS_L = 4, F_BATWINGS = 8, F_TAIL_DEVIL = 16,
    F_TAIL_FLUFF = 32, F_MANE = 64, F_SPRINKLES = 128, F_FEET = 256, F_ARMS = 512,
    F_BLUSH = 1024, F_DRIP = 2048, F_FLAME = 4096, F_CHERRY = 8192, F_COLLAR = 16384
};

typedef struct {
    int8_t cx, cy, rx, ry;
    uint8_t shape, ears, eyes, mouth;
    uint16_t flags;
} FormDef;

static const FormDef forms[PET_FORM_COUNT] = {
    /* CUTE */
    { 16, 24,  9,  6, SH_DROP,    EA_NONE,  EY_DOT,      MO_SMILE, F_BLUSH },
    { 16, 21, 11,  9, SH_BLOB,    EA_CAT,   EY_BIG,      MO_CAT,   F_BLUSH | F_FEET },
    { 16, 19, 10, 11, SH_BEAN,    EA_BUNNY, EY_BIG,      MO_SMILE, F_SPRINKLES | F_ARMS | F_FEET | F_BLUSH },
    { 16, 14, 13,  8, SH_CUPCAKE, EA_NONE,  EY_BIG,      MO_CAT,   F_CHERRY | F_ARMS | F_BLUSH },
    /* ANGEL */
    { 16, 24,  9,  6, SH_DROP,    EA_NONE,  EY_SERENE,   MO_SMILE, F_HALO | F_BLUSH },
    { 16, 21, 11,  9, SH_CLOUD,   EA_NONE,  EY_SERENE,   MO_CAT,   F_HALO | F_WINGS_S | F_BLUSH },
    { 16, 19, 10, 11, SH_BEAN,    EA_BUNNY, EY_SERENE,   MO_O,     F_HALO | F_COLLAR | F_FEET },
    { 16, 18, 11, 11, SH_BLOB,    EA_TUFT,  EY_SERENE,   MO_SMILE, F_HALO | F_WINGS_L | F_FEET | F_BLUSH },
    /* FERAL */
    { 16, 24,  9,  6, SH_DROP,    EA_TUFT,  EY_GRUMPY,   MO_FANG,  0 },
    { 16, 21, 10,  9, SH_BLOB,    EA_POINTY,EY_GRUMPY,   MO_FANG,  F_MANE | F_FEET | F_ARMS },
    { 16, 19, 11, 10, SH_BLOB,    EA_POINTY,EY_GRUMPY,   MO_FANG,  F_MANE | F_TAIL_FLUFF | F_FEET },
    { 17, 21, 12,  7, SH_BEAST,   EA_POINTY,EY_GRUMPY,   MO_FANG,  F_MANE | F_TAIL_FLUFF },
    /* DEMON */
    { 16, 24,  9,  6, SH_DROP,    EA_HORN_S,EY_MISCHIEF, MO_GRIN,  F_TAIL_DEVIL },
    { 16, 21, 10, 10, SH_BLOB,    EA_HORN_L,EY_MISCHIEF, MO_GRIN,  F_TAIL_DEVIL | F_FEET | F_BLUSH },
    { 16, 19, 12,  8, SH_MUFFIN,  EA_HORN_L,EY_MISCHIEF, MO_FANG,  F_FLAME | F_TAIL_DEVIL },
    { 16, 18, 12, 11, SH_BLOB,    EA_HORN_CURL, EY_CYCLOPS, MO_GRIN, F_BATWINGS | F_DRIP | F_TAIL_DEVIL }
};

/* ---------------------------------------------------------------------- */
/* canvas                                                                   */
static uint8_t *cv;

static void dot(int x, int y, int v)
{
    if ((unsigned)x < N && (unsigned)y < N) cv[y * N + x] = (uint8_t)v;
}
static int get(int x, int y) { return ((unsigned)x < N && (unsigned)y < N) ? cv[y * N + x] : 0; }

/* ellipse with a centre in half pixels (cx2 = 2*cx) so even widths work;
 * one span per row (integer sqrt), so a form renders in a few thousand
 * operations */
static void ell2(int cx2, int cy2, int rx2, int ry2, int v)
{
    int y;
    if (rx2 <= 0 || ry2 <= 0) return;
    for (y = 0; y < N; y++) {
        int dy = 2 * y + 1 - cy2, x0, x1, x;
        uint32_t k, hw;
        if (dy < 0) dy = -dy;
        if (dy > ry2) continue;
        /* half width (in half pixels) = rx2 * sqrt(1 - dy^2 / ry2^2) */
        k = (uint32_t)(ry2 * ry2 - dy * dy) * 4096u / (uint32_t)(ry2 * ry2);
        hw = (uint32_t)rx2 * fx_isqrt(k * 4096u) / 4096u;
        /* pixel x is inside when |2x + 1 - cx2| <= hw */
        x0 = (cx2 - (int)hw) / 2;
        if (2 * x0 + 1 < cx2 - (int)hw) x0++;
        x1 = (cx2 + (int)hw - 1) / 2;
        if (x0 < 0) x0 = 0;
        if (x1 > N - 1) x1 = N - 1;
        for (x = x0; x <= x1; x++) cv[y * N + x] = (uint8_t)v;
    }
}
static void ell(int cx, int cy, int rx, int ry, int v) { ell2(2 * cx, 2 * cy, 2 * rx + 1, 2 * ry + 1, v); }

static void rect(int x, int y, int w, int h, int v)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) dot(x + i, y + j, v);
}

static void line(int x0, int y0, int x1, int y1, int v)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx - dy;
    for (;;) {
        dot(x0, y0, v);
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = err * 2;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 < dx) { err += dx; y0 += sy; }
        }
    }
}

/* every lit body dot touching the outside becomes ink (in place, with
 * copies of the previous and current rows) */
static void outline(void)
{
    uint8_t prev[N], cur[N];
    int x, y;
    memset(prev, 0, sizeof(prev));
    for (y = 0; y < N; y++) {
        memcpy(cur, &cv[y * N], N);
        for (x = 0; x < N; x++) {
            if (cur[x] != 3) continue;
            if (x == 0 || y == 0 || x == N - 1 || y == N - 1 || !cur[x - 1] || !cur[x + 1] ||
                !prev[x] || !cv[(y + 1) * N + x])
                cv[y * N + x] = 1;
        }
        memcpy(prev, cur, N);
    }
}

/* soft dithered shade on the lower right of the body */
static void shade(int cx, int cy, int rx, int ry)
{
    int x, y;
    for (y = 0; y < N; y++)
        for (x = 0; x < N; x++) {
            int u, k;
            if (cv[y * N + x] != 3) continue;
            u = (x - cx) * 16 / (rx > 0 ? rx : 1) + (y - cy) * 16 / (ry > 0 ? ry : 1);
            k = u - 10;
            if (k >= 10 && get(x + 1, y + 1) != 0) cv[y * N + x] = 2;
            else if (k > 0 && ((x + y) & 1)) cv[y * N + x] = 2;
        }
}

/* ---------------------------------------------------------------------- */
/* parts                                                                    */
static void ears(int kind, int cx, int top, int rx)
{
    int s = rx * 6 / 10;
    switch (kind) {
    case EA_CAT: case EA_POINTY: {
        int h = kind == EA_POINTY ? 6 : 5, i;
        for (i = 0; i < h; i++) {
            int w = (h - i) * 2 / 2 + 1;
            rect(cx - s - w / 2, top - i + 1, w, 1, 3);
            rect(cx + s - w / 2 + 1, top - i + 1, w, 1, 3);
        }
        break;
    }
    case EA_BUNNY:
        ell(cx - 4, top - 5, 2, 6, 3);
        ell(cx + 4, top - 5, 2, 6, 3);
        break;
    case EA_TUFT:
        line(cx - 1, top + 1, cx - 3, top - 3, 3);
        line(cx, top + 1, cx + 1, top - 4, 3);
        line(cx + 1, top + 1, cx + 4, top - 2, 3);
        dot(cx - 3, top - 3, 3); dot(cx + 1, top - 4, 3); dot(cx + 4, top - 2, 3);
        break;
    default: break;
    }
}

static void horns(int kind, int cx, int top, int rx)
{
    int s = rx * 6 / 10;
    if (kind == EA_HORN_S) {
        rect(cx - s, top - 1, 2, 2, 1); dot(cx - s, top - 2, 1);
        rect(cx + s - 1, top - 1, 2, 2, 1); dot(cx + s, top - 2, 1);
    } else if (kind == EA_HORN_L) {
        line(cx - s, top + 1, cx - s - 2, top - 4, 1);
        line(cx - s + 1, top + 1, cx - s - 1, top - 4, 1);
        line(cx + s, top + 1, cx + s + 2, top - 4, 1);
        line(cx + s - 1, top + 1, cx + s + 1, top - 4, 1);
        dot(cx - s - 3, top - 5, 1); dot(cx + s + 3, top - 5, 1);
    } else if (kind == EA_HORN_CURL) {
        int sd;
        for (sd = -1; sd <= 1; sd += 2) {
            int bx = cx + sd * s;
            line(bx, top + 2, bx + sd * 3, top - 2, 1);
            line(bx + sd, top + 2, bx + sd * 4, top - 2, 1);
            line(bx + sd * 4, top - 2, bx + sd * 5, top - 5, 1);
            line(bx + sd * 5, top - 5, bx + sd * 3, top - 6, 1);
            dot(bx + sd * 2, top - 5, 1);
        }
    }
}

static void halo(int cx, int y)
{
    int i;
    for (i = -4; i <= 4; i++) { dot(cx + i, y - 1, 1); dot(cx + i, y + 1, 1); }
    dot(cx - 5, y, 1); dot(cx + 5, y, 1);
    for (i = -3; i <= 3; i++) dot(cx + i, y, 2);
}

static void wings(int cx, int cy, int rx, int big, int bat)
{
    int sd;
    for (sd = -1; sd <= 1; sd += 2) {
        int bx = cx + sd * (rx - 1), i, len = big ? 8 : 5;
        if (bat) {
            for (i = 0; i < len; i++) line(bx, cy - 2 + i / 3, bx + sd * (len - i / 2), cy - 6 + i, 1);
            for (i = 1; i < len - 1; i++) line(bx + sd, cy - 1 + i / 3, bx + sd * (len - 1 - i / 2), cy - 5 + i, 2);
            dot(bx + sd * len, cy - 7, 1);
        } else {
            ell(bx + sd * (len / 2 + 1), cy - 3, len / 2 + 1, big ? 4 : 3, 3);
            ell(bx + sd * (len / 2 + 2), cy - 1, len / 2, big ? 3 : 2, 3);
        }
    }
}

static void tail(int kind, int x, int y, int frame)
{
    int wag = (frame & 1) ? 1 : 0;
    if (kind == F_TAIL_DEVIL) {
        line(x, y, x + 3, y - 2 - wag, 1);
        line(x + 3, y - 2 - wag, x + 4, y - 5 - wag, 1);
        rect(x + 3, y - 8 - wag, 3, 2, 1);
        dot(x + 4, y - 9 - wag, 1);
    } else {
        ell(x + 3, y - 3 - wag, 3, 3, 3);
        ell(x + 5, y - 6 - wag, 2, 2, 3);
    }
}

/* draw the silhouette of a body shape (value 3) */
static void body(const FormDef *f, int cy, int rx, int ry)
{
    int cx = f->cx, i;
    switch (f->shape) {
    case SH_DROP:
        ell(cx, cy, rx, ry, 3);
        rect(cx - rx + 1, cy + 1, rx * 2 - 1, ry, 3);        /* flat mochi bottom */
        for (i = 0; i < N; i++) if (cy + ry + 1 < N) cv[(cy + ry + 1) * N + i] = 0;
        break;
    case SH_BEAN:
        ell(cx, cy - ry / 3, rx - 1, ry * 2 / 3, 3);
        ell(cx, cy + ry / 3, rx, ry * 2 / 3, 3);
        break;
    case SH_CLOUD:
        ell(cx, cy, rx, ry, 3);
        for (i = -rx + 2; i <= rx - 2; i += 4) ell(cx + i, cy - ry + 1, 2, 2, 3);
        ell(cx - rx, cy + 1, 2, 3, 3);
        ell(cx + rx, cy + 1, 2, 3, 3);
        break;
    case SH_CUPCAKE:
        /* frosting dome on top, wrapper below */
        ell(cx, cy, rx, ry, 3);
        ell(cx - 5, cy + ry - 2, 3, 2, 3);
        ell(cx + 5, cy + ry - 2, 3, 2, 3);
        for (i = 0; i < 9; i++) rect(cx - 9 + i / 3, cy + ry + i, 19 - (i / 3) * 2, 1, 3);
        break;
    case SH_MUFFIN:
        ell(cx, cy, rx, ry, 3);
        for (i = -rx + 2; i <= rx - 2; i += 4) ell(cx + i, cy - ry + 1, 2, 2, 3);
        for (i = 0; i < 8; i++) rect(cx - 8 + i / 3, cy + ry - 1 + i, 17 - (i / 3) * 2, 1, 3);
        break;
    case SH_BEAST:
        ell(cx + 2, cy, rx, ry, 3);                           /* body       */
        ell(cx - 7, cy - 6, 6, 6, 3);                         /* head left  */
        for (i = 0; i < 4; i++) rect(cx - 6 + i * 6, cy + ry - 2, 3, 6, 3);  /* legs */
        break;
    default:
        ell(cx, cy, rx, ry, 3);
        break;
    }
}

static void mane(int cx, int cy, int r)
{
    int a;
    for (a = 0; a < DOOMO_ANG_FULL; a += DOOMO_ANG_FULL / 12) {
        int x0 = cx + ((fx_cos(a) * r) >> 16), y0 = cy + ((fx_sin(a) * r) >> 16);
        int x1 = cx + ((fx_cos(a + 85) * (r + 3)) >> 16), y1 = cy + ((fx_sin(a + 85) * (r + 3)) >> 16);
        line(x0, y0, x1, y1, 3);
        dot(x1, y1, 3);
    }
}

/* ---------------------------------------------------------------------- */
/* faces                                                                    */
static void eye(int x, int y, int kind, int frame, int left)
{
    if (frame == PET_ANIM_SLEEP || (frame == PET_ANIM_IDLE1 && kind != EY_CYCLOPS)) {
        rect(x - 1, y + 1, 3, 1, 1);                          /* closed     */
        return;
    }
    if (frame == PET_ANIM_HAPPY) {                            /* ^ ^        */
        dot(x - 1, y + 1, 1); dot(x, y, 1); dot(x + 1, y + 1, 1);
        return;
    }
    if (frame == PET_ANIM_SICK) {                             /* x x        */
        dot(x - 1, y - 1, 1); dot(x + 1, y - 1, 1); dot(x, y, 1); dot(x - 1, y + 1, 1); dot(x + 1, y + 1, 1);
        return;
    }
    if (frame == PET_ANIM_SAD) {                              /* > <        */
        if (left) { dot(x - 1, y - 1, 1); dot(x, y, 1); dot(x - 1, y + 1, 1); }
        else { dot(x + 1, y - 1, 1); dot(x, y, 1); dot(x + 1, y + 1, 1); }
        dot(left ? x - 1 : x + 1, y + 3, 2);                  /* a tear     */
        return;
    }
    switch (kind) {
    case EY_DOT:
        rect(x, y, 2, 2, 1);
        break;
    case EY_BIG: case EY_CYCLOPS:
        rect(x - 1, y - 1, 3, 4, 1);
        dot(x - 1, y - 1, 3);                                 /* glint      */
        if (kind == EY_CYCLOPS) { rect(x - 2, y - 2, 5, 6, 1); rect(x - 1, y - 1, 3, 4, 2); rect(x, y, 1, 2, 1); dot(x - 1, y - 1, 3); }
        break;
    case EY_SERENE:
        dot(x - 1, y, 1); rect(x, y + 1, 2, 1, 1); dot(x + 2, y, 1);
        break;
    case EY_GRUMPY:
        rect(x - 1, y, 3, 2, 1);
        line(left ? x - 2 : x + 2, y - 2, left ? x + 1 : x - 1, y - 1, 1);
        break;
    case EY_MISCHIEF:
        rect(x - 1, y, 3, 2, 1);
        dot(left ? x - 1 : x + 1, y, 3);
        line(left ? x - 2 : x + 2, y - 1, left ? x + 1 : x - 1, y - 2, 1);
        break;
    }
    if (frame == PET_ANIM_ANGRY)
        line(left ? x - 2 : x + 2, y - 3, left ? x + 1 : x - 1, y - 2, 1);
}

static void mouth(int x, int y, int kind, int frame)
{
    if (frame == PET_ANIM_EAT) { rect(x - 1, y, 3, 3, 1); dot(x, y + 1, 2); return; }
    if (frame == PET_ANIM_HAPPY) { rect(x - 1, y, 3, 2, 1); dot(x, y + 1, 2); return; }
    if (frame == PET_ANIM_SAD || frame == PET_ANIM_SICK) {
        dot(x - 1, y + 1, 1); dot(x, y, 1); dot(x + 1, y + 1, 1);
        return;
    }
    if (frame == PET_ANIM_ANGRY) { dot(x - 2, y, 1); dot(x - 1, y + 1, 1); dot(x, y, 1); dot(x + 1, y + 1, 1); dot(x + 2, y, 1); return; }
    if (frame == PET_ANIM_SLEEP) { dot(x, y, 1); return; }
    switch (kind) {
    case MO_CAT: dot(x - 2, y, 1); dot(x - 1, y + 1, 1); dot(x, y, 1); dot(x + 1, y + 1, 1); dot(x + 2, y, 1); break;
    case MO_FANG: rect(x - 2, y, 5, 1, 1); dot(x - 1, y + 1, 1); dot(x + 1, y + 1, 1); break;
    case MO_GRIN: rect(x - 2, y, 5, 1, 1); dot(x - 3, y - 1, 1); dot(x + 3, y - 1, 1); dot(x + 1, y + 1, 1); break;
    case MO_O: rect(x - 1, y, 2, 2, 1); break;
    default: dot(x - 1, y, 1); rect(x, y + 1, 1, 1, 1); dot(x + 1, y, 1); break;
    }
}

static void draw_form(int form, int frame)
{
    const FormDef *f = &forms[form];
    int bob = 0, rx = f->rx, ry = f->ry, cy, cx = f->cx, top, eyey, sep;
    int squash = frame == PET_ANIM_IDLE1 || frame == PET_ANIM_SLEEP;
    if (frame == PET_ANIM_HAPPY) bob = -3;
    if (squash) { rx++; ry--; bob = 1; }
    cy = f->cy + bob;
    top = (f->shape == SH_BEAST) ? cy - 12 : (f->shape == SH_BEAN ? cy - ry - 1 : cy - ry);

    /* behind the body */
    if (f->flags & (F_WINGS_S | F_WINGS_L))
        wings(cx, cy - 1 + (frame == PET_ANIM_HAPPY ? -1 : 0), rx, (f->flags & F_WINGS_L) != 0, 0);
    if (f->flags & F_TAIL_FLUFF) tail(F_TAIL_FLUFF, cx + rx - 1, cy + ry - 2, frame);
    if (f->flags & F_MANE) mane(f->shape == SH_BEAST ? cx - 7 : cx, f->shape == SH_BEAST ? cy - 6 : cy - 1,
                                f->shape == SH_BEAST ? 7 : rx);
    ears(f->ears, f->shape == SH_BEAST ? cx - 7 : cx, f->shape == SH_BEAST ? cy - 11 : top + 1, f->shape == SH_BEAST ? 6 : rx);
    body(f, cy, rx, ry);
    if (f->flags & F_FEET) {
        int fy = cy + ry - (squash ? 0 : 1);
        ell(cx - rx / 2, fy + 1, 2, 1, 3);
        ell(cx + rx / 2, fy + 1, 2, 1, 3);
    }
    if (f->flags & F_ARMS) {
        int ay = cy + (frame == PET_ANIM_HAPPY ? -4 : 1);
        ell(cx - rx, ay, 1, 2, 3);
        ell(cx + rx, ay, 1, 2, 3);
    }
    if (f->flags & F_DRIP) {
        int i;
        for (i = -2; i <= 2; i++) rect(cx + i * 4, cy + ry - 1, 2, 3 + ((i + frame) & 1) * 2, 3);
    }
    outline();
    shade(cx, cy, rx, ry);

    /* surface details */
    if (f->shape == SH_CUPCAKE) {
        int i;
        for (i = -7; i <= 7; i += 3) line(cx + i, cy + ry + 1, cx + i * 8 / 9, cy + ry + 8, 2);
        rect(cx - 9, cy + ry, 19, 1, 1);
    }
    if (f->shape == SH_MUFFIN) {
        int i;
        for (i = -6; i <= 6; i += 3) line(cx + i, cy + ry, cx + i * 7 / 8, cy + ry + 6, 2);
        rect(cx - 8, cy + ry - 1, 17, 1, 1);
    }
    if (f->flags & F_SPRINKLES) {
        dot(cx - 4, cy + 3, 2); dot(cx + 5, cy + 1, 2); dot(cx - 1, cy + 6, 2); dot(cx + 3, cy + 6, 2);
        dot(cx - 6, cy - 1, 2);
    }
    if (f->flags & F_COLLAR) {
        int i;
        for (i = -5; i <= 5; i++) dot(cx + i, cy + 2 + (i & 1), 1);
    }
    if (f->flags & F_BLUSH) {
        int by = cy + (f->shape == SH_CUPCAKE ? 1 : 1);
        rect(cx - rx + 2, by, 2, 1, 2);
        rect(cx + rx - 3, by, 2, 1, 2);
    }
    /* face */
    sep = rx >= 10 ? 4 : 3;
    eyey = (f->shape == SH_CUPCAKE) ? cy - 2 : (f->shape == SH_BEAN ? cy - 4 : cy - ry / 3 - 1);
    if (f->shape == SH_BEAST) {
        eye(cx - 9, cy - 8, f->eyes, frame, 1);
        eye(cx - 5, cy - 8, f->eyes, frame, 0);
        mouth(cx - 7, cy - 4, f->mouth, frame);
    } else if (f->eyes == EY_CYCLOPS) {
        eye(cx, eyey, f->eyes, frame, 1);
        mouth(cx, eyey + 6, f->mouth, frame);
    } else {
        eye(cx - sep, eyey, f->eyes, frame, 1);
        eye(cx + sep - 1, eyey, f->eyes, frame, 0);
        mouth(cx, eyey + 4, f->mouth, frame);
    }
    /* in front */
    if (f->flags & F_TAIL_DEVIL) tail(F_TAIL_DEVIL, cx + rx - 1, cy + ry / 2, frame);
    if (f->flags & F_BATWINGS) wings(cx, cy - 2, rx, 1, 1);
    horns(f->ears, cx, top + 1, rx);
    if (f->flags & F_HALO) halo(cx, top - 4 + (frame & 1));
    if (f->flags & F_CHERRY) {
        ell(cx + 1, top - 2, 2, 2, 1);
        dot(cx, top - 3, 3);
        line(cx + 2, top - 4, cx + 4, top - 7, 1);
    }
    if (f->flags & F_FLAME) {
        int fl = frame & 1;
        line(cx - 3, top - 1, cx - 2, top - 5 - fl, 1);
        line(cx - 2, top - 5 - fl, cx, top - 2, 1);
        line(cx, top - 2, cx + 1, top - 7 + fl, 1);
        line(cx + 1, top - 7 + fl, cx + 3, top - 1, 1);
        dot(cx, top - 4, 2);
    }
    if (frame == PET_ANIM_SICK) { dot(cx + rx + 1, top + 1, 2); dot(cx + rx + 1, top + 2, 1); }
    if (frame == PET_ANIM_ANGRY) {                            /* puff marks */
        dot(cx + rx + 1, top - 1, 1); dot(cx + rx + 3, top - 1, 1);
        dot(cx + rx + 2, top - 2, 1); dot(cx + rx + 2, top, 1);
    }
}

static void draw_egg(int frame)
{
    int i, cx = 16 + (frame == 0 ? -1 : frame == 1 ? 1 : 0), cy = 19;
    ell(cx, cy, 8, 10, 3);
    outline();
    shade(cx, cy, 8, 10);
    /* a zigzag band and spots */
    for (i = -7; i <= 7; i++) dot(cx + i, cy + ((i + 8) & 2 ? 1 : -1), 1);
    for (i = -7; i <= 7; i++) dot(cx + i, cy + 1 + ((i + 8) & 2 ? 1 : -1), 2);
    ell(cx - 3, cy - 5, 1, 1, 2);
    ell(cx + 3, cy - 7, 1, 1, 2);
    ell(cx + 2, cy + 5, 1, 1, 2);
    dot(cx - 4, cy - 6, 3);
    if (frame >= 2) {
        line(cx - 2, cy - 9, cx, cy - 6, 1);
        line(cx, cy - 6, cx - 1, cy - 3, 1);
        line(cx - 1, cy - 3, cx + 2, cy - 1, 1);
    }
    if (frame >= 3) {
        line(cx + 2, cy - 1, cx + 5, cy - 4, 1);
        line(cx - 2, cy - 9, cx - 5, cy - 7, 1);
        dot(cx + 7, cy - 9, 1); dot(cx - 7, cy - 11, 1); dot(cx + 9, cy - 4, 1);
    }
}

static void draw_ghost(int frame)
{
    int cx = 16, cy = 16 - (frame & 1), i;
    ell(cx, cy, 8, 8, 3);
    rect(cx - 8, cy, 17, 7, 3);
    for (i = -8; i <= 8; i += 4) ell(cx + i + 2 * (frame & 1), cy + 8, 2, 2, 0);
    outline();
    shade(cx, cy, 8, 8);
    halo(cx, cy - 11);
    dot(cx - 4, cy - 1, 1); dot(cx - 3, cy - 2, 1); dot(cx - 2, cy - 1, 1);
    dot(cx + 2, cy - 1, 1); dot(cx + 3, cy - 2, 1); dot(cx + 4, cy - 1, 1);
    dot(cx, cy + 2, 1);
    rect(cx - 6, cy + 1, 2, 1, 2);
    rect(cx + 5, cy + 1, 2, 1, 2);
}

void pet_render(int form, int frame, uint8_t *out)
{
    cv = out;
    memset(out, 0, N * N);
    frame = DOOMO_CLAMP(frame, 0, PET_ANIM_COUNT - 1);
    if (form == -1) draw_egg(frame & 3);
    else if (form == -2) draw_ghost(frame);
    else if (form >= 0 && form < PET_FORM_COUNT) draw_form(form, frame);
}

/* ---------------------------------------------------------------------- */
/* as DoomoImages in LCD colours                                            */
static uint8_t lcd_col[4];

static void lcd_colours(void)
{
    if (lcd_col[1]) return;
    lcd_col[0] = 0;
    lcd_col[1] = palette_nearest(44, 56, 40);
    lcd_col[2] = palette_nearest(104, 124, 76);
    lcd_col[3] = PAL_UI_LCD;
}

#define IMG_CACHE 4
static DoomoImage img_img[IMG_CACHE];
static int16_t img_key[IMG_CACHE] = { -999, -999, -999, -999 };
static uint8_t img_next;

/* (integration, memory budget) The image cache and SCENE_PET's 32x32
 * canvas (5 KB) live in the shared scene scratch (scene.h, tag PETS), not
 * in .bss: both are caches that only the pet screen uses (pet_image() is
 * not drawn anywhere else), remade when another scene took the scratch. */
#define PET_SCRATCH_TAG SAVE_EXT_TAG('P', 'E', 'T', 'S')
typedef struct PetScratch {
    uint8_t img[IMG_CACHE][N * N];
    uint8_t canvas[N * N];              /* pet_scene.c's render_cached()  */
} PetScratch;
static PetScratch *pet_scr;
static uint8_t canvas_lost;

static PetScratch *pet_scratch(void)
{
    int i;
    if (!pet_scr || !scene_scratch_mine(PET_SCRATCH_TAG)) {
        pet_scr = (PetScratch *)scene_scratch(PET_SCRATCH_TAG, (uint32_t)sizeof(PetScratch));
        for (i = 0; i < IMG_CACHE; i++) img_key[i] = -999;
        canvas_lost = 1;
    }
    return pet_scr;
}

/* SCENE_PET's canvas; *fresh = 1 when what it held is gone. */
uint8_t *pet_scene_canvas(int *fresh)
{
    PetScratch *ps = pet_scratch();
    *fresh = canvas_lost;
    canvas_lost = 0;
    return ps->canvas;
}

const DoomoImage *pet_form_image(int form, int frame)
{
    int key = (form + 2) * 16 + (frame & 15), i, k;
    uint8_t (*img_pix)[N * N] = pet_scratch()->img;
    lcd_colours();
    for (i = 0; i < IMG_CACHE; i++)
        if (img_key[i] == key) return &img_img[i];
    i = img_next;
    img_next = (uint8_t)((img_next + 1) % IMG_CACHE);
    pet_render(form, frame, img_pix[i]);
    for (k = 0; k < N * N; k++) img_pix[i][k] = lcd_col[img_pix[i][k] & 3];
    img_img[i].w = N;
    img_img[i].h = N;
    img_img[i].pix = img_pix[i];
    img_img[i].uid = doomo_image_new_uid();
    img_key[i] = (int16_t)key;
    return &img_img[i];
}
