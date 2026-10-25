/* home_edit.c - the house editor's rules (owner: H1; HF sec.2.2, 9.3):
 * catalog access, footprints, the placement rules and the "nope" reasons,
 * stacking smalls on surfaces, wall pieces, the fixed slots (door,
 * windows, the hearth and Kevin's rule), and the 32-step undo ring of an
 * editing session. No drawing here (home_draw.c, H4).
 */
#include "home_int.h"

/* ---- the catalog --------------------------------------------------------------- */
static const DoomoFurn *test_rows;
static int test_n;

void home_test_catalog(const DoomoFurn *rows, int n)
{
    test_rows = rows;
    test_n = rows ? n : 0;
}

int home_furn_count(void)
{
    int n = test_rows ? test_n : (int)doomo_furn_count;
    return n > FURN_MAX ? FURN_MAX : n;
}

const DoomoFurn *home_furn(int furn)
{
    if (furn < 0 || furn >= home_furn_count()) return NULL;
    return test_rows ? &test_rows[furn] : &doomo_furn[furn];
}

int home_furn_by_role(int role)
{
    int i, n = home_furn_count();
    for (i = 0; i < n; i++)
        if (home_furn(i)->role == role) return i;
    return -1;
}

const char *const home_nope_text[HPR_COUNT] = {
    "", "Off the grid.", "Something's already there.", "That goes on a wall.",
    "That goes on the floor.", "The chimney has to breathe.", "That one stays put.",
    "The room is full.", "You don't have one spare.", "Only fits a window slot.",
    "Remove only. For now.", "Keep the door clear."
};

/* ---- footprints ------------------------------------------------------------------ */
void home_footprint(const DoomoFurn *f, int rot, int *w, int *d)
{
    int fw = f && f->fw ? f->fw : 1, fd = f && f->fd ? f->fd : 1;
    if (rot & 1) { *w = fd; *d = fw; } else { *w = fw; *d = fd; }
}

typedef struct Box { int x0, y0, x1, y1; } Box;   /* tiles, x1/y1 exclusive */

static Box obj_box(const HomeObj *o)
{
    Box b;
    int w, d;
    home_footprint(home_furn(HO_FURN(o)), HO_ROT(o), &w, &d);
    b.x0 = HO_X(o); b.y0 = HO_Y(o); b.x1 = b.x0 + w; b.y1 = b.y0 + d;
    return b;
}

static int overlap(Box a, Box b)
{
    return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
}

/* What layer an object lives in. */
enum { LY_RUG, LY_FLOOR, LY_STACK, LY_WALL };
static int obj_layer(const HomeObj *o)
{
    const DoomoFurn *f = home_furn(HO_FURN(o));
    if (!f) return LY_FLOOR;
    if (f->flags & FF_WALL) return LY_WALL;
    if (f->flags & FF_RUG) return LY_RUG;
    if (HO_ONSURF(o)) return LY_STACK;
    return LY_FLOOR;
}

/* Wall pieces: rot even = wall A (y = 0), odd = wall B (x = 0); they span
 * fw tiles along the wall and fh*4 px from z*4. */
static int wall_z_default(const DoomoFurn *f)
{
    int h = f->fh * 4;
    if (f->role == FR_WINDOW) return 6;
    if (h >= 40) return 0;
    return ((64 - h) / 2) / 4;
}

static int wall_span(const HomeObj *o, int *a0, int *a1, int *z0, int *z1)
{
    const DoomoFurn *f = home_furn(HO_FURN(o));
    int along = (HO_ROT(o) & 1) ? HO_Y(o) : HO_X(o);
    *a0 = along;
    *a1 = along + (f && f->fw ? f->fw : 1);
    *z0 = o->z * 4;
    *z1 = *z0 + (f ? f->fh * 4 : 16);
    return HO_ROT(o) & 1;
}

int home_obj_is_fixed(const HomeRoom *r, int idx)
{
    const DoomoFurn *f;
    if (idx < 0 || idx >= r->nobj) return 0;
    f = home_furn(HO_FURN(&r->obj[idx]));
    return f && (f->flags2 & (FF_PORTAL | FF_FIXED));
}

int home_stackers_on(const HomeRoom *r, int idx)
{
    int i, n = 0;
    Box b;
    if (idx < 0 || idx >= r->nobj || obj_layer(&r->obj[idx]) != LY_FLOOR) return 0;
    b = obj_box(&r->obj[idx]);
    for (i = 0; i < r->nobj; i++) {
        const HomeObj *o = &r->obj[i];
        if (i != idx && HO_ONSURF(o) && HO_X(o) >= b.x0 && HO_X(o) < b.x1 &&
            HO_Y(o) >= b.y0 && HO_Y(o) < b.y1) n++;
    }
    return n;
}

int home_obj_at(const HomeRoom *r, int x, int y)
{
    int i, best = -1, rank = 0;
    for (i = 0; i < r->nobj; i++) {
        const HomeObj *o = &r->obj[i];
        int ly = obj_layer(o), k = 0;
        if (ly == LY_WALL) {
            int a0, a1, z0, z1, wb = wall_span(o, &a0, &a1, &z0, &z1);
            int along = wb ? y : x, edge = wb ? x : y;
            if (edge == 0 && along >= a0 && along < a1) k = 2;
        } else {
            Box b = obj_box(o);
            if (x >= b.x0 && x < b.x1 && y >= b.y0 && y < b.y1)
                k = ly == LY_STACK ? 5 : ly == LY_FLOOR ? 4 : 1;
        }
        if (k > rank) { rank = k; best = i; }
    }
    return best;
}

/* ---- the rules (HF sec.2.2) -------------------------------------------------------- */
int home_fit(const HomeRoom *r, int room, int furn, int x, int y, int rot,
             int ignore, HomeFit *out)
{
    return home_fit_sub(r, room, furn, x, y, rot, 0, ignore, out);
}

int home_fit_sub(const HomeRoom *r, int room, int furn, int x, int y, int rot, int sub,
                 int ignore, HomeFit *out)
{
    const DoomoFurn *f = home_furn(furn);
    const HomeRoomDef *def = &home_room_defs[(unsigned)room < HOME_ROOM_COUNT ? room : 0];
    HomeFit tmp;
    int W = HR_W(r), D = HR_D(r), i, w, d;
    Box b;
    if (!out) out = &tmp;
    out->z = 0; out->on_surface = 0; out->surface = -1; out->replaces = -1;
    if (!f) return HPR_OFFGRID;
    if (f->flags2 & FF_FIXED) return HPR_FIXED;      /* decals: only the house places them */
    rot &= 3;
    /* fixed slots: windows swap into a window slot, doors and the hearth
     * never move */
    if (f->flags2 & FF_PORTAL) {
        int k;
        if (f->role != FR_WINDOW) return HPR_FIXED;
        for (k = 0; k < def->nwin; k++) {
            int wb = def->win_wall[k] == HW_B;
            if ((rot & 1) == wb && (wb ? (x == 0 && y == def->win_at[k]) : (y == 0 && x == def->win_at[k]))) {
                for (i = 0; i < r->nobj; i++) {
                    const DoomoFurn *g = home_furn(HO_FURN(&r->obj[i]));
                    if (i != ignore && g && g->role == FR_WINDOW && r->obj[i].pos == HO_POS(x, y) &&
                        (HO_ROT(&r->obj[i]) & 1) == wb)
                        out->replaces = (int8_t)i;
                }
                out->z = (uint8_t)wall_z_default(f);
                return HPR_OK;
            }
        }
        return HPR_SLOT_ONLY;
    }
    /* Ch3's Shelter pieces (HF sec.10.3): a welded plate snaps over a window
     * slot that has a window and no plate yet; the toaster turret stands on
     * the floor tile in front of a window slot */
    if (f->role == FR_PLATE || f->role == FR_TURRET) {
        int k, slot = -1;
        for (k = 0; k < def->nwin; k++) {
            int wb = def->win_wall[k] == HW_B;
            int sx = wb ? 0 : def->win_at[k], sy = wb ? def->win_at[k] : 0;
            if (x == sx && y == sy && (f->role == FR_TURRET || (rot & 1) == wb)) slot = k;
        }
        if (slot < 0) return HPR_SLOT_ONLY;
        if (f->role == FR_PLATE) {
            int wb = def->win_wall[slot] == HW_B, win = -1;
            for (i = 0; i < r->nobj; i++) {
                const DoomoFurn *g = home_furn(HO_FURN(&r->obj[i]));
                if (i == ignore || !g || r->obj[i].pos != HO_POS(x, y) || (HO_ROT(&r->obj[i]) & 1) != wb) continue;
                if (g->role == FR_PLATE) return HPR_OCCUPIED;
                if (g->role == FR_WINDOW) win = i;
            }
            if (win < 0) return HPR_SLOT_ONLY;
            out->z = r->obj[win].z;
            return HPR_OK;
        }
    }
    if (f->flags & FF_WALL) {
        int wb = rot & 1, a0 = wb ? y : x, a1 = a0 + (f->fw ? f->fw : 1);
        int z0 = wall_z_default(f) * 4, z1 = z0 + f->fh * 4;
        if (def->kind == ROOMK_GARDEN && f->role != FR_FENCE) return HPR_FLOOR_ONLY;
        if ((wb ? x : y) != 0) return HPR_WALL_ONLY;
        if (a0 < 0 || a1 > (wb ? D : W)) return HPR_OFFGRID;
        for (i = 0; i < r->nobj; i++) {
            int b0, b1, c0, c1;
            if (i == ignore || obj_layer(&r->obj[i]) != LY_WALL) continue;
            if (wall_span(&r->obj[i], &b0, &b1, &c0, &c1) != wb) continue;
            if (a0 < b1 && b0 < a1 && z0 < c1 && c0 < z1) return HPR_OCCUPIED;
        }
        /* the hearth stands against wall B: nothing hangs over its mouth */
        if (wb && def->hearth_at != 0xFF && a0 < def->hearth_at + 2 && def->hearth_at < a1 && z0 < 48)
            return HPR_FIREPLACE;
        out->z = (uint8_t)(z0 / 4);
        return HPR_OK;
    }
    home_footprint(f, rot, &w, &d);
    b.x0 = x; b.y0 = y; b.x1 = x + w; b.y1 = y + d;
    if (x < 0 || y < 0 || b.x1 > W || b.y1 > D) return HPR_OFFGRID;
    /* a small piece on a surface stacks, snapping to half tiles (HF sec.2.2) */
    if (f->flags & FF_SMALL) {
        for (i = 0; i < r->nobj; i++) {
            const HomeObj *o = &r->obj[i];
            const DoomoFurn *g = home_furn(HO_FURN(o));
            Box c;
            if (i == ignore || obj_layer(o) != LY_FLOOR || !g || !(g->flags & FF_SURFACE)) continue;
            c = obj_box(o);
            if (x >= c.x0 && x < c.x1 && y >= c.y0 && y < c.y1) {
                int j;
                for (j = 0; j < r->nobj; j++)
                    if (j != ignore && HO_ONSURF(&r->obj[j]) && r->obj[j].pos == HO_POS(x, y) &&
                        HO_SUB(&r->obj[j]) == (sub & 3))
                        return HPR_OCCUPIED;
                out->on_surface = 1;
                out->surface = (int8_t)i;
                out->z = g->fh;
                return HPR_OK;
            }
        }
    }
    for (i = 0; i < r->nobj; i++) {
        const HomeObj *o = &r->obj[i];
        int ly = obj_layer(o);
        if (i == ignore || ly == LY_WALL || ly == LY_STACK) continue;
        if ((ly == LY_RUG) != ((f->flags & FF_RUG) != 0)) continue;   /* rugs live under */
        if (overlap(b, obj_box(o))) return HPR_OCCUPIED;
    }
    if (!(f->flags & FF_RUG)) {
        /* Kevin's rule: nothing blocks the fireplace (the two tiles in front) */
        if (def->hearth_at != 0xFF) {
            Box h;
            h.x0 = 1; h.x1 = 2; h.y0 = def->hearth_at; h.y1 = def->hearth_at + 2;
            if (overlap(b, h)) return HPR_FIREPLACE;
        }
        /* the door's own tile stays clear */
        {
            Box dr;
            if (def->door_wall == HW_A) { dr.x0 = def->door_at; dr.y0 = 0; }
            else { dr.x0 = 0; dr.y0 = def->door_at; }
            dr.x1 = dr.x0 + 1; dr.y1 = dr.y0 + 1;
            if (overlap(b, dr)) return HPR_DOOR;
        }
    }
    return HPR_OK;
}

/* ---- the editing session ---------------------------------------------------------- */
void home_edit_begin(HomeEdit *e, int room)
{
    memset(e, 0, sizeof(*e));
    if (room < 0 || room >= HOME_ROOM_COUNT) room = 0;
    e->room_id = (uint8_t)room;
    home_room_load(room, &e->room);
    e->remove_only = (uint8_t)(home_remove_only_on() || home_phase() == HP_EMPTY);
    e->gen = 1;
}

static void obj_bytes(const HomeObj *o, uint8_t b[6])
{
    b[0] = (uint8_t)(o->w0 & 0xFF); b[1] = (uint8_t)(o->w0 >> 8);
    b[2] = o->pos; b[3] = o->z; b[4] = o->pa; b[5] = o->pb;
}

static void bytes_obj(const uint8_t b[6], HomeObj *o)
{
    o->w0 = (uint16_t)(b[0] | (b[1] << 8));
    o->pos = b[2]; o->z = b[3]; o->pa = b[4]; o->pb = b[5];
}

static void push_undo(HomeEdit *e, int op, int idx, const HomeObj *old)
{
    HomeUndo *u = &e->undo[e->undo_head];
    u->op = (uint8_t)op;
    u->idx = (uint8_t)idx;
    if (old) obj_bytes(old, u->old); else memset(u->old, 0, sizeof(u->old));
    e->undo_head = (uint8_t)((e->undo_head + 1) % HOME_UNDO_MAX);
    if (e->undo_n < HOME_UNDO_MAX) e->undo_n++;
}

int home_edit_avail(const HomeEdit *e, int furn)
{
    const DoomoFurn *f = home_furn(furn);
    if (!f) return 0;
    if (!home_state()) {          /* no house state (GBA before G0): a showroom */
        int i, n = 0;
        for (i = 0; i < e->room.nobj; i++) if (HO_FURN(&e->room.obj[i]) == furn) n++;
        return n < 1 ? 1 : 0;
    }
    return home_owned(furn) - home_placed(furn, &e->room, e->room_id);
}

int home_edit_place(HomeEdit *e, int furn, int x, int y, int rot, int pa, int pb)
{
    return home_edit_place_sub(e, furn, x, y, rot, 0, pa, pb);
}

int home_edit_place_sub(HomeEdit *e, int furn, int x, int y, int rot, int sub, int pa, int pb)
{
    HomeFit fit;
    HomeObj o;
    int rc;
    if (e->remove_only) return -HPR_REMOVE_ONLY;
    rc = home_fit_sub(&e->room, e->room_id, furn, x, y, rot, sub, -1, &fit);
    if (rc != HPR_OK) return -rc;
    if (fit.replaces < 0 && e->room.nobj >= HOME_OBJ_MAX) return -HPR_FULL;
    if (home_edit_avail(e, furn) <= 0) return -HPR_NOT_OWNED;
    o.w0 = HO_W0S(furn, rot, fit.on_surface, fit.on_surface ? sub : 0);
    o.pos = HO_POS(x, y);
    o.z = fit.z;
    o.pa = (uint8_t)(pa < 0 ? FURN_PAINT_DEFAULT : (pa & 0x1F));
    o.pb = (uint8_t)(pb < 0 ? FURN_PAINT_DEFAULT : (pb & 0x1F));
    e->gen++;
    if (fit.replaces >= 0) {
        push_undo(e, HU_MOVE, fit.replaces, &e->room.obj[(int)fit.replaces]);
        e->room.obj[(int)fit.replaces] = o;
        return fit.replaces;
    }
    e->room.obj[e->room.nobj] = o;
    push_undo(e, HU_PLACE, e->room.nobj, NULL);
    return e->room.nobj++;
}

static void remove_at(HomeRoom *r, int idx)
{
    int i;
    for (i = idx; i + 1 < r->nobj; i++) r->obj[i] = r->obj[i + 1];
    r->nobj--;
    memset(&r->obj[r->nobj], 0, sizeof(r->obj[0]));   /* no stale bytes past nobj */
}

int home_edit_pickup(HomeEdit *e, int idx)
{
    int furn;
    if (idx < 0 || idx >= e->room.nobj) return -HPR_OFFGRID;
    if (home_obj_is_fixed(&e->room, idx)) return -HPR_FIXED;
    if (home_stackers_on(&e->room, idx)) return -HPR_OCCUPIED;
    furn = HO_FURN(&e->room.obj[idx]);
    push_undo(e, HU_REMOVE, idx, &e->room.obj[idx]);
    remove_at(&e->room, idx);
    e->gen++;
    return furn;
}

int home_edit_move(HomeEdit *e, int idx, int x, int y, int rot)
{
    HomeFit fit;
    HomeObj *o;
    int rc;
    if (e->remove_only) return -HPR_REMOVE_ONLY;
    if (idx < 0 || idx >= e->room.nobj) return -HPR_OFFGRID;
    if (home_obj_is_fixed(&e->room, idx)) return -HPR_FIXED;
    if (home_stackers_on(&e->room, idx)) return -HPR_OCCUPIED;
    o = &e->room.obj[idx];
    rc = home_fit(&e->room, e->room_id, HO_FURN(o), x, y, rot, idx, &fit);
    if (rc != HPR_OK) return -rc;
    push_undo(e, HU_MOVE, idx, o);
    o->w0 = HO_W0(HO_FURN(o), rot, fit.on_surface);
    o->pos = HO_POS(x, y);
    o->z = fit.z;
    e->gen++;
    return idx;
}

static void undo_one(HomeEdit *e);

int home_edit_undo(HomeEdit *e)
{
    HomeUndo *u;
    int n;
    if (!e->undo_n) return 0;
    u = &e->undo[(e->undo_head + HOME_UNDO_MAX - 1) % HOME_UNDO_MAX];
    if (u->op == HU_GROUP) {
        /* a group step: the header, then its entries, undone as one */
        n = u->idx;
        e->undo_head = (uint8_t)((e->undo_head + HOME_UNDO_MAX - 1) % HOME_UNDO_MAX);
        e->undo_n--;
        u->op = HU_NONE;
        while (n-- > 0 && e->undo_n) undo_one(e);
        e->gen++;
        return 1;
    }
    undo_one(e);
    e->gen++;
    return 1;
}

static void mask_reset(HomeEdit *e, const HomeUndo *u, int slot)
{
    uint64_t m = (uint64_t)u->old[0] | ((uint64_t)u->old[1] << 8) | ((uint64_t)u->old[2] << 16) |
                 ((uint64_t)u->old[3] << 24) | ((uint64_t)u->idx << 32);
    int i;
    for (i = 0; i < e->room.nobj && i < 40; i++)
        if ((m >> i) & 1) {
            uint8_t *p = slot ? &e->room.obj[i].pb : &e->room.obj[i].pa;
            *p = (uint8_t)((*p & 0xE0) | FURN_PAINT_DEFAULT);
        }
}

static void undo_one(HomeEdit *e)
{
    HomeUndo *u;
    int i;
    e->undo_head = (uint8_t)((e->undo_head + HOME_UNDO_MAX - 1) % HOME_UNDO_MAX);
    e->undo_n--;
    u = &e->undo[e->undo_head];
    switch (u->op) {
    case HU_PLACE:
        if (u->idx < e->room.nobj) remove_at(&e->room, u->idx);
        break;
    case HU_REMOVE:
        if (e->room.nobj < HOME_OBJ_MAX && u->idx <= e->room.nobj) {
            for (i = e->room.nobj; i > u->idx; i--) e->room.obj[i] = e->room.obj[i - 1];
            bytes_obj(u->old, &e->room.obj[u->idx]);
            e->room.nobj++;
        }
        break;
    case HU_MOVE:
        if (u->idx < e->room.nobj) bytes_obj(u->old, &e->room.obj[u->idx]);
        break;
    case HU_WALL:
        e->room.wallpaper = u->old[0]; e->room.floor = u->old[1];
        e->room.wall_paint = u->old[2]; e->room.floor_paint = u->old[3];
        e->room.flags = u->old[4];
        break;
    case HU_TMASK_A: mask_reset(e, u, 0); break;
    case HU_TMASK_B: mask_reset(e, u, 1); break;
    default:
        break;
    }
    u->op = HU_NONE;
}

/* ---- H1 M3: recolours, walls, themes (HF sec.2.3) ------------------------------- */
static void push_group(HomeEdit *e, int n)
{
    HomeUndo *u = &e->undo[e->undo_head];
    u->op = HU_GROUP;
    u->idx = (uint8_t)n;
    memset(u->old, 0, sizeof(u->old));
    e->undo_head = (uint8_t)((e->undo_head + 1) % HOME_UNDO_MAX);
    if (e->undo_n < HOME_UNDO_MAX) e->undo_n++;
}

static void push_wall(HomeEdit *e)
{
    HomeUndo *u = &e->undo[e->undo_head];
    u->op = HU_WALL;
    u->idx = 0;
    u->old[0] = e->room.wallpaper; u->old[1] = e->room.floor;
    u->old[2] = e->room.wall_paint; u->old[3] = e->room.floor_paint;
    u->old[4] = e->room.flags; u->old[5] = 0;
    e->undo_head = (uint8_t)((e->undo_head + 1) % HOME_UNDO_MAX);
    if (e->undo_n < HOME_UNDO_MAX) e->undo_n++;
}

static void push_mask(HomeEdit *e, int op, uint64_t m)
{
    HomeUndo *u = &e->undo[e->undo_head];
    u->op = (uint8_t)op;
    u->idx = (uint8_t)(m >> 32);
    u->old[0] = (uint8_t)m; u->old[1] = (uint8_t)(m >> 8);
    u->old[2] = (uint8_t)(m >> 16); u->old[3] = (uint8_t)(m >> 24);
    u->old[4] = u->old[5] = 0;
    e->undo_head = (uint8_t)((e->undo_head + 1) % HOME_UNDO_MAX);
    if (e->undo_n < HOME_UNDO_MAX) e->undo_n++;
}

int home_edit_paint(HomeEdit *e, int idx, int pa, int pb)
{
    HomeObj *o;
    const DoomoFurn *f;
    if (idx < 0 || idx >= e->room.nobj) return 0;
    o = &e->room.obj[idx];
    f = home_furn(HO_FURN(o));
    if (!f || (f->paint_a == 0xFF && f->paint_b == 0xFF)) return 0;
    push_undo(e, HU_MOVE, idx, o);
    if (f->paint_a != 0xFF) o->pa = (uint8_t)((o->pa & 0xE0) | (pa < 0 ? FURN_PAINT_DEFAULT : (pa & 0x1F)));
    if (f->paint_b != 0xFF) o->pb = (uint8_t)((o->pb & 0xE0) | (pb < 0 ? FURN_PAINT_DEFAULT : (pb & 0x1F)));
    e->gen++;
    return 1;
}

int home_edit_walls(HomeEdit *e, int wallpaper, int floor, int wall_paint, int floor_paint)
{
    if (e->remove_only) return 0;
    push_wall(e);
    if (wallpaper >= 0) e->room.wallpaper = (uint8_t)(wallpaper & 31);
    if (floor >= 0) e->room.floor = (uint8_t)(floor & 31);
    if (wall_paint >= 0) e->room.wall_paint = (uint8_t)(wall_paint & 31);
    if (floor_paint >= 0) e->room.floor_paint = (uint8_t)(floor_paint & 31);
    e->gen++;
    return 1;
}

/* Apply theme: walls, floor, the ceiling tint, and every free (still
 * catalog-default) paint slot recoloured to the theme's two paints. One
 * undo step: the walls, the two recolour masks and the group header. */
int home_edit_theme(HomeEdit *e, int theme)
{
    const DoomoHomeTheme *t;
    uint64_t ma = 0, mb = 0;
    int i;
    HomeState *s = home_state();
    if (theme < 0 || theme >= THEME_COUNT || e->remove_only) return 0;
    t = &doomo_home_themes[theme];
    push_wall(e);
    e->room.wallpaper = t->wallpaper;
    e->room.floor = t->floor;
    e->room.wall_paint = t->wall_paint;
    e->room.floor_paint = t->floor_paint;
    e->room.flags = (uint8_t)((e->room.flags & ~HRF_CEIL_MASK) | ((t->ceiling & 7) << HRF_CEIL_SHIFT));
    for (i = 0; i < e->room.nobj && i < 40; i++) {
        HomeObj *o = &e->room.obj[i];
        const DoomoFurn *f = home_furn(HO_FURN(o));
        if (!f || (f->flags2 & FF_PORTAL)) continue;
        if (f->paint_a != 0xFF && (o->pa & 0x1F) == FURN_PAINT_DEFAULT) {
            o->pa = (uint8_t)((o->pa & 0xE0) | t->paint_a);
            ma |= (uint64_t)1 << i;
        }
        if (f->paint_b != 0xFF && (o->pb & 0x1F) == FURN_PAINT_DEFAULT) {
            o->pb = (uint8_t)((o->pb & 0xE0) | t->paint_b);
            mb |= (uint64_t)1 << i;
        }
    }
    push_mask(e, HU_TMASK_A, ma);
    push_mask(e, HU_TMASK_B, mb);
    push_group(e, 3);
    if (s) s->theme = (uint8_t)theme;
    e->gen++;
    return 1;
}

int home_edit_lights(HomeEdit *e, int idx)
{
    const DoomoFurn *f;
    if (idx < 0 || idx >= e->room.nobj) return 0;
    f = home_furn(HO_FURN(&e->room.obj[idx]));
    if (!f || !(f->flags & FF_LIGHT)) return 0;
    e->room.obj[idx].pa ^= HOS_OFF;
    e->gen++;
    return 1;
}

/* ---- H1 M3: multi-select, group moves, the clipboard (HF sec.2.1) ----------------- */
static int stacked_on(const HomeRoom *r, int small, int surf)
{
    const HomeObj *o = &r->obj[small], *s = &r->obj[surf];
    int w, d;
    if (!HO_ONSURF(o) || HO_ONSURF(s) || obj_layer(s) != LY_FLOOR) return 0;
    home_footprint(home_furn(HO_FURN(s)), HO_ROT(s), &w, &d);
    return HO_X(o) >= HO_X(s) && HO_X(o) < HO_X(s) + w && HO_Y(o) >= HO_Y(s) && HO_Y(o) < HO_Y(s) + d;
}

int home_edit_select(const HomeEdit *e, int x0, int y0, int x1, int y1, uint8_t *out, int cap)
{
    const HomeRoom *r = &e->room;
    int i, j, n = 0, t;
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
    if (cap > HOME_SEL_MAX) cap = HOME_SEL_MAX;
    /* the pieces first (floor, rugs, walls), their smalls after them */
    for (i = 0; i < r->nobj && n < cap; i++) {
        Box b;
        if (home_obj_is_fixed(r, i) || HO_ONSURF(&r->obj[i])) continue;
        if (obj_layer(&r->obj[i]) == LY_WALL) {
            int a0, a1, z0, z1, wb = wall_span(&r->obj[i], &a0, &a1, &z0, &z1);
            b.x0 = wb ? 0 : a0; b.x1 = wb ? 1 : a1; b.y0 = wb ? a0 : 0; b.y1 = wb ? a1 : 1;
        } else {
            b = obj_box(&r->obj[i]);
        }
        if (b.x0 < x0 || b.y0 < y0 || b.x1 - 1 > x1 || b.y1 - 1 > y1) continue;
        out[n++] = (uint8_t)i;
    }
    for (i = 0; i < r->nobj && n < cap; i++) {
        int in = 0;
        if (!HO_ONSURF(&r->obj[i])) continue;
        for (j = 0; j < n && !in; j++) in = stacked_on(r, i, out[j]);
        if (!in && (HO_X(&r->obj[i]) < x0 || HO_X(&r->obj[i]) > x1 || HO_Y(&r->obj[i]) < y0 || HO_Y(&r->obj[i]) > y1))
            continue;
        out[n++] = (uint8_t)i;
    }
    return n;
}

/* Try a new place for every piece of a group: the room without the group,
 * then the group back in, surfaces before their smalls. pos[] are the new
 * (x, y, rot) per piece; 1 = all fit (the fits are in fit[]). */
static int group_fits(const HomeEdit *e, const uint8_t *idx, int n, const uint8_t (*pos)[3], HomeFit *fit)
{
    HomeRoom tmp;
    int i, k, pass, rc;
    uint8_t in[HOME_OBJ_MAX];
    memset(in, 0, sizeof(in));
    for (k = 0; k < n; k++) in[idx[k]] = 1;
    tmp = e->room;
    tmp.nobj = 0;
    for (i = 0; i < e->room.nobj; i++) if (!in[i]) tmp.obj[tmp.nobj++] = e->room.obj[i];
    for (pass = 0; pass < 2; pass++)
        for (k = 0; k < n; k++) {
            const HomeObj *o = &e->room.obj[idx[k]];
            HomeObj *t;
            if ((pass == 0) == (HO_ONSURF(o) != 0)) continue;
            rc = home_fit_sub(&tmp, e->room_id, HO_FURN(o), pos[k][0], pos[k][1], pos[k][2],
                              HO_ONSURF(o) ? HO_SUB(o) : 0, -1, &fit[k]);
            if (rc != HPR_OK) return -rc;
            if (pass == 1 && !fit[k].on_surface && HO_ONSURF(o)) return -HPR_OCCUPIED;
            t = &tmp.obj[tmp.nobj++];
            *t = *o;
            t->w0 = HO_W0S(HO_FURN(o), pos[k][2], fit[k].on_surface, fit[k].on_surface ? HO_SUB(o) : 0);
            t->pos = HO_POS(pos[k][0], pos[k][1]);
            t->z = fit[k].z;
        }
    return 1;
}

static int group_commit(HomeEdit *e, const uint8_t *idx, int n, const uint8_t (*pos)[3], const HomeFit *fit)
{
    int k;
    for (k = 0; k < n; k++) {
        HomeObj *o = &e->room.obj[idx[k]];
        push_undo(e, HU_MOVE, idx[k], o);
        o->w0 = HO_W0S(HO_FURN(o), pos[k][2], fit[k].on_surface, fit[k].on_surface ? HO_SUB(o) : 0);
        o->pos = HO_POS(pos[k][0], pos[k][1]);
        o->z = fit[k].z;
    }
    push_group(e, n);
    e->gen++;
    return n;
}

int home_edit_group_move(HomeEdit *e, const uint8_t *idx, int n, int dx, int dy)
{
    uint8_t pos[HOME_SEL_MAX][3];
    HomeFit fit[HOME_SEL_MAX];
    int k, rc;
    if (e->remove_only) return -HPR_REMOVE_ONLY;
    if (n <= 0 || n > HOME_SEL_MAX) return 0;
    for (k = 0; k < n; k++) {
        const HomeObj *o = &e->room.obj[idx[k]];
        int x = HO_X(o) + dx, y = HO_Y(o) + dy;
        if (obj_layer(o) == LY_WALL) {         /* wall pieces slide along their wall */
            if (HO_ROT(o) & 1) x = 0; else y = 0;
        }
        if (x < 0 || y < 0 || x > 15 || y > 15) return -HPR_OFFGRID;
        pos[k][0] = (uint8_t)x; pos[k][1] = (uint8_t)y; pos[k][2] = (uint8_t)HO_ROT(o);
    }
    rc = group_fits(e, idx, n, (const uint8_t (*)[3])pos, fit);
    if (rc != 1) return rc;
    return group_commit(e, idx, n, (const uint8_t (*)[3])pos, fit);
}

/* Rotate a group a quarter turn about its bounding box's centre (in half
 * tiles); wall pieces stay where they hang. */
int home_edit_group_rotate(HomeEdit *e, const uint8_t *idx, int n, int dir)
{
    uint8_t pos[HOME_SEL_MAX][3];
    HomeFit fit[HOME_SEL_MAX];
    int k, rc, bx0 = 99, by0 = 99, bx1 = -1, by1 = -1, cx2, cy2;
    if (e->remove_only) return -HPR_REMOVE_ONLY;
    if (n <= 0 || n > HOME_SEL_MAX) return 0;
    for (k = 0; k < n; k++) {
        Box b = obj_box(&e->room.obj[idx[k]]);
        if (obj_layer(&e->room.obj[idx[k]]) == LY_WALL) continue;
        if (b.x0 < bx0) bx0 = b.x0;
        if (b.y0 < by0) by0 = b.y0;
        if (b.x1 > bx1) bx1 = b.x1;
        if (b.y1 > by1) by1 = b.y1;
    }
    cx2 = bx0 + bx1;                            /* twice the centre */
    cy2 = by0 + by1;
    for (k = 0; k < n; k++) {
        const HomeObj *o = &e->room.obj[idx[k]];
        Box b = obj_box(o);
        int nx0, ny0, nx1, ny1, t;
        if (obj_layer(o) == LY_WALL) {
            pos[k][0] = (uint8_t)HO_X(o); pos[k][1] = (uint8_t)HO_Y(o); pos[k][2] = (uint8_t)HO_ROT(o);
            continue;
        }
        /* rotate the box's corners (doubled coordinates) about the centre */
        if (dir > 0) {
            nx0 = cx2 - (2 * b.y1 - cy2); nx1 = cx2 - (2 * b.y0 - cy2);
            ny0 = cy2 + (2 * b.x0 - cx2); ny1 = cy2 + (2 * b.x1 - cx2);
        } else {
            nx0 = cx2 + (2 * b.y0 - cy2); nx1 = cx2 + (2 * b.y1 - cy2);
            ny0 = cy2 - (2 * b.x1 - cx2); ny1 = cy2 - (2 * b.x0 - cx2);
        }
        if (nx1 < nx0) { t = nx0; nx0 = nx1; nx1 = t; }
        if (ny1 < ny0) { t = ny0; ny0 = ny1; ny1 = t; }
        nx0 /= 2; ny0 /= 2;                     /* an odd centre lands on the lower tile */
        if (nx0 < 0 || ny0 < 0) return -HPR_OFFGRID;
        pos[k][0] = (uint8_t)nx0; pos[k][1] = (uint8_t)ny0;
        pos[k][2] = (uint8_t)((HO_ROT(o) + (dir > 0 ? 1 : 3)) & 3);
    }
    rc = group_fits(e, idx, n, (const uint8_t (*)[3])pos, fit);
    if (rc != 1) return rc;
    return group_commit(e, idx, n, (const uint8_t (*)[3])pos, fit);
}

int home_edit_group_remove(HomeEdit *e, const uint8_t *idx, int n)
{
    uint8_t order[HOME_SEL_MAX];
    int k, j, t, done = 0;
    if (n <= 0 || n > HOME_SEL_MAX) return 0;
    memcpy(order, idx, (size_t)n);
    /* highest index first, so the undo (lowest first) puts each back in place */
    for (k = 0; k < n; k++)
        for (j = k + 1; j < n; j++)
            if (order[j] > order[k]) { t = order[k]; order[k] = order[j]; order[j] = (uint8_t)t; }
    for (k = 0; k < n; k++) {
        if (order[k] >= e->room.nobj || home_obj_is_fixed(&e->room, order[k])) continue;
        push_undo(e, HU_REMOVE, order[k], &e->room.obj[order[k]]);
        remove_at(&e->room, order[k]);
        done++;
    }
    if (done) { push_group(e, done); e->gen++; }
    return done;
}

int home_edit_copy(const HomeEdit *e, const uint8_t *idx, int n, HomeClip *c)
{
    int k, x0 = 99, y0 = 99;
    memset(c, 0, sizeof(*c));
    if (n > HOME_SEL_MAX) n = HOME_SEL_MAX;
    for (k = 0; k < n; k++) {
        const HomeObj *o = &e->room.obj[idx[k]];
        if (HO_X(o) < x0) x0 = HO_X(o);
        if (HO_Y(o) < y0) y0 = HO_Y(o);
    }
    for (k = 0; k < n; k++) {
        HomeObj o = e->room.obj[idx[k]];
        o.pos = HO_POS(HO_X(&o) - x0, HO_Y(&o) - y0);
        c->o[c->n++] = o;
    }
    return c->n;
}

/* Paste the clipboard with its corner at (x, y): only spares you own are
 * placed (surfaces first), one undo step. */
int home_edit_paste(HomeEdit *e, const HomeClip *c, int x, int y)
{
    int k, pass, placed = 0;
    if (e->remove_only) return 0;
    for (pass = 0; pass < 2; pass++)
        for (k = 0; k < c->n; k++) {
            const HomeObj *o = &c->o[k];
            const DoomoFurn *f = home_furn(HO_FURN(o));
            int px = x + HO_X(o), py = y + HO_Y(o), r;
            if ((pass == 0) == (HO_ONSURF(o) != 0)) continue;
            if (f && (f->flags & FF_WALL)) { if (HO_ROT(o) & 1) px = 0; else py = 0; }
            r = home_edit_place_sub(e, HO_FURN(o), px, py, HO_ROT(o), HO_SUB(o),
                                    (o->pa & 0x1F) == FURN_PAINT_DEFAULT ? -1 : (o->pa & 0x1F),
                                    (o->pb & 0x1F) == FURN_PAINT_DEFAULT ? -1 : (o->pb & 0x1F));
            if (r >= 0) placed++;
        }
    if (placed > 1) push_group(e, placed);
    return placed;
}

/* ---- H1 M3: blueprints (HF sec.2.3: 12 in ROM, 2 saved) ------------------------- */
int home_blueprint_count(void) { return (int)doomo_home_blueprint_count + HOME_BLUEPRINTS; }

const char *home_blueprint_name(int bp)
{
    HomeState *s = home_state();
    if (bp >= 0 && bp < (int)doomo_home_blueprint_count) return doomo_home_blueprints[bp].name;
    bp -= (int)doomo_home_blueprint_count;
    if (bp < 0 || bp >= HOME_BLUEPRINTS || !s || !s->bp[bp].nobj) return NULL;
    /* --- H4: no table of pointers here: on the PS1 a two-pointer table is
     * .sdata (-G8), stays resident and pins its strings out of the home
     * overlay group (ovl_plan.py) --- */
    return bp ? "My Blueprint 2" : "My Blueprint 1";
    /* --- end H4 --- */
}

int home_blueprint_save(int slot, const HomeRoom *r)
{
    HomeState *s = home_state();
    int i;
    if (!s || slot < 0 || slot >= HOME_BLUEPRINTS) return 0;
    s->bp[slot] = *r;
    s->bp[slot].nobj = 0;
    for (i = 0; i < r->nobj; i++)
        if (!home_obj_is_fixed(r, i)) s->bp[slot].obj[s->bp[slot].nobj++] = r->obj[i];
    s->ver_flags |= HVF_TOUCHED;
    return s->bp[slot].nobj > 0;
}

static void ghost(HomePlan *p, int furn, int x, int y, int rot, int status)
{
    HomeGhost *g;
    if (status == HPS_MISSING) p->missing++; else p->blocked++;
    if (p->nghost >= HOME_OBJ_MAX) return;
    g = &p->ghost[p->nghost++];
    g->furn = (uint16_t)furn; g->x = (uint8_t)x; g->y = (uint8_t)y; g->rot = (uint8_t)rot; g->status = (uint8_t)status;
}

static void plan_piece(HomeEdit *e, HomePlan *p, int furn, int x, int y, int rot, int sub, int pa, int pb)
{
    const DoomoFurn *f = home_furn(furn);
    int r;
    if (!f) return;
    if (home_edit_avail(e, furn) <= 0 && !((f->flags2 & FF_PORTAL) && f->role == FR_WINDOW && home_owned(furn))) {
        ghost(p, furn, x, y, rot, HPS_MISSING);
        return;
    }
    r = home_edit_place_sub(e, furn, x, y, rot, sub, pa == FURN_PAINT_DEFAULT ? -1 : pa,
                            pb == FURN_PAINT_DEFAULT ? -1 : pb);
    if (r >= 0) p->placed++;
    else ghost(p, furn, x, y, rot, -r == HPR_NOT_OWNED ? HPS_MISSING : HPS_BLOCKED);
}

int home_edit_blueprint(HomeEdit *e, int bp, HomePlan *plan)
{
    HomeState *s = home_state();
    int i, k, pass, theme = THEME_NONE, nrom = (int)doomo_home_blueprint_count;
    const HomeRoom *saved = NULL;
    memset(plan, 0, sizeof(*plan));
    if (e->remove_only || bp < 0) return 0;
    if (bp >= nrom) {
        if (!s || bp - nrom >= HOME_BLUEPRINTS || !s->bp[bp - nrom].nobj) return 0;
        saved = &s->bp[bp - nrom];
    } else {
        theme = doomo_home_blueprints[bp].theme;
    }
    /* everything that is not a fixed slot goes back to storage */
    for (i = e->room.nobj - 1; i >= 0; i--)
        if (!home_obj_is_fixed(&e->room, i)) remove_at(&e->room, i);
    if (saved) {
        e->room.wallpaper = saved->wallpaper; e->room.floor = saved->floor;
        e->room.wall_paint = saved->wall_paint; e->room.floor_paint = saved->floor_paint;
    } else if (theme < THEME_COUNT) {
        const DoomoHomeTheme *t = &doomo_home_themes[theme];
        e->room.wallpaper = t->wallpaper; e->room.floor = t->floor;
        e->room.wall_paint = t->wall_paint; e->room.floor_paint = t->floor_paint;
        e->room.flags = (uint8_t)((e->room.flags & ~HRF_CEIL_MASK) | ((t->ceiling & 7) << HRF_CEIL_SHIFT));
        if (s) s->theme = (uint8_t)theme;
    }
    plan->first = e->room.nobj;
    /* floor pieces and rugs first, then wall pieces, then the smalls */
    for (pass = 0; pass < 3; pass++) {
        if (saved) {
            for (k = 0; k < saved->nobj; k++) {
                const HomeObj *o = &saved->obj[k];
                const DoomoFurn *f = home_furn(HO_FURN(o));
                int ly = !f ? 0 : HO_ONSURF(o) ? 2 : (f->flags & FF_WALL) ? 1 : 0;
                if (ly != pass) continue;
                plan_piece(e, plan, HO_FURN(o), HO_X(o), HO_Y(o), HO_ROT(o), HO_SUB(o), o->pa & 0x1F, o->pb & 0x1F);
            }
        } else {
            const DoomoHomeSet *b = &doomo_home_blueprints[bp];
            for (k = 0; k < b->n; k++) {
                const DoomoHomePiece *q = &doomo_home_pieces[b->first + k];
                const DoomoFurn *f = home_furn(q->furn);
                int ly = !f ? 0 : (f->flags & FF_SMALL) ? 2 : (f->flags & FF_WALL) ? 1 : 0;
                if (ly != pass) continue;
                plan_piece(e, plan, q->furn, q->x, q->y, q->rot, q->sub, q->pa, q->pb);
            }
        }
    }
    e->undo_n = 0;
    e->undo_head = 0;
    e->gen++;
    if (!plan->missing && !plan->blocked && plan->placed && !unlock_has(HUL_BLUEPRINT)) unlock_grant(HUL_BLUEPRINT);
    return plan->placed;
}

/* ---- H1 M3: decorate-for (SPEC R19; SF's Wisp's room and the side missions) --------- */
const DoomoHomeSet *home_layout_find(int id)
{
    int i;
    for (i = 0; i < (int)doomo_home_layout_count; i++)
        if (doomo_home_layouts[i].id == id && doomo_home_layouts[i].n) return &doomo_home_layouts[i];
    return NULL;
}

int home_layout_progress(const HomeRoom *r, int id, int tol)
{
    const DoomoHomeSet *l = home_layout_find(id);
    HomeState *s = home_state();
    uint8_t used[HOME_OBJ_MAX];
    int k, i, hit = 0, prog;
    if (!l) return 0;
    if (tol < 0) tol = l->tol;
    memset(used, 0, sizeof(used));
    for (k = 0; k < l->n; k++) {
        const DoomoHomePiece *q = &doomo_home_pieces[l->first + k];
        for (i = 0; i < r->nobj; i++) {
            const HomeObj *o = &r->obj[i];
            const DoomoFurn *f = home_furn(HO_FURN(o));
            int dx = HO_X(o) - q->x, dy = HO_Y(o) - q->y;
            if (used[i] || !f) continue;
            if (q->furn & HOME_PIECE_ROLE) { if (f->role != (q->furn & 0xFF)) continue; }
            else if (HO_FURN(o) != q->furn) continue;
            if ((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) > tol) continue;
            /* "any" pieces may face any way; named ones must face as asked
             * (a wall piece's facing is its wall) */
            if (!(q->furn & HOME_PIECE_ROLE) && HO_ROT(o) != q->rot &&
                !((f->flags & FF_WALL) && (HO_ROT(o) & 1) == (q->rot & 1)))
                continue;
            used[i] = 1;
            hit++;
            break;
        }
    }
    prog = l->need ? (hit >= l->need ? 8 : hit * 8 / l->need) : 0;
    if (s) {
        s->decorate_for = (uint8_t)(((id & 15) << 4) | (prog & 15));
        s->cache[HC_DECOR] = (uint8_t)prog;
        s->ver_flags |= HVF_TOUCHED;
    }
    return prog;
}
