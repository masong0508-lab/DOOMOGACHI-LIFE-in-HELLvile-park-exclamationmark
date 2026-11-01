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

HomeBox home_obj_box(const HomeObj *o)
{
    HomeBox b;
    int w, d;
    home_footprint(home_furn(HO_FURN(o)), HO_ROT(o), &w, &d);
    b.x0 = HO_X(o); b.y0 = HO_Y(o); b.x1 = b.x0 + w; b.y1 = b.y0 + d;
    return b;
}

static int overlap(HomeBox a, HomeBox b)
{
    return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
}

/* What layer an object lives in (HOL_*, home_int.h). */
int home_obj_layer(const HomeObj *o)
{
    const DoomoFurn *f = home_furn(HO_FURN(o));
    if (!f) return HOL_FLOOR;
    if (f->flags & FF_WALL) return HOL_WALL;
    if (f->flags & FF_RUG) return HOL_RUG;
    if (HO_ONSURF(o)) return HOL_STACK;
    return HOL_FLOOR;
}

/* Wall pieces: rot even = wall A (y = 0), odd = wall B (x = 0), and in the
 * open kinds rot 2 / 3 on the front edges (H5, home_wall_edge); they span
 * fw tiles along the edge and fh*4 px from z*4. */
static int wall_z_default(const DoomoFurn *f)
{
    int h = f->fh * 4;
    if (f->role == FR_WINDOW) return 6;
    if (f->role == FR_FENCE) return 0;          /* (H5) fences stand on the ground */
    if (h >= 40) return 0;
    return ((64 - h) / 2) / 4;
}

int home_wall_span(const HomeRoom *r, const HomeObj *o, int *a0, int *a1, int *z0, int *z1)
{
    const DoomoFurn *f = home_furn(HO_FURN(o));
    int along = (HO_ROT(o) & 1) ? HO_Y(o) : HO_X(o);
    *a0 = along;
    *a1 = along + (f && f->fw ? f->fw : 1);
    *z0 = o->z * 4;
    *z1 = *z0 + (f ? f->fh * 4 : 16);
    return home_wall_edge(HO_ROT(o), HO_X(o), HO_Y(o), HR_W(r), HR_D(r));
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
    HomeBox b;
    if (idx < 0 || idx >= r->nobj || home_obj_layer(&r->obj[idx]) != HOL_FLOOR) return 0;
    b = home_obj_box(&r->obj[idx]);
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
        int ly = home_obj_layer(o), k = 0;
        if (ly == HOL_WALL) {
            int a0, a1, z0, z1, e = home_wall_span(r, o, &a0, &a1, &z0, &z1), wb = e & 1;
            int along = wb ? y : x, edge = wb ? x : y;
            int at = e == HE_FRONT_A ? HR_D(r) - 1 : e == HE_FRONT_B ? HR_W(r) - 1 : 0;
            if (edge == at && along >= a0 && along < a1) k = 2;
        } else {
            HomeBox b = home_obj_box(o);
            if (x >= b.x0 && x < b.x1 && y >= b.y0 && y < b.y1)
                k = ly == HOL_STACK ? 5 : ly == HOL_FLOOR ? 4 : 1;
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
    HomeBox b;
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
        /* (H5) the open kinds take wall pieces on every edge: the Garden
         * only its fences, the Park Plot anything (rot 2 / 3 on the front) */
        int wb = rot & 1, a0 = wb ? y : x, a1 = a0 + (f->fw ? f->fw : 1);
        int z0 = wall_z_default(f) * 4, z1 = z0 + f->fh * 4, e = wb;
        if (def->kind == ROOMK_GARDEN && f->role != FR_FENCE) return HPR_FLOOR_ONLY;
        if (def->kind != ROOMK_WALLS) e = home_wall_edge(rot, x, y, W, D);
        if (e < HE_FRONT_A && (wb ? x : y) != 0) return HPR_WALL_ONLY;
        if (a0 < 0 || a1 > (wb ? D : W)) return HPR_OFFGRID;
        for (i = 0; i < r->nobj; i++) {
            int b0, b1, c0, c1;
            if (i == ignore || home_obj_layer(&r->obj[i]) != HOL_WALL) continue;
            if (home_wall_span(r, &r->obj[i], &b0, &b1, &c0, &c1) != e) continue;
            if (a0 < b1 && b0 < a1 && z0 < c1 && c0 < z1) return HPR_OCCUPIED;
        }
        /* the hearth stands against wall B: nothing hangs over its mouth */
        if (e == HE_B && def->hearth_at != 0xFF && a0 < def->hearth_at + 2 && def->hearth_at < a1 && z0 < 48)
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
            HomeBox c;
            if (i == ignore || home_obj_layer(o) != HOL_FLOOR || !g || !(g->flags & FF_SURFACE)) continue;
            c = home_obj_box(o);
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
        int ly = home_obj_layer(o);
        if (i == ignore || ly == HOL_WALL || ly == HOL_STACK) continue;
        if ((ly == HOL_RUG) != ((f->flags & FF_RUG) != 0)) continue;   /* rugs live under */
        if (overlap(b, home_obj_box(o))) return HPR_OCCUPIED;
    }
    if (!(f->flags & FF_RUG)) {
        /* Kevin's rule: nothing blocks the fireplace (the two tiles in front) */
        if (def->hearth_at != 0xFF) {
            HomeBox h;
            h.x0 = 1; h.x1 = 2; h.y0 = def->hearth_at; h.y1 = def->hearth_at + 2;
            if (overlap(b, h)) return HPR_FIREPLACE;
        }
        /* the door's own tile stays clear */
        {
            HomeBox dr;
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

void home_edit_push(HomeEdit *e, int op, int idx, const HomeObj *old)
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
        home_edit_push(e, HU_MOVE, fit.replaces, &e->room.obj[(int)fit.replaces]);
        e->room.obj[(int)fit.replaces] = o;
        return fit.replaces;
    }
    e->room.obj[e->room.nobj] = o;
    home_edit_push(e, HU_PLACE, e->room.nobj, NULL);
    return e->room.nobj++;
}

void home_room_remove(HomeRoom *r, int idx)
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
    home_edit_push(e, HU_REMOVE, idx, &e->room.obj[idx]);
    home_room_remove(&e->room, idx);
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
    home_edit_push(e, HU_MOVE, idx, o);
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
        home_room_skins_sync(&e->room);         /* (H4) a theme's fittings came back */
        e->gen++;
        return 1;
    }
    undo_one(e);
    home_room_skins_sync(&e->room);
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
        if (u->idx < e->room.nobj) home_room_remove(&e->room, u->idx);
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

/* ---- H1 M3: recolours (HF sec.2.3); the walls and themes are in home_build.c ---- */
int home_edit_paint(HomeEdit *e, int idx, int pa, int pb)
{
    HomeObj *o;
    const DoomoFurn *f;
    if (idx < 0 || idx >= e->room.nobj) return 0;
    o = &e->room.obj[idx];
    f = home_furn(HO_FURN(o));
    if (!f || (f->paint_a == 0xFF && f->paint_b == 0xFF)) return 0;
    home_edit_push(e, HU_MOVE, idx, o);
    if (f->paint_a != 0xFF) o->pa = (uint8_t)((o->pa & 0xE0) | (pa < 0 ? FURN_PAINT_DEFAULT : (pa & 0x1F)));
    if (f->paint_b != 0xFF) o->pb = (uint8_t)((o->pb & 0xE0) | (pb < 0 ? FURN_PAINT_DEFAULT : (pb & 0x1F)));
    e->gen++;
    return 1;
}

/* ---- H4: door and window skins: the room's record of them (the skins
 * themselves, the Walls panel and the themes are in home_build.c) --------------- */
int home_is_skin(const DoomoFurn *f, int role)
{
    return f && f->role == role && (f->flags2 & FF_PORTAL);
}

static int find_role(const HomeRoom *r, int role)
{
    int i;
    for (i = 0; i < r->nobj && i < HOME_OBJ_MAX; i++)
        if (home_is_skin(home_furn(HO_FURN(&r->obj[i])), role)) return i;
    return -1;
}

int home_room_door(const HomeRoom *r) { return find_role(r, FR_DOOR); }
int home_room_window(const HomeRoom *r) { return find_role(r, FR_WINDOW); }

int home_skin_code(int furn, int role)
{
    int i, rank = 0;
    if (!home_is_skin(home_furn(furn), role)) return HSK_NONE;
    for (i = 0; i < furn; i++)
        if (home_is_skin(home_furn(i), role)) rank++;
    return rank < HSK_NONE ? rank : HSK_NONE;
}

int home_skin_from_code(int code, int role)
{
    int i, n = home_furn_count();
    if (code < 0 || code >= HSK_NONE) return -1;
    for (i = 0; i < n; i++)
        if (home_is_skin(home_furn(i), role) && code-- == 0) return i;
    return -1;
}

void home_room_skins_sync(HomeRoom *r)
{
    int d = home_room_door(r), w = home_room_window(r);
    r->door_skin = (uint8_t)(d < 0 ? HSK_NONE : home_skin_code(HO_FURN(&r->obj[d]), FR_DOOR));
    r->window_skins = (uint8_t)(w < 0 ? HSK_NONE : home_skin_code(HO_FURN(&r->obj[w]), FR_WINDOW));
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
