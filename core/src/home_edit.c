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
    return f && (f->flags2 & FF_PORTAL);
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
    const DoomoFurn *f = home_furn(furn);
    const HomeRoomDef *def = &home_room_defs[(unsigned)room < HOME_ROOM_COUNT ? room : 0];
    HomeFit tmp;
    int W = HR_W(r), D = HR_D(r), i, w, d;
    Box b;
    if (!out) out = &tmp;
    out->z = 0; out->on_surface = 0; out->surface = -1; out->replaces = -1;
    if (!f) return HPR_OFFGRID;
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
    /* a small piece on a surface stacks (HF: half-tile snap is M3) */
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
                    if (j != ignore && HO_ONSURF(&r->obj[j]) && r->obj[j].pos == HO_POS(x, y))
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
    HomeFit fit;
    HomeObj o;
    int rc;
    if (e->remove_only) return -HPR_REMOVE_ONLY;
    rc = home_fit(&e->room, e->room_id, furn, x, y, rot, -1, &fit);
    if (rc != HPR_OK) return -rc;
    if (fit.replaces < 0 && e->room.nobj >= HOME_OBJ_MAX) return -HPR_FULL;
    if (home_edit_avail(e, furn) <= 0) return -HPR_NOT_OWNED;
    o.w0 = HO_W0(furn, rot, fit.on_surface);
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

int home_edit_undo(HomeEdit *e)
{
    HomeUndo *u;
    int i;
    if (!e->undo_n) return 0;
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
    default:
        break;
    }
    u->op = HU_NONE;
    e->gen++;
    return 1;
}
