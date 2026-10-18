/* home_score.c - the house score (owner: H1; HF sec.4): Comfort, Spook,
 * Style (Harmony from Ch4), Light and Barricade (Ch3), the named feng-shui
 * rules the Score panel explains, and the grade stamps COSY / LIT / HELD /
 * HOME / ETERNAL. Integer only; recomputed when the layout changes, never
 * per frame. HOME also needs the Annex furnished: home.c applies that one
 * house-wide.
 */
#include "home_int.h"

#define GRID 16

/* kept small: the score runs on the 6 KB GBA stack */
typedef struct ScoreObj { const DoomoFurn *f; uint8_t x0, y0, x1, y1, rot, layer; } ScoreObj;
#define BIT(m, x, y) (((m)[y] >> (x)) & 1)
#define SETBIT(m, x, y) ((m)[y] |= (uint16_t)(1u << (x)))

static int clamp100(int v) { return v < 0 ? 0 : v > 100 ? 100 : v; }

static int box_adjacent(const ScoreObj *a, const ScoreObj *b)
{
    /* share an edge: touching on one axis, overlapping on the other */
    int ox = a->x0 < b->x1 && b->x0 < a->x1, oy = a->y0 < b->y1 && b->y0 < a->y1;
    return (ox && (a->y1 == b->y0 || b->y1 == a->y0)) || (oy && (a->x1 == b->x0 || b->x1 == a->x0));
}

static int box_dist(const ScoreObj *a, int x, int y)
{
    int dx = x < a->x0 ? a->x0 - x : x >= a->x1 ? x - a->x1 + 1 : 0;
    int dy = y < a->y0 ? a->y0 - y : y >= a->y1 ? y - a->y1 + 1 : 0;
    return dx + dy;
}

void home_score_room(const HomeRoom *r, int room, unsigned ctx, HomeScore *out)
{
    const HomeRoomDef *def = &home_room_defs[(unsigned)room < HOME_ROOM_COUNT ? room : 0];
    ScoreObj so[HOME_OBJ_MAX];
    uint16_t occ[GRID], lit[GRID], seen[GRID];
    uint8_t q[GRID * GRID];
    int theme_n[THEME_COUNT];
    int W = HR_W(r), D = HR_D(r), tiles = W * D;
    int n = 0, i, j, x, y, comfy = 0, spooky = 0, covered = 0, occupied = 0;
    int fs_comfort = 0, fs_spook = 0, fs_style = 0, fs_light = 0, harmony_bonus = 0, pairs = 0;
    int bed = -1, windows = 0, plates = 0, dom = 0, themed = 0, light, style, barricade = 0;
    int clutter, path = 0, qh, qt, dx_, dy_;
    memset(out, 0, sizeof(*out));
    memset(occ, 0, sizeof(occ));
    memset(lit, 0, sizeof(lit));
    memset(theme_n, 0, sizeof(theme_n));
    if (W > GRID) W = GRID;
    if (D > GRID) D = GRID;
    tiles = W * D;

    for (i = 0; i < r->nobj && i < HOME_OBJ_MAX; i++) {
        const HomeObj *o = &r->obj[i];
        const DoomoFurn *f = home_furn(HO_FURN(o));
        ScoreObj *s = &so[n];
        int w, d;
        if (!f) continue;
        home_footprint(f, HO_ROT(o), &w, &d);
        s->f = f; s->rot = (uint8_t)HO_ROT(o);
        s->x0 = (uint8_t)HO_X(o); s->y0 = (uint8_t)HO_Y(o);
        s->x1 = (uint8_t)(s->x0 + w); s->y1 = (uint8_t)(s->y0 + d);
        s->layer = (uint8_t)((f->flags & FF_WALL) ? 3 : (f->flags & FF_RUG) ? 0 : HO_ONSURF(o) ? 2 : 1);
        if (f->role == FR_WINDOW) windows++;
        if (f->role == FR_PLATE) plates++;
        n++;
        if (f->flags2 & FF_PORTAL) continue;           /* the door, windows, the hearth */
        out->pieces++;
        comfy += f->comfy;
        spooky += f->spooky;
        if (f->role == FR_SEAT || f->interact == FI_SIT) out->seats++;
        if (f->role == FR_BED || f->interact == FI_SLEEP) { out->beds++; if (bed < 0) bed = n - 1; }
        if (f->flags2 & FF_CURSED) out->cursed++;
        if (f->role == FR_TURRET) out->turret = 1;
        if (f->flags & FF_HELL) out->hell++; else out->hill++;
        if (f->theme < THEME_COUNT) { theme_n[f->theme]++; themed++; }
        if (s->layer == 1)
            for (y = s->y0; y < s->y1 && y < GRID; y++)
                for (x = s->x0; x < s->x1 && x < GRID; x++)
                    if (!BIT(occ, x, y)) { SETBIT(occ, x, y); occupied++; }
        if ((f->flags & FF_LIGHT) && !(o->pa & HOS_OFF)) {
            int rr = f->light_r ? f->light_r : 1;
            if (f->role == FR_CANDLE) rr *= 2;                /* candles count double */
            out->lamps++;
            for (y = 0; y < D; y++)
                for (x = 0; x < W; x++)
                    if (box_dist(s, x, y) <= rr) SETBIT(lit, x, y);
        }
    }
    for (y = 0; y < D; y++) for (x = 0; x < W; x++) covered += BIT(lit, x, y);
    light = tiles ? 100 * covered / tiles : 0;

    /* ---- feng shui (HF sec.4.2) ---- */
    if (def->hearth_at != 0xFF) out->rules |= HRULE_FIREPLACE_CLEAR;
    if (bed >= 0) {
        const ScoreObj *b = &so[bed];
        int faces = def->door_wall == HW_A ? b->rot == 2 : b->rot == 3;
        if (faces) { fs_comfort -= 5; out->rules |= HRULE_BED_FACES_DOOR; }
        else { fs_comfort += 5; out->rules |= HRULE_BED_NOT_DOOR; }
    }
    for (i = 0; i < n; i++) {
        const ScoreObj *s = &so[i];
        if (bed >= 0) {
            const ScoreObj *b = &so[bed];
            int cx = s->x0, cy = s->y0;
            if ((s->f->flags & FF_LIGHT) && !(out->rules & HRULE_LAMP_NEAR_BED) && i != bed &&
                box_dist(b, cx, cy) <= 2) {
                fs_comfort += 5; fs_spook -= 5; out->rules |= HRULE_LAMP_NEAR_BED;
            }
            if (s->f->role == FR_PLUSH && !(out->rules & HRULE_PLUSH_ON_BED) && box_dist(b, cx, cy) == 0) {
                fs_comfort += 3; out->rules |= HRULE_PLUSH_ON_BED;
            }
            if (s->f->role == FR_PENTAGRAM && !(out->rules & HRULE_PENTAGRAM_BED) &&
                s->x0 < b->x1 && b->x0 < s->x1 && s->y0 < b->y1 && b->y0 < s->y1) {
                fs_comfort -= 10; fs_spook += 15; out->rules |= HRULE_PENTAGRAM_BED;
            }
        }
        if (s->f->role == FR_MIRROR && !(out->rules & HRULE_MIRROR_WINDOW))
            for (j = 0; j < n; j++)
                if (so[j].f->role == FR_WINDOW && (so[j].rot & 1) != (s->rot & 1)) {
                    fs_light += 10; fs_style += 3; out->rules |= HRULE_MIRROR_WINDOW;
                    break;
                }
        if (s->f->cat == FCAT_KITCHEN && room != HOME_ROOM_KITCHEN && !(s->f->flags2 & FF_PORTAL)) {
            fs_style -= 3; out->rules |= HRULE_KITCHEN_OUTSIDE;
        }
        if (s->f->role == FR_PLAIN_HUMAN) out->rules |= HRULE_PLAIN_HUMAN;   /* it does nothing */
        for (j = i + 1; j < n; j++) {
            const ScoreObj *t = &so[j];
            if (s->layer != 1 || t->layer != 1 || !box_adjacent(s, t)) continue;
            if (s->f->role == FR_TABLE && t->f->role == FR_TABLE) out->rules |= HRULE_TABLES_TOGETHER;
            if (((s->f->flags ^ t->f->flags) & FF_HELL) && !((s->f->flags2 | t->f->flags2) & FF_PORTAL))
                pairs++;
        }
    }
    if (ctx & HSC_CH4) {
        if (out->rules & HRULE_TABLES_TOGETHER) harmony_bonus += 10;
        if (pairs) { harmony_bonus += DOOMO_MIN(pairs * 2, 20); out->rules |= HRULE_MIXED_PAIRS; }
    }

    /* ---- the path: can the door reach the bed? (BFS over free tiles) ---- */
    if (bed >= 0) {
        int sx0 = def->door_wall == HW_A ? def->door_at : 0, sy0 = def->door_wall == HW_A ? 0 : def->door_at;
        int reach = 0;
        memset(seen, 0, sizeof(seen));
        qh = qt = 0;
        if (sx0 < W && sy0 < D && !BIT(occ, sx0, sy0)) { SETBIT(seen, sx0, sy0); q[qt++] = (uint8_t)(sx0 | (sy0 << 4)); }
        while (qh < qt && !reach) {
            static const int8_t step[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
            int cx = q[qh] & 15, cy = q[qh] >> 4, k;
            qh++;
            for (k = 0; k < 4; k++) {
                int nx = cx + step[k][0], ny = cy + step[k][1];
                if (nx < 0 || ny < 0 || nx >= W || ny >= D) continue;
                if (box_dist(&so[bed], nx, ny) == 0) { reach = 1; break; }
                if (BIT(seen, nx, ny) || BIT(occ, nx, ny)) continue;
                SETBIT(seen, nx, ny);
                q[qt++] = (uint8_t)(nx | (ny << 4));
            }
        }
        if (!reach) { path = 8; out->rules |= HRULE_BED_UNREACHABLE; }
    }

    clutter = tiles && occupied * 100 / tiles > 60 ? occupied * 100 / tiles - 60 : 0;
    if (clutter) out->rules |= HRULE_CLUTTER;

    /* ---- the stats (HF sec.4.1) ---- */
    out->stat[HS_COMFORT] = (uint8_t)clamp100(comfy + (out->beds ? 10 : 0) + 4 * DOOMO_MIN(out->seats, 6) +
                                              light / 4 - clutter - path + fs_comfort);
    out->stat[HS_SPOOK] = (uint8_t)clamp100(spooky + 15 * out->cursed +
                                            ((ctx & HSC_NIGHT) && light < 25 ? 20 : 0) +
                                            (room == HOME_ROOM_CELLAR ? 10 : 0) + fs_spook);
    for (i = 0; i < THEME_COUNT; i++) if (theme_n[i] > theme_n[dom]) dom = i;
    if (!out->pieces) style = 0;
    else if (ctx & HSC_CH4)
        style = 100 * DOOMO_MIN(out->hill, out->hell) * 2 / out->pieces + harmony_bonus;
    else
        style = themed ? 100 * theme_n[dom] / out->pieces : 0;
    out->stat[HS_STYLE] = (uint8_t)clamp100(style + fs_style);
    out->stat[HS_LIGHT] = (uint8_t)clamp100(light + fs_light +
                                            (!(ctx & HSC_NIGHT) && windows && !def->dim ? 50 : 0));
    if (ctx & HSC_CH3) {
        for (i = 0; i < n; i++) {
            const ScoreObj *s = &so[i];
            int near = 0;
            if (s->layer != 1 || !s->f->mass) continue;
            for (j = 0; j < n && !near; j++) {
                const ScoreObj *t = &so[j];
                if (t->f->role != FR_DOOR && t->f->role != FR_WINDOW) continue;
                /* the slot's floor tile, one tile of slack */
                dx_ = (t->rot & 1) ? 0 : t->x0;
                dy_ = (t->rot & 1) ? t->y0 : 0;
                if (s->x0 <= dx_ + 1 && dx_ - 1 < s->x1 && s->y0 <= dy_ + 1 && dy_ - 1 < s->y1) near = 1;
            }
            if (near) barricade += DOOMO_MIN(s->f->mass, 20);
        }
        barricade += 25 * plates + 40 * out->turret;
    }
    out->stat[HS_BARRICADE] = (uint8_t)clamp100(barricade);

    /* ---- the stamps ---- */
    if (out->stat[HS_COMFORT] >= 40 && out->beds && out->pieces >= 6) out->stamps |= HOME_STAMP(HG_COSY);
    if (light + fs_light >= 60) out->stamps |= HOME_STAMP(HG_LIT);     /* lamps only: a night goal */
    if (out->stat[HS_BARRICADE] >= 100 && out->turret) out->stamps |= HOME_STAMP(HG_HELD);
    if (out->seats >= 8 && out->hill && out->hell && (ctx & HSC_CH4) && out->stat[HS_STYLE] >= 60)
        out->stamps |= HOME_STAMP(HG_HOME);
    if (out->hell >= 4 && out->cursed >= 1) out->stamps |= HOME_STAMP(HG_ETERNAL);
}
