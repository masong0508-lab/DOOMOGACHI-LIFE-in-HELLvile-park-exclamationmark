/* home.c - the dream house's state (owner: H1): the 'HOME' chunk (v2, SPEC
 * R9/R10/C9), the room templates, moving in, room unlocks, ownership and
 * grants, and the cache house_api.c serves (C6).
 *
 * On the PS1 this file belongs to the `home` overlay group except what the
 * save registry reaches (the chunk callbacks below are address-taken, so
 * ovl_plan.py keeps them resident): those must only touch the HOME .bss and
 * the resident grant table, never the catalog. Everything that needs the
 * catalog (templates, scoring) runs from SCENE_HOME.
 */
#include "home_int.h"

#if HOME_PERSIST
static HomeState hs;
HomeState *home_state(void) { return &hs; }
#else
HomeState *home_state(void) { return NULL; }
#endif

/* ---- the rooms (HF sec.1.1, R10) ----------------------------------------------
 *              name             w  d  kind          door        nwin  windows            hearth  wp fl wall_paint      floor_paint   dim */
const HomeRoomDef home_room_defs[HOME_ROOM_COUNT] = {
    { "Living Room",    6, 6, ROOMK_WALLS,  HW_A, 4, 2, { HW_A, HW_B }, { 1, 4 }, 1,   0, 0, PAINT_BUBBLEGUM, PAINT_CARAMEL, 0 },
    { "Bedroom",        6, 6, ROOMK_WALLS,  HW_A, 4, 1, { HW_B, 0 },    { 2, 0 }, 0xFF, 1, 1, PAINT_LAVENDER, PAINT_HONEY, 0 },
    { "Kitchen",        8, 6, ROOMK_WALLS,  HW_A, 6, 1, { HW_A, 0 },    { 2, 0 }, 0xFF, 2, 2, PAINT_MINT, PAINT_WHITE, 0 },
    { "Hell-Cellar",    8, 8, ROOMK_WALLS,  HW_A, 1, 0, { 0, 0 },       { 0, 0 }, 0xFF, 3, 3, PAINT_CHARCOAL, PAINT_RUST, 6 },
    { "Garden",         8, 6, ROOMK_GARDEN, HW_A, 3, 0, { 0, 0 },       { 0, 0 }, 0xFF, 4, 4, PAINT_SKY, PAINT_LIME, 0 },
    { "Roof Annex",     6, 6, ROOMK_WALLS,  HW_A, 4, 1, { HW_B, 0 },    { 2, 0 }, 0xFF, 5, 5, PAINT_BONE, PAINT_CHOCOLATE, 0 },
    { "Towers 7B",      6, 6, ROOMK_WALLS,  HW_A, 4, 1, { HW_B, 0 },    { 2, 0 }, 0xFF, 6, 6, PAINT_SILVER, PAINT_CARAMEL, 0 },
    { "Hell Pits Flat", 6, 6, ROOMK_WALLS,  HW_A, 4, 1, { HW_B, 0 },    { 2, 0 }, 0xFF, 7, 7, PAINT_GRAPE, PAINT_CHARCOAL, 0 },
    { "Park Plot",     10, 8, ROOMK_OPEN,   HW_A, 5, 0, { 0, 0 },       { 0, 0 }, 0xFF, 4, 4, PAINT_SKY, PAINT_FOREST, 0 },
};

/* ---- ownership ------------------------------------------------------------- */
int home_owned(int furn)
{
    HomeState *s = home_state();
    if (!s || furn < 0 || furn >= FURN_MAX) return 0;
    return (s->owned[furn >> 1] >> ((furn & 1) * 4)) & 15;
}

int home_placed(int furn, const HomeRoom *work, int work_room)
{
    HomeState *s = home_state();
    int r, i, n = 0;
    for (r = 0; r < HOME_ROOM_COUNT; r++) {
        const HomeRoom *rm = (work && r == work_room) ? work : (s ? &s->room[r] : NULL);
        if (!rm) continue;
        for (i = 0; i < rm->nobj && i < HOME_OBJ_MAX; i++)
            if (HO_FURN(&rm->obj[i]) == furn) n++;
    }
    return n;
}

void home_take_grants(void)
{
    HomeState *s = home_state();
    const DoomoFurnGrant *g;
    if (!s) return;
    for (g = doomo_furn_grants; g && g->name; g++)
        if (g->furn < FURN_MAX && unlock_has(g->unlock) && home_owned(g->furn) == 0)
            home_grant(g->furn);
}

/* ---- rooms ------------------------------------------------------------------ */
int home_room_open(int room)
{
    HomeState *s = home_state();
    if (room < 0 || room >= HOME_ROOM_COUNT) return 0;
    if (!s) return room == HOME_ROOM_LIVING;
    return (s->room[room].flags & HRF_OPEN) != 0;
}

static void put_fixed(HomeRoom *out, int role, int wall, int at, int z)
{
    int f = home_furn_by_role(role);
    HomeObj *o;
    if (f < 0 || out->nobj >= HOME_OBJ_MAX) return;
    o = &out->obj[out->nobj++];
    if (role == FR_HEARTH) {                     /* a floor piece against wall B */
        o->w0 = HO_W0(f, 1, 0);
        o->pos = HO_POS(0, at);
    } else {
        o->w0 = HO_W0(f, wall == HW_A ? 0 : 1, 0);
        o->pos = wall == HW_A ? HO_POS(at, 0) : HO_POS(0, at);
    }
    o->z = (uint8_t)z;
    o->pa = FURN_PAINT_DEFAULT;
    o->pb = FURN_PAINT_DEFAULT;
}

void home_room_template(int room, HomeRoom *out)
{
    const HomeRoomDef *d;
    int i;
    memset(out, 0, sizeof(*out));
    if (room < 0 || room >= HOME_ROOM_COUNT) room = 0;
    d = &home_room_defs[room];
    out->size = (uint8_t)(((d->w - 1) & 15) | (((d->d - 1) & 15) << 4));
    out->wallpaper = 0xFF;
    out->floor = 0xFF;
    out->wall_paint = 0xFF;
    out->floor_paint = 0xFF;
    out->flags = HRF_OPEN | HRF_LIGHTS;
    out->name = (uint8_t)room;
    put_fixed(out, FR_DOOR, d->door_wall, d->door_at, 0);
    for (i = 0; i < d->nwin && i < 2; i++) put_fixed(out, FR_WINDOW, d->win_wall[i], d->win_at[i], 6);
    if (d->hearth_at != 0xFF) put_fixed(out, FR_HEARTH, HW_B, d->hearth_at, 0);
}

void home_room_load(int room, HomeRoom *out)
{
    HomeState *s = home_state();
    if (room < 0 || room >= HOME_ROOM_COUNT) room = 0;
    if (s && (s->room[room].flags & HRF_OPEN)) *out = s->room[room];
    else home_room_template(room, out);
}

void home_room_store(int room, const HomeRoom *in)
{
    HomeState *s = home_state();
    if (!s || room < 0 || room >= HOME_ROOM_COUNT) return;
    s->room[room] = *in;
    s->room[room].flags |= HRF_OPEN;
    s->ver_flags |= HVF_TOUCHED;
}

void home_unlock_room(int room)
{
    HomeState *s = home_state();
    if (!s || room < 0 || room >= HOME_ROOM_COUNT) return;
    if (!(s->room[room].flags & HRF_OPEN)) home_room_template(room, &s->room[room]);
    s->ver_flags |= HVF_TOUCHED;
}

void home_move_in(void)
{
    HomeState *s = home_state();
    int i, n;
    if (!s || (s->ver_flags & HVF_MOVED_IN)) return;
    s->ver_flags |= HVF_MOVED_IN | HVF_TOUCHED;
    s->active_room = HOME_ROOM_LIVING;
    home_unlock_room(HOME_ROOM_LIVING);
    /* the starter kit (the Ch1 crate, a lamp, a bed ...: FF_STARTER rows) */
    n = home_furn_count();
    for (i = 0; i < n; i++) {
        const DoomoFurn *f = home_furn(i);
        if (f && (f->flags2 & FF_STARTER) && home_owned(i) == 0) home_grant(i);
    }
    home_take_grants();
    if (unlock_has(HUL_FIRST_HOUSE) == 0) unlock_grant(HUL_FIRST_HOUSE);
}

/* ---- the cache and the trophies (C6, C9) ----------------------------------------- */
static unsigned score_ctx(void)
{
    unsigned c = 0;
    int ch = doomo_town.mode == TOWN_STORY ? story_chapter() : 0;
    if (doomo_town.minute < 6 * 60 || doomo_town.minute >= 20 * 60) c |= HSC_NIGHT;
    if (ch == 3) c |= HSC_CH3;
    if (ch >= 4) c |= HSC_CH4;
    return c;
}

static void trophy(int id, int cond)
{
    if (cond && !unlock_has(id)) unlock_grant(id);
}

void home_refresh_cache(const HomeRoom *work, int work_room)
{
    HomeState *s = home_state();
    HomeScore sc;
    unsigned all = 0, ctx = score_ctx();
    uint32_t points = 0;
    int r, open = 0, owned = 0, i, n, cats = 0;
    int annex_ok;
    if (!s) return;
    /* HOME also wants the Annex furnished (HF sec.10.1): 4 pieces or more */
    {
        const HomeRoom *ax = (work && work_room == HOME_ROOM_ANNEX) ? work : &s->room[HOME_ROOM_ANNEX];
        int k = 0;
        for (i = 0; i < ax->nobj; i++) {
            const DoomoFurn *f = home_furn(HO_FURN(&ax->obj[i]));
            if (f && !(f->flags2 & FF_PORTAL)) k++;
        }
        annex_ok = (ax->flags & HRF_OPEN) && k >= 4;
    }
    for (r = 0; r < HOME_ROOM_COUNT; r++) {
        const HomeRoom *rm = (work && r == work_room) ? work : &s->room[r];
        if (!(rm->flags & HRF_OPEN) && !(work && r == work_room)) { s->cache[HC_STAMPS + r] = 0; continue; }
        home_score_room(rm, r, ctx, &sc);
        if (!annex_ok) sc.stamps &= (uint8_t)~HOME_STAMP(HG_HOME);
        s->cache[HC_STAMPS + r] = sc.stamps;
        all |= sc.stamps;
        if (r == s->active_room)
            for (i = 0; i < HS_COUNT; i++) s->cache[HC_STAT + i] = sc.stat[i];
        points += (uint32_t)sc.stat[HS_COMFORT] + sc.stat[HS_STYLE] + sc.stat[HS_LIGHT];
        if (r < HOME_COTTAGE_ROOMS) open++;
    }
    if (points > 0xFFFF) points = 0xFFFF;
    if (points > s->house_points) s->house_points = (uint16_t)points;
    n = home_furn_count();
    for (i = 0; i < n; i++) {
        int k = home_owned(i);
        const DoomoFurn *f = home_furn(i);
        owned += k;
        if (k && f && f->cat < 16) cats |= 1 << f->cat;
    }
    trophy(HUL_COSY, (all & HOME_STAMP(HG_COSY)) != 0);
    trophy(HUL_LIT, (all & HOME_STAMP(HG_LIT)) != 0);
    trophy(HUL_HELD, (all & HOME_STAMP(HG_HELD)) != 0);
    trophy(HUL_HOME, (all & HOME_STAMP(HG_HOME)) != 0);
    trophy(HUL_PIECES_100, owned >= 100);
    trophy(HUL_EVERY_CAT, cats == (1 << FCAT_COUNT) - 1);
    trophy(HUL_ALL_ROOMS, open >= HOME_COTTAGE_ROOMS);
}

/* ---- the chunk ------------------------------------------------------------------ */
static int room_bytes(const HomeRoom *r)
{
    return HOME_SZ_ROOM_HEADER + (r->nobj > HOME_OBJ_MAX ? HOME_OBJ_MAX : r->nobj) * HOME_SZ_OBJ;
}

int home_serialize_size(void)
{
    HomeState *s = home_state();
    int r, n;
    if (!s || !(s->ver_flags & (HVF_TOUCHED | HVF_MOVED_IN))) return HOME_CHUNK_EMPTY;
    n = HOME_SZ_HEADER;
    for (r = 0; r < HOME_ROOM_COUNT; r++) n += room_bytes(&s->room[r]);
    n += HOME_SZ_OWNED + HOME_SZ_SEEN_NEW + HOME_SZ_GIFTS + HOME_SZ_VISITED + HOME_SZ_HAUNT;
    n += HOME_BLUEPRINTS * HOME_SZ_ROOM_HEADER;       /* empty blueprint slots (M3) */
    n += HOME_SZ_PHOTOS + HOME_SZ_WANTS + HOME_SZ_VISITOR_LOG + HOME_SZ_DECORATE_FOR;
    n += HOME_SZ_PENDING_GAG + HOME_SZ_CACHE;
    return n;
}

static uint8_t *put_room(uint8_t *p, const HomeRoom *r)
{
    int i, n = r->nobj > HOME_OBJ_MAX ? HOME_OBJ_MAX : r->nobj;
    p[0] = r->wallpaper; p[1] = r->floor; p[2] = r->wall_paint; p[3] = r->floor_paint;
    p[4] = r->size; p[5] = r->flags; p[6] = (uint8_t)n; p[7] = r->name;
    p[8] = r->window_skins; p[9] = r->door_skin;
    p += HOME_SZ_ROOM_HEADER;
    for (i = 0; i < n; i++, p += HOME_SZ_OBJ) {
        const HomeObj *o = &r->obj[i];
        p[0] = (uint8_t)(o->w0 & 0xFF); p[1] = (uint8_t)(o->w0 >> 8);
        p[2] = o->pos; p[3] = o->z; p[4] = o->pa; p[5] = o->pb;
    }
    return p;
}

uint32_t home_chunk_write(uint8_t *dst, uint32_t cap)
{
    HomeState *s = home_state();
    uint32_t need = (uint32_t)home_serialize_size();
    uint8_t *p = dst;
    int r, i;
    if (cap < need) return 0;
    if (!s) { memset(dst, 0, HOME_CHUNK_EMPTY); return HOME_CHUNK_EMPTY; }
    p[0] = s->ver_flags; p[1] = s->active_room;
    p[2] = (uint8_t)(s->house_points & 0xFF); p[3] = (uint8_t)(s->house_points >> 8);
    p[4] = s->theme; p[5] = s->defence;
    p[6] = (uint8_t)(s->days_since_fire & 0xFF); p[7] = (uint8_t)(s->days_since_fire >> 8);
    p += HOME_SZ_HEADER;
    if (need == HOME_CHUNK_EMPTY) { p[0] = 0; p[1] = 0; return HOME_CHUNK_EMPTY; }
    for (r = 0; r < HOME_ROOM_COUNT; r++) p = put_room(p, &s->room[r]);
    memcpy(p, s->owned, HOME_SZ_OWNED); p += HOME_SZ_OWNED;
    memcpy(p, s->seen_new, HOME_SZ_SEEN_NEW); p += HOME_SZ_SEEN_NEW;
    memcpy(p, s->gifts, HOME_SZ_GIFTS); p += HOME_SZ_GIFTS;
    memcpy(p, s->visited, HOME_SZ_VISITED); p += HOME_SZ_VISITED;
    memcpy(p, s->haunt, HOME_SZ_HAUNT); p += HOME_SZ_HAUNT;
    for (i = 0; i < HOME_BLUEPRINTS; i++) {          /* empty slots until M3 */
        memset(p, 0, HOME_SZ_ROOM_HEADER);
        p[0] = p[1] = p[2] = p[3] = 0xFF;
        p += HOME_SZ_ROOM_HEADER;
    }
    memset(p, 0, HOME_SZ_PHOTOS); p += HOME_SZ_PHOTOS;
    memcpy(p, s->wants, HOME_SZ_WANTS); p += HOME_SZ_WANTS;
    memcpy(p, s->vlog, HOME_SZ_VISITOR_LOG); p += HOME_SZ_VISITOR_LOG;
    *p++ = s->decorate_for;
    for (i = 0; i < HOME_PENDING_GAGS; i++) { p[0] = (uint8_t)(s->gag[i] & 0xFF); p[1] = (uint8_t)(s->gag[i] >> 8); p += 2; }
    memcpy(p, s->cache, HOME_SZ_CACHE); p += HOME_SZ_CACHE;
    return (uint32_t)(p - dst);
}

/* Parse one room record; *used = its bytes (0 = malformed). */
static int get_room(const uint8_t *p, uint32_t left, HomeRoom *r, uint32_t *used)
{
    int i, n;
    *used = 0;
    if (left < HOME_SZ_ROOM_HEADER) return 0;
    n = p[6];
    if (n > HOME_OBJ_MAX || left < (uint32_t)(HOME_SZ_ROOM_HEADER + n * HOME_SZ_OBJ)) return 0;
    if (r) {
        memset(r, 0, sizeof(*r));
        r->wallpaper = p[0]; r->floor = p[1]; r->wall_paint = p[2]; r->floor_paint = p[3];
        r->size = p[4]; r->flags = p[5]; r->nobj = (uint8_t)n; r->name = p[7];
        r->window_skins = p[8]; r->door_skin = p[9];
        for (i = 0; i < n; i++) {
            const uint8_t *q = p + HOME_SZ_ROOM_HEADER + i * HOME_SZ_OBJ;
            HomeObj *o = &r->obj[i];
            o->w0 = (uint16_t)(q[0] | (q[1] << 8));
            o->pos = q[2]; o->z = q[3]; o->pa = q[4]; o->pb = q[5];
        }
    }
    *used = (uint32_t)(HOME_SZ_ROOM_HEADER + n * HOME_SZ_OBJ);
    return 1;
}

int home_chunk_read(const uint8_t *src, uint32_t len, uint16_t version)
{
    HomeState *s = home_state();
    const uint8_t *p = src;
    uint32_t left = len, used;
    int r, i;
    home_reset();
    if (version != HOME_CHUNK_VERSION || len < HOME_CHUNK_EMPTY) return 0;
    if (!s) return 1;
#define TAKE(n) do { if (left < (uint32_t)(n)) { home_reset(); return 0; } } while (0)
    s->ver_flags = p[0]; s->active_room = p[1] < HOME_ROOM_COUNT ? p[1] : 0;
    s->house_points = (uint16_t)(p[2] | (p[3] << 8));
    s->theme = p[4]; s->defence = p[5];
    s->days_since_fire = (uint16_t)(p[6] | (p[7] << 8));
    p += HOME_SZ_HEADER; left -= HOME_SZ_HEADER;
    if (len == HOME_CHUNK_EMPTY) return 1;
    for (r = 0; r < HOME_ROOM_COUNT; r++) {
        if (!get_room(p, left, &s->room[r], &used)) { home_reset(); return 0; }
        p += used; left -= used;
    }
    TAKE(HOME_SZ_OWNED + HOME_SZ_SEEN_NEW + HOME_SZ_GIFTS + HOME_SZ_VISITED + HOME_SZ_HAUNT);
    memcpy(s->owned, p, HOME_SZ_OWNED); p += HOME_SZ_OWNED;
    memcpy(s->seen_new, p, HOME_SZ_SEEN_NEW); p += HOME_SZ_SEEN_NEW;
    memcpy(s->gifts, p, HOME_SZ_GIFTS); p += HOME_SZ_GIFTS;
    memcpy(s->visited, p, HOME_SZ_VISITED); p += HOME_SZ_VISITED;
    memcpy(s->haunt, p, HOME_SZ_HAUNT); p += HOME_SZ_HAUNT;
    left -= HOME_SZ_OWNED + HOME_SZ_SEEN_NEW + HOME_SZ_GIFTS + HOME_SZ_VISITED + HOME_SZ_HAUNT;
    for (i = 0; i < HOME_BLUEPRINTS; i++) {          /* M3 keeps them; M1 skips them */
        if (!get_room(p, left, NULL, &used)) { home_reset(); return 0; }
        p += used; left -= used;
    }
    TAKE(HOME_SZ_PHOTOS + HOME_SZ_WANTS + HOME_SZ_VISITOR_LOG + HOME_SZ_DECORATE_FOR +
         HOME_SZ_PENDING_GAG + HOME_SZ_CACHE);
    p += HOME_SZ_PHOTOS;
    memcpy(s->wants, p, HOME_SZ_WANTS); p += HOME_SZ_WANTS;
    memcpy(s->vlog, p, HOME_SZ_VISITOR_LOG); p += HOME_SZ_VISITOR_LOG;
    s->decorate_for = *p++;
    for (i = 0; i < HOME_PENDING_GAGS; i++, p += 2) s->gag[i] = (uint16_t)(p[0] | (p[1] << 8));
    memcpy(s->cache, p, HOME_SZ_CACHE);
#undef TAKE
    /* never trust a piece id from the medium */
    for (r = 0; r < HOME_ROOM_COUNT; r++) {
        HomeRoom *rm = &s->room[r];
        int k = 0;
        for (i = 0; i < rm->nobj; i++)
            if (HO_FURN(&rm->obj[i]) < FURN_MAX) rm->obj[k++] = rm->obj[i];
        rm->nobj = (uint8_t)k;
    }
    home_take_grants();
    return 1;
}

void home_reset(void)
{
    HomeState *s = home_state();
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->theme = THEME_NONE;
}

static uint32_t hx_size(void) { return (uint32_t)home_serialize_size(); }
static void hx_reset(void) { home_reset(); }

const DoomoSaveExt home_ext = {
    HOME_CHUNK_TAG, HOME_CHUNK_VERSION, hx_size, home_chunk_write, home_chunk_read, hx_reset,
    HOME_CHUNK_WORST
};
#if HOME_PERSIST
const DoomoSaveExt *const doomo_save_ext_home = &home_ext;
#endif
