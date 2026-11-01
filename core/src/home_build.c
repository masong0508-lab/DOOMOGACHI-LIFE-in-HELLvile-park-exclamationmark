/* home_build.c - the house editor's Build-mode operations (owners: H1 for the
 * rules, H4 for the split and the fittings; HF sec.2.1-2.3): the walls and
 * themes, the door and window skins (the Walls panel, home_skin_*), the
 * rubber band's multi-select, group moves and turns, the clipboard, and the
 * blueprints. Split out of home_edit.c (H4) so that the PS1 links it as the
 * home group's `build` part (ps1/tools/ovl_plan.py PARTS): only the scene's
 * own-home and story modes call into it, with PLAT_PART_HOME_BUILD asked
 * for first (home_draw.c want_build(): the Walls, Themes, Blueprints and
 * group menus and the rubber band), never a visit or Hold the Door. The
 * rules, the undo ring and the helpers it shares stay in home_edit.c.
 */
#include "home_int.h"

/* ---- the undo ring's multi-entry steps (HU_GROUP, HU_WALL, the masks) ---------- */
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


/* ---- the walls and the fittings (H4: door and window skins) -------------------- */
int home_edit_walls(HomeEdit *e, int wallpaper, int floor, int wall_paint, int floor_paint)
{
    return home_edit_fixtures(e, wallpaper, floor, wall_paint, floor_paint, -1, -1);
}


int home_skin_free(int furn)
{
    const DoomoFurn *f = home_furn(furn);
    int k;
    if (!f || !(f->flags2 & FF_PORTAL)) return 0;
    for (k = 0; k < HOME_ROOM_COUNT; k++)
        if (home_room_defs[k].door_furn == furn || home_room_defs[k].win_furn == furn) return 1;
    for (k = 0; k < THEME_COUNT; k++)
        if (doomo_home_themes[k].door == furn || doomo_home_themes[k].window == furn) return 1;
    return home_furn_by_role(f->role) == furn;          /* the catalog's first: the default */
}

int home_skin_ok(int furn, int role)
{
    if (!home_is_skin(home_furn(furn), role)) return 0;
    return !home_state() || home_skin_free(furn) || home_owned(furn) > 0;
}

int home_skin_next(int role, int cur, int dir)
{
    int n = home_furn_count(), k, f;
    if (n <= 0) return cur;
    if (cur < 0 || cur >= n) cur = dir > 0 ? n - 1 : 0;
    for (k = 1; k <= n; k++) {
        f = ((cur + (dir > 0 ? k : -k)) % n + n) % n;
        if (home_skin_ok(f, role)) return f;
    }
    return cur;
}


/* swap the door's / every window slot's piece (-1: keep), one HU_MOVE each:
 * the slot, the height and a lamp-like state bit stay, the paints go back to
 * the new piece's own; returns the undo entries pushed */
static int skin_moves(HomeEdit *e, int door, int window)
{
    int i, n = 0;
    if (!home_is_skin(home_furn(door), FR_DOOR)) door = -1;
    if (!home_is_skin(home_furn(window), FR_WINDOW)) window = -1;
    for (i = 0; i < e->room.nobj && i < HOME_OBJ_MAX; i++) {
        HomeObj *o = &e->room.obj[i];
        const DoomoFurn *f = home_furn(HO_FURN(o));
        int to = home_is_skin(f, FR_DOOR) ? door : home_is_skin(f, FR_WINDOW) ? window : -1;
        if (to < 0 || HO_FURN(o) == to) continue;
        home_edit_push(e, HU_MOVE, i, o);
        o->w0 = (uint16_t)((o->w0 & ~0x1FFu) | (unsigned)to);
        o->pa = (uint8_t)((o->pa & 0xE0) | FURN_PAINT_DEFAULT);
        o->pb = (uint8_t)((o->pb & 0xE0) | FURN_PAINT_DEFAULT);
        n++;
    }
    if (n) home_room_skins_sync(&e->room);
    return n;
}

int home_edit_skins(HomeEdit *e, int door, int window)
{
    int n;
    if (e->remove_only) return 0;
    if (door >= 0 && !home_skin_ok(door, FR_DOOR)) door = -1;
    if (window >= 0 && !home_skin_ok(window, FR_WINDOW)) window = -1;
    n = skin_moves(e, door, window);
    if (n > 1) push_group(e, n);
    if (n) e->gen++;
    return n;
}

int home_edit_fixtures(HomeEdit *e, int wallpaper, int floor, int wall_paint, int floor_paint,
                       int door, int window)
{
    int n;
    if (e->remove_only) return 0;
    if (door >= 0 && !home_skin_ok(door, FR_DOOR)) door = -1;
    if (window >= 0 && !home_skin_ok(window, FR_WINDOW)) window = -1;
    push_wall(e);
    if (wallpaper >= 0) e->room.wallpaper = (uint8_t)(wallpaper & 31);
    if (floor >= 0) e->room.floor = (uint8_t)(floor & 31);
    if (wall_paint >= 0) e->room.wall_paint = (uint8_t)(wall_paint & 31);
    if (floor_paint >= 0) e->room.floor_paint = (uint8_t)(floor_paint & 31);
    n = skin_moves(e, door, window);
    if (n) push_group(e, n + 1);                /* the walls and the fittings: one step */
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
    int i, k;
    HomeState *s = home_state();
    if (theme < 0 || theme >= THEME_COUNT || e->remove_only) return 0;
    t = &doomo_home_themes[theme];
    push_wall(e);
    e->room.wallpaper = t->wallpaper;
    e->room.floor = t->floor;
    e->room.wall_paint = t->wall_paint;
    e->room.floor_paint = t->floor_paint;
    e->room.flags = (uint8_t)((e->room.flags & ~HRF_CEIL_MASK) | ((t->ceiling & 7) << HRF_CEIL_SHIFT));
    k = skin_moves(e, t->door, t->window);        /* (H4) the theme's fittings come with it */
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
    push_group(e, 3 + k);
    if (s) s->theme = (uint8_t)theme;
    e->gen++;
    return 1;
}


/* ---- H1 M3: multi-select, group moves, the clipboard (HF sec.2.1) ----------------- */
static int stacked_on(const HomeRoom *r, int small, int surf)
{
    const HomeObj *o = &r->obj[small], *s = &r->obj[surf];
    int w, d;
    if (!HO_ONSURF(o) || HO_ONSURF(s) || home_obj_layer(s) != HOL_FLOOR) return 0;
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
        HomeBox b;
        if (home_obj_is_fixed(r, i) || HO_ONSURF(&r->obj[i])) continue;
        if (home_obj_layer(&r->obj[i]) == HOL_WALL) {
            int a0, a1, z0, z1, wb = home_wall_span(&r->obj[i], &a0, &a1, &z0, &z1);
            b.x0 = wb ? 0 : a0; b.x1 = wb ? 1 : a1; b.y0 = wb ? a0 : 0; b.y1 = wb ? a1 : 1;
        } else {
            b = home_obj_box(&r->obj[i]);
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
        home_edit_push(e, HU_MOVE, idx[k], o);
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
        if (home_obj_layer(o) == HOL_WALL) {         /* wall pieces slide along their wall */
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
        HomeBox b = home_obj_box(&e->room.obj[idx[k]]);
        if (home_obj_layer(&e->room.obj[idx[k]]) == HOL_WALL) continue;
        if (b.x0 < bx0) bx0 = b.x0;
        if (b.y0 < by0) by0 = b.y0;
        if (b.x1 > bx1) bx1 = b.x1;
        if (b.y1 > by1) by1 = b.y1;
    }
    cx2 = bx0 + bx1;                            /* twice the centre */
    cy2 = by0 + by1;
    for (k = 0; k < n; k++) {
        const HomeObj *o = &e->room.obj[idx[k]];
        HomeBox b = home_obj_box(o);
        int nx0, ny0, nx1, ny1, t;
        if (home_obj_layer(o) == HOL_WALL) {
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
        home_edit_push(e, HU_REMOVE, order[k], &e->room.obj[order[k]]);
        home_room_remove(&e->room, order[k]);
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
    /* (H4) it has no fixed pieces: the header keeps the room's fittings */
    s->bp[slot].door_skin = r->door_skin;
    s->bp[slot].window_skins = r->window_skins;
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
        if (!home_obj_is_fixed(&e->room, i)) home_room_remove(&e->room, i);
    if (saved) {
        int d = home_skin_from_code(saved->door_skin, FR_DOOR), w = home_skin_from_code(saved->window_skins, FR_WINDOW);
        e->room.wallpaper = saved->wallpaper; e->room.floor = saved->floor;
        e->room.wall_paint = saved->wall_paint; e->room.floor_paint = saved->floor_paint;
        /* (H4) the fittings it was saved with, where you still may */
        skin_moves(e, d >= 0 && home_skin_ok(d, FR_DOOR) ? d : -1, w >= 0 && home_skin_ok(w, FR_WINDOW) ? w : -1);
    } else if (theme < THEME_COUNT) {
        const DoomoHomeTheme *t = &doomo_home_themes[theme];
        e->room.wallpaper = t->wallpaper; e->room.floor = t->floor;
        e->room.wall_paint = t->wall_paint; e->room.floor_paint = t->floor_paint;
        e->room.flags = (uint8_t)((e->room.flags & ~HRF_CEIL_MASK) | ((t->ceiling & 7) << HRF_CEIL_SHIFT));
        skin_moves(e, t->door, t->window);       /* (H4) and the theme's fittings */
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

