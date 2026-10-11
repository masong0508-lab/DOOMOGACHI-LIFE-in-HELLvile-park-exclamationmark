/* overworld_npc.c - who walks around the overworld (owner: story).
 *
 * Three kinds of people share OW_NPC_MAX slots on the current map:
 *  - the story cast and side-quest givers (script_cast / side_cast), who
 *    stand at their spots and turn to face you,
 *  - ambient citizens from the roster, placed on the map's THING_NPC wander
 *    spots (Hillvile folk in HaHaland, Hellvile folk in Hellvile, both in
 *    the united park) and wandering around their spot,
 *  - in chapter 3, packs of demons sitting on the invaded town's nests.
 * Everyone is a chibi bodyspr_image billboard; only on-screen citizens ask
 * the portrait cache for images.
 */
#include "overworld_int.h"

/* ow_npcs / ow_props live in the scene scratch (overworld_int.h) */
int ow_npc_count;
int ow_prop_count;

static DoomoRng nrng;
static struct { uint8_t x, y, n; } secrets[8];
static int nsecrets;
static struct { uint8_t x, y, ok; } nest_at[STORY_NESTS];

void script_nest_encounter(int nest);

/* Demons sitting on each nest (overworld look only; the fight spawns its own). */
static const uint8_t nest_look[STORY_NESTS][3] = {
    { THING_IMPLING, THING_PUFF_DEMON, THING_IMPLING },
    { THING_HELLHOUND_PUP, THING_SLIME_BELCHER, THING_HELLHOUND_PUP },
    { THING_BONE_JESTER, THING_IMPLING, THING_CYBER_PIGGY },
    { THING_VAPE_WRAITH, THING_GARGOYLE, THING_VAPE_WRAITH }
};

static int prop_radius(int type)
{
    switch (type) {
    case THING_PROP_TREE: return 12;
    case THING_PROP_LAMP: return 6;
    case THING_PROP_BENCH: return 16;
    case THING_PROP_FOUNTAIN: return 34;
    case THING_PROP_BARREL: return 9;
    case THING_PROP_SKULLS: return 12;
    case THING_PROP_TORCH: return 5;
    case THING_PROP_SIGN: return 7;
    case THING_PROP_FLOWERS: return 12;
    case THING_PROP_MAILBOX: return 7;
    case THING_VAPE_PORTAL: return 18;
    default: return 0;
    }
}

/* Where demon nest `nest` sits on the invaded map. Read from the map
 * definition itself, so it works before (or without) loading that map. */
int ow_nest_cell(int nest, int *cx, int *cy)
{
    const DoomoMapDef *d = doomo_maps[MAPID_HAHALAND_INVADED];
    int i;
    if (nest < 0 || nest >= STORY_NESTS) return 0;
    if (nest_at[nest].ok) {
        if (cx) *cx = nest_at[nest].x;
        if (cy) *cy = nest_at[nest].y;
        return 1;
    }
    if (!d || !d->things) return 0;
    for (i = 0; i < d->thing_count; i++)
        if (d->things[i].type == THING_SPAWNER && d->things[i].arg == nest) {
            if (cx) *cx = d->things[i].x;
            if (cy) *cy = d->things[i].y;
            return 1;
        }
    return 0;
}

static int secret_base(void)
{
    return ow.map == MAPID_HELLVILE ? 16 : ow.map == MAPID_PARK_UNITED ? 24 : 0;
}

void ownpc_load_props(void)
{
    const DoomoMapDef *d = doomo_map.def;
    int i, x, y;
    ow_prop_count = 0;
    nsecrets = 0;
    memset(nest_at, 0, sizeof(nest_at));
    if (d && d->things) {
        for (i = 0; i < d->thing_count; i++) {
            const DoomoThing *t = &d->things[i];
            if (t->type == THING_SPAWNER && t->arg < STORY_NESTS) {
                nest_at[t->arg].x = t->x;
                nest_at[t->arg].y = t->y;
                nest_at[t->arg].ok = 1;
                continue;
            }
            if (!prop_radius(t->type) || ow_prop_count >= OW_PROP_MAX) continue;
            ow_props[ow_prop_count].x = (int16_t)(t->x * DOOMO_CELL_UNITS + DOOMO_CELL_UNITS / 2);
            ow_props[ow_prop_count].y = (int16_t)(t->y * DOOMO_CELL_UNITS + DOOMO_CELL_UNITS / 2);
            ow_props[ow_prop_count].type = t->type;
            ow_props[ow_prop_count].r = (uint8_t)prop_radius(t->type);
            ow_prop_count++;
        }
    }
    for (y = 0; y < doomo_map.h; y++)
        for (x = 0; x < doomo_map.w && nsecrets < 8; x++) {
            const DoomoCell *c = map_cell(&doomo_map, x, y);
            if ((c->flags & CELL_TRIGGER) && c->tag >= STORY_TAG_EVENT_SECRET && c->tag < STORY_TAG_EVENT_SECRET + 8) {
                secrets[nsecrets].x = (uint8_t)x;
                secrets[nsecrets].y = (uint8_t)y;
                secrets[nsecrets].n = (uint8_t)(c->tag - STORY_TAG_EVENT_SECRET);
                nsecrets++;
            }
        }
}

int ownpc_find(int id)
{
    int i;
    for (i = 0; i < ow_npc_count; i++)
        if (ow_npcs[i].kind == NK_RESIDENT && ow_npcs[i].id == id) return i;
    return -1;
}

static OwNpc *new_npc(int id, int kind, int cx, int cy, int flags)
{
    OwNpc *n;
    if (ow_npc_count >= OW_NPC_MAX) return NULL;
    n = &ow_npcs[ow_npc_count++];
    memset(n, 0, sizeof(*n));
    n->id = (int16_t)id;
    n->kind = (uint8_t)kind;
    n->flags = (uint8_t)flags;
    n->x = n->hx = n->tx = (int32_t)cx * OW_CELL + OW_CELL / 2;
    n->y = n->hy = n->ty = (int32_t)cy * OW_CELL + OW_CELL / 2;
    n->state = NS_IDLE;
    n->timer = (uint16_t)rng_between(&nrng, 10, 90);
    n->phase = (uint8_t)rng_range(&nrng, 256);
    n->speed = 20;
    if (kind == NK_RESIDENT) {
        const DoomoResident *r = town_res(id);
        if (r) n->speed = (uint8_t)(12 + r->persona[PERS_MOVEMENT] * 2);
    }
    return n;
}

static void add_spot(const StoryNpcSpot *s)
{
    int k = ownpc_find(s->id);
    OwNpc *n;
    if (k >= 0) {
        ow_npcs[k].flags |= (uint8_t)(s->flags & (NPCF_MAIN | NPCF_SIDE | NPCF_READY));
        return;
    }
    if (s->x >= doomo_map.w || s->y >= doomo_map.h) return;
    if (map_cell(&doomo_map, s->x, s->y)->wall) return;
    n = new_npc(s->id, NK_RESIDENT, s->x, s->y, s->flags);
    if (n) n->dir = s->dir;
}

/* An ambient citizen for wander spot k. */
static int pick_citizen(int k, int district)
{
    uint32_t h = doomo_hash32((uint32_t)ow.map * 977u + (uint32_t)k * 131u + doomo_town.day * 7919u);
    int base = district ? DOOMO_DISTRICT_SIZE : 0, i;
    int start = (int)(h % DOOMO_DISTRICT_SIZE);
    for (i = 0; i < DOOMO_DISTRICT_SIZE; i++) {
        int id = base + (start + i * 37) % DOOMO_DISTRICT_SIZE;
        const DoomoResident *r = &doomo_town.res[id];
        if (r->status != RES_ALIVE && r->status != RES_POSSESSED && r->status != RES_GHOST) continue;
        if (r->flags & RF_STORY_NPC) continue;
        if (r->activity == ACT_SLEEPING && town_is_night() && (h & 1)) continue;
        if (ownpc_find(id) >= 0) continue;
        return id;
    }
    return -1;
}

void ownpc_spawn_all(void)
{
    StoryNpcSpot sp[32];
    const DoomoMapDef *d = doomo_map.def;
    int i, n, k = 0, b = story_beat();
    ow_npc_count = 0;
    rng_seed(&nrng, (uint32_t)ow.map * 31u + doomo_town.day);
    if (!ow.visit) {
        n = script_cast(ow.map, sp, 24);
        for (i = 0; i < n; i++) add_spot(&sp[i]);
        n = side_cast(ow.map, sp, 24);
        for (i = 0; i < n; i++) add_spot(&sp[i]);
        if (ow.map == MAPID_HAHALAND_INVADED && b == BEAT_C3_NESTS) {
            int nest;
            for (nest = 0; nest < STORY_NESTS; nest++) {
                if (!nest_at[nest].ok || story_flag(SF_NEST0 + nest)) continue;
                for (i = 0; i < 3; i++) {
                    static const int8_t ox[3] = { 0, -1, 1 }, oy[3] = { 0, 1, 1 };
                    int cx = nest_at[nest].x + ox[i], cy = nest_at[nest].y + oy[i];
                    OwNpc *m;
                    if (map_cell_safe(&doomo_map, cx, cy)->wall) { cx = nest_at[nest].x; cy = nest_at[nest].y; }
                    m = new_npc(nest_look[nest][i], NK_DEMON, cx, cy, NPCF_FIXED | NPCF_DEMON);
                    if (m) m->nest = (uint8_t)nest;
                }
            }
        }
    }
    /* ambient citizens on the wander spots */
    if (d && d->things) {
        for (i = 0; i < d->thing_count && ow_npc_count < OW_NPC_MAX; i++) {
            const DoomoThing *t = &d->things[i];
            int district, id;
            if (t->type != THING_NPC) continue;
            k++;
            if (ow.map == MAPID_HAHALAND_INVADED && (k % 3) != 0) continue;   /* folks are hiding */
            if (ow.map == MAPID_HAHALAND && !ow.visit && b <= BEAT_C1_KEY && (k % 2)) continue;
            district = ow.map == MAPID_HELLVILE ? 1 : ow.map == MAPID_PARK_UNITED ? (k & 1) : 0;
            id = pick_citizen(k, district);
            if (id >= 0) new_npc(id, NK_RESIDENT, t->x, t->y, 0);
        }
    }
}

int ownpc_blocks(int32_t x, int32_t y, int self)
{
    int i;
    for (i = 0; i < ow_npc_count; i++) {
        const OwNpc *n = &ow_npcs[i];
        int32_t dx, dy;
        if (i == self || n->kind != NK_RESIDENT) continue;
        dx = (n->x - x) / OWQ;
        dy = (n->y - y) / OWQ;
        if (dx > 20 || dx < -20 || dy > 20 || dy < -20) continue;
        if (dx * dx + dy * dy * 2 < 13 * 13) return 1;
    }
    return 0;
}

void ownpc_face_player(int i)
{
    OwNpc *n = &ow_npcs[i];
    int32_t dx = ow.px - n->x, dy = ow.py - n->y;
    int32_t adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    if (adx > ady) n->dir = dx < 0 ? 1 : 2;
    else n->dir = dy < 0 ? 3 : 0;
}

static int target_ok(int cx, int cy, int fh)
{
    int i;
    if (!ow_walkable_cell(cx, cy, fh)) return 0;
    for (i = 0; i < ow_prop_count; i++)
        if (ow_props[i].x / DOOMO_CELL_UNITS == cx && ow_props[i].y / DOOMO_CELL_UNITS == cy) return 0;
    if (map_cell(&doomo_map, cx, cy)->flags & CELL_TRIGGER) return 0;
    return 1;
}

static int near_player(const OwNpc *n, int units)
{
    int32_t dx = (n->x - ow.px) / OWQ, dy = (n->y - ow.py) / OWQ;
    return dx > -units && dx < units && dy > -units && dy < units;
}

void ownpc_update(void)
{
    int i;
    for (i = 0; i < ow_npc_count; i++) {
        OwNpc *n = &ow_npcs[i];
        if (n->kind == NK_DEMON) {
            n->x = n->hx + (int32_t)ui_wave((int)ow.t + n->phase * 3, 70 + n->phase % 30, 10) * OWQ;
            n->y = n->hy + (int32_t)ui_wave((int)ow.t + n->phase * 5, 90 + n->phase % 20, 6) * OWQ;
            n->dir = ((ow.t + n->phase) / 60) & 1 ? 1 : 2;
            continue;
        }
        switch (n->state) {
        case NS_TALK:
            if (!story_talking()) {
                if (n->timer) n->timer--;
                else { n->state = NS_IDLE; n->timer = 40; }
            }
            ownpc_face_player(i);
            break;
        case NS_IDLE:
            if (n->timer) n->timer--;
            if ((n->flags & NPCF_FIXED) || (n->flags & (NPCF_MAIN | NPCF_SIDE | NPCF_READY))) {
                if (near_player(n, 100)) ownpc_face_player(i);
                else if (!n->timer) {
                    n->dir = (uint8_t)rng_range(&nrng, 4);
                    if (n->dir == 3) n->dir = 0;
                    n->timer = (uint16_t)rng_between(&nrng, 60, 180);
                }
                break;
            }
            if (!n->timer) {
                int hx = n->hx / OW_CELL, hy = n->hy / OW_CELL;
                int fh = ow_floor_h(hx, hy);
                int tx = hx + rng_between(&nrng, -2, 2), ty = hy + rng_between(&nrng, -2, 2);
                if (target_ok(tx, ty, fh)) {
                    n->tx = (int32_t)tx * OW_CELL + OW_CELL / 2 + rng_between(&nrng, -12, 12) * OWQ;
                    n->ty = (int32_t)ty * OW_CELL + OW_CELL / 2 + rng_between(&nrng, -8, 8) * OWQ;
                    n->state = NS_WALK;
                }
                n->timer = (uint16_t)rng_between(&nrng, 40, 160);
            }
            if (near_player(n, 56) && (ow.t & 15) == 0) ownpc_face_player(i);
            break;
        case NS_WALK: {
            int32_t dx = n->tx - n->x, dy = n->ty - n->y;
            int32_t sx = 0, sy = 0, sp = n->speed;
            int fh = ow_floor_h(n->x / OW_CELL, n->y / OW_CELL), moved = 0;
            if (dx > sp) sx = sp; else if (dx < -sp) sx = -sp; else sx = dx;
            if (dy > sp) sy = sp; else if (dy < -sp) sy = -sp; else sy = dy;
            if (sx) {
                int32_t nx = n->x + sx;
                int32_t ddx = (nx - ow.px) / OWQ, ddy = (n->y - ow.py) / OWQ;
                if (ow_walkable_cell(nx / OW_CELL, n->y / OW_CELL, fh) && ddx * ddx + ddy * ddy * 2 > 14 * 14) { n->x = nx; moved = 1; }
            }
            if (sy) {
                int32_t ny = n->y + sy;
                int32_t ddx = (n->x - ow.px) / OWQ, ddy = (ny - ow.py) / OWQ;
                if (ow_walkable_cell(n->x / OW_CELL, ny / OW_CELL, fh) && ddx * ddx + ddy * ddy * 2 > 14 * 14) { n->y = ny; moved = 1; }
            }
            if (sx && (sx < 0 ? -sx : sx) >= (sy < 0 ? -sy : sy)) n->dir = sx < 0 ? 1 : 2;
            else if (sy) n->dir = sy < 0 ? 3 : 0;
            if (!moved || (dx == sx && dy == sy)) {
                n->state = NS_IDLE;
                n->timer = (uint16_t)rng_between(&nrng, 50, 200);
            }
            break; }
        default: break;
        }
        /* emotes */
        {
            const DoomoResident *r = town_res(n->id);
            if (!r) continue;
            if (r->status == RES_POSSESSED) n->emote = EMO_SKULL;
            else if (n->state == NS_TALK) n->emote = EMO_TALK;
            else if (ow.map == MAPID_HAHALAND_INVADED) n->emote = (n->phase & 1) ? EMO_SWEAT : EMO_NONE;
            else if (town_is_night() && (n->phase & 3) == 0) n->emote = EMO_ZZZ;
            else if (r->mood == MOOD_LOVESICK || (ow.map == MAPID_PARK_UNITED && (n->phase & 3) == 1)) n->emote = EMO_HEART;
            else if (r->mood == MOOD_ECSTATIC || (ow.map == MAPID_PARK_UNITED && (n->phase & 3) == 2)) n->emote = EMO_NOTE;
            else n->emote = EMO_NONE;
        }
    }
}

int ownpc_nest_near(int32_t x, int32_t y)
{
    int i;
    for (i = 0; i < ow_npc_count; i++) {
        const OwNpc *n = &ow_npcs[i];
        int32_t dx, dy;
        if (n->kind != NK_DEMON) continue;
        dx = (n->x - x) / OWQ;
        dy = (n->y - y) / OWQ;
        if (dx * dx + dy * dy < 52 * 52) return n->nest;
    }
    return -1;
}

void ow_add_billboard(int32_t x, int32_t y, int z, const DoomoImage *img, int flags, int light);

void ownpc_add_billboards(int32_t vx0, int32_t vy0, int32_t vx1, int32_t vy1)
{
    int i;
    uint32_t t = doomo_frame_count();
    for (i = 0; i < ow_npc_count; i++) {
        const OwNpc *n = &ow_npcs[i];
        int cx, cy, z, f;
        if (n->x < vx0 || n->x > vx1 || n->y < vy0 || n->y > vy1) continue;
        cx = n->x / OW_CELL;
        cy = n->y / OW_CELL;
        z = ow_floor_h(cx, cy) * DOOMO_HEIGHT_STEP;
        if (n->kind == NK_DEMON) {
            int spr = SPR_IMPLING + (n->id - THING_IMPLING), fl = BB_SHADOW;
            if (sprite_flags(spr) & SPRF_FLOATS) z += 16 + ui_wave((int)t + n->phase, 40, 6);
            if (sprite_flags(spr) & SPRF_TRANSLUCENT) fl |= BB_TRANSLUCENT;
            if (n->dir == 1) fl |= BB_FLIPX;
            ow_add_billboard(n->x, n->y, z, sprite_image(spr, (int)((t / 8 + n->phase) & 1)), fl, 2);
            continue;
        }
        {
            const DoomoResident *r = town_res(n->id);
            int fl = BB_SHADOW;
            if (!r) continue;
            if (r->status == RES_GHOST) fl |= BB_TRANSLUCENT;
            f = n->state == NS_WALK ? (int)(((t + n->phase) / 5) & 3) : 0;
            if (n->state != NS_WALK && (((t + n->phase) / 24) & 1)) z += 2;     /* breathing bob */
            ow_add_billboard(n->x, n->y, z, bodyspr_image(n->id, n->dir, f), fl, 0);
            if (n->flags & (NPCF_MAIN | NPCF_SIDE | NPCF_READY)) {
                int bob = ui_wave((int)t + n->phase, 36, 6);
                ow_add_billboard(n->x, n->y + OWQ, z + 108 + bob, owfx_marker(n->flags, (int)t), BB_FULLBRIGHT, 2);
            } else if (n->emote && near_player(n, 200)) {
                int bob = ui_wave((int)t + n->phase, 50, 4);
                ow_add_billboard(n->x, n->y + OWQ, z + 100 + bob, owfx_emote(n->emote), 0, 1);
            }
        }
    }
    /* secret sparkles twinkle every few seconds */
    if (!ow.visit) {
        for (i = 0; i < nsecrets; i++) {
            int f = STORY_FLAG_SECRET_FIRST + secret_base() + secrets[i].n;
            int32_t x = (int32_t)secrets[i].x * OW_CELL + OW_CELL / 2, y = (int32_t)secrets[i].y * OW_CELL + OW_CELL / 2;
            uint32_t ph = (t + (uint32_t)i * 37u) % 120u;
            if (story_flag(f) || ph >= 16) continue;
            if (x < vx0 || x > vx1 || y < vy0 || y > vy1) continue;
            ow_add_billboard(x, y, ow_floor_h(secrets[i].x, secrets[i].y) * DOOMO_HEIGHT_STEP + 12,
                             sprite_image(SPR_SPARKLE, (int)(ph / 4)), BB_FULLBRIGHT, 4);
        }
    }
}
