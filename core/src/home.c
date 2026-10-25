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
#include "doomo/story_vm.h"
#include "../gen/furn_ids.h"

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

/* (H4 M2) the rooms the story has reached (HF sec.1.1): the Bedroom after
 * Ep 1.5, the Kitchen after Ep 2.1, the Garden after Ep 2.4, the Cellar
 * after Ep 2.6, the Annex after Ep 4.3. Free play buys them (home_mart.c). */
void home_story_rooms(void)
{
    static const uint8_t room_of[5] = { HOME_ROOM_BEDROOM, HOME_ROOM_KITCHEN, HOME_ROOM_GARDEN,
                                        HOME_ROOM_CELLAR, HOME_ROOM_ANNEX };
    static const uint8_t beat_of[5] = { BEAT_EP1_5, BEAT_EP2_1, BEAT_EP2_4, BEAT_EP2_6, BEAT_EP4_3 };
    int i;
    if (doomo_town.mode != TOWN_STORY || !home_state()) return;
    for (i = 0; i < 5; i++)
        if (story_episode_done(beat_of[i]) && !home_room_open(room_of[i])) home_unlock_room(room_of[i]);
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
    for (r = 0; r < HOME_BLUEPRINTS; r++) n += room_bytes(&s->bp[r]);
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
    for (i = 0; i < HOME_BLUEPRINTS; i++) p = put_room(p, &s->bp[i]);
    memcpy(p, s->photo, HOME_SZ_PHOTOS); p += HOME_SZ_PHOTOS;
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
    for (i = 0; i < HOME_BLUEPRINTS; i++) {
        if (!get_room(p, left, &s->bp[i], &used)) { home_reset(); return 0; }
        p += used; left -= used;
    }
    TAKE(HOME_SZ_PHOTOS + HOME_SZ_WANTS + HOME_SZ_VISITOR_LOG + HOME_SZ_DECORATE_FOR +
         HOME_SZ_PENDING_GAG + HOME_SZ_CACHE);
    memcpy(s->photo, p, HOME_SZ_PHOTOS); p += HOME_SZ_PHOTOS;
    memcpy(s->wants, p, HOME_SZ_WANTS); p += HOME_SZ_WANTS;
    memcpy(s->vlog, p, HOME_SZ_VISITOR_LOG); p += HOME_SZ_VISITOR_LOG;
    s->decorate_for = *p++;
    for (i = 0; i < HOME_PENDING_GAGS; i++, p += 2) s->gag[i] = (uint16_t)(p[0] | (p[1] << 8));
    memcpy(s->cache, p, HOME_SZ_CACHE);
#undef TAKE
    /* never trust a piece id from the medium */
    for (r = 0; r < HOME_ROOM_COUNT + HOME_BLUEPRINTS; r++) {
        HomeRoom *rm = r < HOME_ROOM_COUNT ? &s->room[r] : &s->bp[r - HOME_ROOM_COUNT];
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

/* ---- photos: 8-byte recipes, re-rendered, never pixels (HF sec.2.3) ----------
 * 64 bits, little endian: place 10 (0-9), view 2 (10-11), zoom 1 (12),
 * filter 3 (13-15), daylight 3 (16-18), three stickers of kind 2 + x 4 + y 3
 * (19-45), the layout hash 16 (46-61), a valid bit (62), one spare. */
void home_photo_pack(const HomePhoto *p, uint8_t out[8])
{
    uint64_t v = 0;
    int i;
    v |= (uint64_t)(p->place & 0x3FF);
    v |= (uint64_t)(p->view & 3) << 10;
    v |= (uint64_t)(p->zoom & 1) << 12;
    v |= (uint64_t)(p->filter & 7) << 13;
    v |= (uint64_t)(p->daylight & 7) << 16;
    for (i = 0; i < 3; i++) {
        uint64_t st = (uint64_t)(p->sticker[i] & 3) | ((uint64_t)(p->sx[i] & 15) << 2) | ((uint64_t)(p->sy[i] & 7) << 6);
        v |= st << (19 + i * 9);
    }
    v |= (uint64_t)p->hash << 46;
    v |= (uint64_t)1 << 62;
    for (i = 0; i < 8; i++) out[i] = (uint8_t)(v >> (i * 8));
}

int home_photo_unpack(const uint8_t in[8], HomePhoto *p)
{
    uint64_t v = 0;
    int i;
    for (i = 0; i < 8; i++) v |= (uint64_t)in[i] << (i * 8);
    memset(p, 0, sizeof(*p));
    if (!((v >> 62) & 1)) return 0;
    p->place = (uint16_t)(v & 0x3FF);
    p->view = (uint8_t)((v >> 10) & 3);
    p->zoom = (uint8_t)((v >> 12) & 1);
    p->filter = (uint8_t)((v >> 13) & 7);
    p->daylight = (uint8_t)((v >> 16) & 7);
    for (i = 0; i < 3; i++) {
        unsigned st = (unsigned)((v >> (19 + i * 9)) & 0x1FF);
        p->sticker[i] = (uint8_t)(st & 3);
        p->sx[i] = (uint8_t)((st >> 2) & 15);
        p->sy[i] = (uint8_t)((st >> 6) & 7);
    }
    p->hash = (uint16_t)((v >> 46) & 0xFFFF);
    return 1;
}

/* CRC-16/CCITT of the room as the save writes it: the gallery says "(the
 * room has changed since)" when it differs. */
uint16_t home_room_hash(const HomeRoom *r)
{
    uint8_t buf[HOME_SZ_ROOM_MAX];
    uint8_t *e = put_room(buf, r);
    uint16_t crc = 0xFFFF;
    const uint8_t *q;
    int b;
    for (q = buf; q < e; q++) {
        crc ^= (uint16_t)(*q << 8);
        for (b = 0; b < 8; b++) crc = (uint16_t)((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
    }
    return crc;
}

int home_photo_add(const HomePhoto *p)
{
    HomeState *s = home_state();
    HomePhoto tmp;
    int i;
    if (!s) return -1;
    for (i = 0; i < HOME_PHOTOS; i++)
        if (!home_photo_unpack(s->photo[i], &tmp)) {
            home_photo_pack(p, s->photo[i]);
            s->ver_flags |= HVF_TOUCHED;
            if (home_photo_count() >= 10 && !unlock_has(HUL_PHOTOS_10)) unlock_grant(HUL_PHOTOS_10);
            return i;
        }
    return -1;
}

int home_photo_get(int slot, HomePhoto *p)
{
    HomeState *s = home_state();
    if (!s || slot < 0 || slot >= HOME_PHOTOS) { memset(p, 0, sizeof(*p)); return 0; }
    return home_photo_unpack(s->photo[slot], p);
}

void home_photo_delete(int slot)
{
    HomeState *s = home_state();
    if (!s || slot < 0 || slot >= HOME_PHOTOS) return;
    memset(s->photo[slot], 0, 8);
    s->ver_flags |= HVF_TOUCHED;
}

int home_photo_count(void)
{
    HomePhoto p;
    int i, n = 0;
    for (i = 0; i < HOME_PHOTOS; i++) n += home_photo_get(i, &p);
    return n;
}

/* ---- the Ch2 haunting (HF sec.10.2; SF Ep 2.3, nights 1-3) -------------------------
 * Night 1: the Picnic Crate is one tile to the left in the morning. Night 2:
 * the lamp is off, and one heart of the wallpaper is an eye (HH_EYE keeps its
 * place; it never leaves). Night 3: two pieces move toward the cellar hatch
 * (the Living Room's front-left corner) and Vex's pentagram rug is under
 * the bed, though nobody put it there. Reaching LIT ends the moves; the eye
 * stays an eye and stops blinking. */
static int floor_piece(const HomeRoom *r, int i)
{
    const DoomoFurn *f = home_furn(HO_FURN(&r->obj[i]));
    return f && !(f->flags & (FF_WALL | FF_RUG)) && !(f->flags2 & FF_PORTAL) && !HO_ONSURF(&r->obj[i]);
}

/* slide piece i one tile by (dx, dy) when it fits; its smalls ride along */
static int nudge(HomeRoom *r, int room, int i, int dx, int dy)
{
    HomeObj *o = &r->obj[i];
    HomeFit fit;
    int ox = HO_X(o), oy = HO_Y(o), nx = ox + dx, ny = oy + dy, j, w, d;
    const DoomoFurn *f = home_furn(HO_FURN(o));
    if (!f || nx < 0 || ny < 0) return 0;
    if (home_fit(r, room, HO_FURN(o), nx, ny, HO_ROT(o), i, &fit) != HPR_OK) return 0;
    home_footprint(f, HO_ROT(o), &w, &d);
    for (j = 0; j < r->nobj; j++) {
        HomeObj *q = &r->obj[j];
        if (j != i && HO_ONSURF(q) && HO_X(q) >= ox && HO_X(q) < ox + w && HO_Y(q) >= oy && HO_Y(q) < oy + d)
            q->pos = HO_POS(HO_X(q) + dx, HO_Y(q) + dy);
    }
    o->pos = HO_POS(nx, ny);
    return 1;
}

static int find_furn(const HomeRoom *r, int furn)
{
    int i;
    for (i = 0; i < r->nobj; i++) if (HO_FURN(&r->obj[i]) == furn) return i;
    return -1;
}

/* is anything hanging on wall A at x (the eye's 36..48 px band)? */
static int wall_taken(const HomeRoom *r, int x)
{
    int i;
    for (i = 0; i < r->nobj; i++) {
        const HomeObj *o = &r->obj[i];
        const DoomoFurn *f = home_furn(HO_FURN(o));
        int z0, z1;
        if (!f || !(f->flags & FF_WALL) || (HO_ROT(o) & 1)) continue;
        z0 = o->z * 4;
        z1 = z0 + (f->fh ? f->fh * 4 : 16);
        if (x >= HO_X(o) && x < HO_X(o) + (f->fw ? f->fw : 1) && z0 < 48 && 36 < z1) return 1;
    }
    return 0;
}

static HomeRoom *haunt_room(HomeRoom *work, int work_room, int room)
{
    HomeState *s = home_state();
    if (work && work_room == room) return work;
    if (!s || !(s->room[room].flags & HRF_OPEN)) return NULL;
    return &s->room[room];
}

static int toward_hatch(const HomeRoom *r, int room, int i)
{
    HomeRoom tmp = *r;
    int x = HO_X(&r->obj[i]);
    if (x > 0 && nudge(&tmp, room, i, -1, 0)) return -1;
    return HR_D(r) > HO_Y(&r->obj[i]) + 1 ? 1 : 0;
}

static int haunt_moves(HomeRoom *lr, int n)
{
    int i, best[2] = { -1, -1 }, size[2] = { 0, 0 }, k, moved = 0;
    for (i = 0; i < lr->nobj; i++) {
        const DoomoFurn *f = home_furn(HO_FURN(&lr->obj[i]));
        int sz;
        if (!floor_piece(lr, i)) continue;
        sz = f->fw * f->fd * 16 + f->fh;
        if (sz > size[0]) { size[1] = size[0]; best[1] = best[0]; size[0] = sz; best[0] = i; }
        else if (sz > size[1]) { size[1] = sz; best[1] = i; }
    }
    for (k = 0; k < n && k < 2; k++) {
        int dir;
        if (best[k] < 0) continue;
        dir = toward_hatch(lr, HOME_ROOM_LIVING, best[k]);
        if (dir < 0) moved += nudge(lr, HOME_ROOM_LIVING, best[k], -1, 0);
        else if (dir > 0) moved += nudge(lr, HOME_ROOM_LIVING, best[k], 0, 1);
    }
    return moved;
}

int home_haunt_apply(HomeRoom *work, int room)
{
    HomeState *s = home_state();
    HomeRoom *lr, *br;
    int out = 0, i;
    if (!s) return 0;
    lr = haunt_room(work, room, HOME_ROOM_LIVING);
    if (!lr) return 0;
    while (s->haunt[HH_APPLIED] < s->haunt[HH_NIGHT] && s->haunt[HH_APPLIED] < 3) {
        int night = ++s->haunt[HH_APPLIED];
        if (!(s->haunt[HH_FLAGS] & HHF_ENDED) && home_goal_check(HG_LIT)) {
            s->haunt[HH_FLAGS] |= HHF_ENDED;
            out |= HHA_ENDED;
        }
        if (s->haunt[HH_FLAGS] & HHF_ENDED) continue;
        if (night == 1) {
            i = find_furn(lr, FURN_PICNIC_CRATE);
            if (i >= 0 && (nudge(lr, HOME_ROOM_LIVING, i, -1, 0) || nudge(lr, HOME_ROOM_LIVING, i, 0, 1)))
                out |= HHA_CRATE;
        } else if (night == 2) {
            for (i = 0; i < lr->nobj; i++) {
                const DoomoFurn *f = home_furn(HO_FURN(&lr->obj[i]));
                if (f && (f->flags & FF_LIGHT) && f->role == FR_LAMP && !(f->flags & FF_WALL)) {
                    lr->obj[i].pa |= HOS_OFF;
                    s->haunt[HH_FLAGS] |= HHF_LAMP_OFF;
                    out |= HHA_LAMP;
                    break;
                }
            }
            if (!(s->haunt[HH_FLAGS] & HHF_EYE) && find_furn(lr, FURN_HEART_WALLPAPER_EYE) < 0 &&
                lr->nobj < HOME_OBJ_MAX) {
                int k, W = HR_W(lr);
                for (k = 0; k < W; k++) {
                    int x = W / 2 + ((k & 1) ? -(k + 1) / 2 : k / 2);
                    HomeObj *o;
                    if (x < 0 || x >= W) continue;
                    /* a decal over the wallpaper: only the other wall pieces stop it */
                    if (wall_taken(lr, x)) continue;
                    o = &lr->obj[lr->nobj++];
                    o->w0 = HO_W0(FURN_HEART_WALLPAPER_EYE, 0, 0);
                    o->pos = HO_POS(x, 0);
                    o->z = 9;
                    o->pa = o->pb = FURN_PAINT_DEFAULT;
                    s->haunt[HH_EYE] = (uint8_t)(0x80 | x);
                    s->haunt[HH_FLAGS] |= HHF_EYE;
                    out |= HHA_EYE;
                    break;
                }
            }
        } else {
            if (haunt_moves(lr, 2)) out |= HHA_TWO;
            br = haunt_room(work, room, HOME_ROOM_BEDROOM);
            if (!br) br = lr;
            for (i = 0; i < br->nobj && !(s->haunt[HH_FLAGS] & HHF_PENTA); i++) {
                const DoomoFurn *f = home_furn(HO_FURN(&br->obj[i]));
                int dx, dy;
                if (!f || f->role != FR_BED) continue;
                if (home_owned(FURN_PENTAGRAM_RUG) == 0) home_grant(FURN_PENTAGRAM_RUG);
                for (dy = 0; dy >= -1 && !(s->haunt[HH_FLAGS] & HHF_PENTA); dy--)
                    for (dx = 0; dx >= -1; dx--) {
                        int x = HO_X(&br->obj[i]) + dx, y = HO_Y(&br->obj[i]) + dy;
                        HomeFit fit;
                        HomeObj *o;
                        if (x < 0 || y < 0 || br->nobj >= HOME_OBJ_MAX) continue;
                        if (home_fit(br, br == lr ? HOME_ROOM_LIVING : HOME_ROOM_BEDROOM, FURN_PENTAGRAM_RUG, x, y, 0, -1,
                                     &fit) != HPR_OK)
                            continue;
                        o = &br->obj[br->nobj++];
                        o->w0 = HO_W0(FURN_PENTAGRAM_RUG, 0, 0);
                        o->pos = HO_POS(x, y);
                        o->z = 0;
                        o->pa = o->pb = FURN_PAINT_DEFAULT;
                        s->haunt[HH_FLAGS] |= HHF_PENTA;
                        out |= HHA_PENTA;
                        break;
                    }
            }
        }
    }
    if (out) s->ver_flags |= HVF_TOUCHED;
    return out;
}

int home_haunt_3am(HomeRoom *work, int room)
{
    HomeState *s = home_state();
    int i, n = 0, pick, k;
    if (!s || !work || room != HOME_ROOM_LIVING || !s->haunt[HH_NIGHT] || (s->haunt[HH_FLAGS] & HHF_ENDED) ||
        home_goal_check(HG_LIT))
        return -1;
    for (i = 0; i < work->nobj; i++) n += floor_piece(work, i);
    if (!n) return -1;
    pick = (int)(story_rng(HOME_CTX_HAUNT ^ doomo_town.day) % (uint32_t)n);
    for (i = 0, k = 0; i < work->nobj; i++) {
        if (!floor_piece(work, i)) continue;
        if (k++ != pick) continue;
        if (nudge(work, room, i, -1, 0) || nudge(work, room, i, 0, 1) || nudge(work, room, i, 1, 0)) return i;
        return -1;
    }
    return -1;
}

int home_haunt_eye_blinks(void)
{
    HomeState *s = home_state();
    return s && (s->haunt[HH_FLAGS] & HHF_EYE) && !(s->haunt[HH_FLAGS] & HHF_ENDED) && !home_goal_check(HG_LIT);
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
