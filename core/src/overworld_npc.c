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
    ownpc_acts_reset();                                   /* S1: indices change */
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
        if (n->sflags & NSF_SCRIPTED) goto emotes;           /* S1: an act drives it */
        if ((n->sflags & NSF_CLOCK) == BCLK_FROZEN || (n->sflags & NSF_CLOCK) == BCLK_STILL)
            goto emotes;                                     /* S1: held by the beat */
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
emotes:
        /* emotes */
        {
            const DoomoResident *r = town_res(n->id);
            if (!r) continue;
            if (n->emote_t) {                                /* S1: a scripted / double-take bubble */
                n->emote_t--;
                n->emote = n->emote_t < 60 ? n->emote_f : EMO_NONE;
                continue;
            }
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
            uint32_t tc = (uint32_t)ownpc_clock(n);          /* S1: frozen / 2x / still clocks */
            int32_t bx = n->x;
            if (r->status == RES_GHOST) fl |= BB_TRANSLUCENT;
            f = n->state == NS_WALK ? (int)(((tc + n->phase) / 5) & 3) : 0;
            if (n->state != NS_WALK && (n->sflags & NSF_CLOCK) != BCLK_STILL &&
                (((tc + n->phase) / 24) & 1)) z += 2;                         /* breathing bob */
            if ((n->sflags & NSF_CLOCK) == BCLK_DOUBLE) bx += (int32_t)((t & 2) ? OWQ : -OWQ);   /* panic jitter */
            if ((n->sflags & NSF_TWITCH) && fx_sin((int)t * 4430) > 0x8000) bx += OWQ;        /* possessed twitch */
            z += n->hop;
            ow_add_billboard(bx, n->y, z, bodyspr_image(n->id, n->dir, f), fl, 0);
            if (n->flags & (NPCF_MAIN | NPCF_SIDE | NPCF_READY)) {
                int bob = ui_wave((int)t + n->phase, 36, 6);
                ow_add_billboard(n->x, n->y + OWQ, z + 108 + bob, owfx_marker(n->flags, (int)t), BB_FULLBRIGHT, 2);
            } else if (n->emote && (near_player(n, 200) || n->emote_t)) {
                int bob = ui_wave((int)t + n->phase, 50, 4);
                ow_add_billboard(n->x, n->y + OWQ, z + 100 + bob, owfx_emote(n->emote), 0, 1);
                if (n->emote_t && n->emote_t < 60 && ow_bb_count > 0)            /* S1: the bubble pops in */
                    ow_bb[ow_bb_count - 1].scale = (uint16_t)tw_tab(&TW_POP_BUBBLE, 59 - n->emote_t);
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

/* ======================================================================
 * S1 M5: scripted NPC acts (SPEC sec.7 S1, SF sec.15.8, CT sec.3.9 / 6.3).
 * Eight script slots live in the scratch (OwScratch.acts); an NPC under
 * an act skips the wander AI. Clock modes (FREEZE / PANIC / STILL) and
 * TWITCH stay on the NPC until THAW.
 * ==================================================================== */
int ownpc_clock(const OwNpc *n)
{
    uint32_t t = doomo_frame_count();
    switch (n->sflags & NSF_CLOCK) {
    case BCLK_FROZEN: case BCLK_STILL: return 0;
    case BCLK_DOUBLE: return (int)(t * 2);
    default: return (int)t;
    }
}

static int dir_toward(int32_t dx, int32_t dy)
{
    int32_t adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    if (adx > ady) return dx < 0 ? 1 : 2;
    return dy < 0 ? 3 : 0;
}

static OwAct *act_slot(int npc)
{
    int i, free_i = -1;
    for (i = 0; i < OW_ACT_MAX; i++) {
        if (ow_scr->acts[i].npc == npc && ow_scr->acts[i].act != 0xFF) return &ow_scr->acts[i];
        if (ow_scr->acts[i].act == 0xFF && free_i < 0) free_i = i;
    }
    return free_i >= 0 ? &ow_scr->acts[free_i] : NULL;
}

static void act_end(OwAct *a)
{
    if (a->npc >= 0 && a->npc < ow_npc_count) {
        OwNpc *n = &ow_npcs[(int)a->npc];
        n->sflags &= (uint8_t)~NSF_SCRIPTED;
        n->hop = 0;
        n->state = NS_IDLE;
        n->timer = 60;
    }
    a->act = 0xFF;
    a->npc = -1;
}

static void emote_pop(OwNpc *n, int emo, int delay)
{
    n->emote_f = (uint8_t)emo;
    n->emote_t = (uint8_t)DOOMO_MIN(60 + delay, 255);
}

int ownpc_cast(int id, int spot, int flags)
{
    int k = ownpc_find(id), cx = spot & 0xFF, cy = (spot >> 8) & 0xFF;
    StoryNpcSpot sp;
    if (!ow_scratch_mine()) return -1;
    if (flags & OWC_REMOVE) {
        if (k < 0) return -1;
        ow_npcs[k] = ow_npcs[--ow_npc_count];
        {   /* the moved npc keeps its acts */
            int i;
            for (i = 0; i < OW_ACT_MAX; i++) {
                if (ow_scr->acts[i].npc == k) act_end(&ow_scr->acts[i]);
                if (ow_scr->acts[i].npc == ow_npc_count) ow_scr->acts[i].npc = (int8_t)k;
            }
        }
        return -1;
    }
    if (k >= 0) {
        OwNpc *n = &ow_npcs[k];
        n->x = n->hx = n->tx = (int32_t)cx * OW_CELL + OW_CELL / 2;
        n->y = n->hy = n->ty = (int32_t)cy * OW_CELL + OW_CELL / 2;
        n->flags = (uint8_t)(flags & 0x1F);
        return k;
    }
    sp.id = (int16_t)id; sp.x = (uint8_t)cx; sp.y = (uint8_t)cy; sp.dir = 0; sp.flags = (uint8_t)(flags & 0x1F);
    add_spot(&sp);
    return ownpc_find(id);
}

int ownpc_acting(int id)
{
    int i, k = ownpc_find(id);
    if (k < 0 || !ow_scratch_mine()) return 0;
    for (i = 0; i < OW_ACT_MAX; i++)
        if (ow_scr->acts[i].npc == k && ow_scr->acts[i].act != 0xFF) return 1;
    return 0;
}

int ownpc_act(int id, int act, int arg)
{
    int k = ownpc_find(id);
    OwNpc *n;
    OwAct *a;
    if (k < 0 || !ow_scratch_mine()) return 0;
    n = &ow_npcs[k];
    switch (act) {                                     /* the instant ones */
    case NACT_TURN: n->dir = (uint8_t)(arg & 3); return 1;
    case NACT_FACE:
        if (arg == DOOMO_PLAYER_ID) ownpc_face_player(k);
        else { int j = ownpc_find(arg); if (j >= 0) n->dir = (uint8_t)dir_toward(ow_npcs[j].x - n->x, ow_npcs[j].y - n->y); }
        return 1;
    case NACT_EMOTE: emote_pop(n, arg, 0); return 1;
    case NACT_FREEZE: n->sflags = (uint8_t)((n->sflags & ~NSF_CLOCK) | BCLK_FROZEN); return 1;
    case NACT_PANIC:  n->sflags = (uint8_t)((n->sflags & ~NSF_CLOCK) | BCLK_DOUBLE); return 1;
    case NACT_STILL:  n->sflags = (uint8_t)((n->sflags & ~NSF_CLOCK) | BCLK_STILL); return 1;
    case NACT_THAW:   n->sflags &= (uint8_t)~(NSF_CLOCK | NSF_TWITCH); return 1;
    case NACT_TWITCH: n->sflags |= NSF_TWITCH; return 1;
    case NACT_RIPPLE: {
        /* neighbours within arg cells bang "!" in a chain, 3 frames apart,
         * nearest first (CT sec.3.9 reaction ripple) */
        int i, order = 0, r = arg > 0 ? arg : 4, done[OW_NPC_MAX];
        memset(done, 0, sizeof(done));
        done[k] = 1;
        emote_pop(n, EMO_BANG, 0);
        for (order = 1; order < 8; order++) {
            int best = -1;
            int32_t bd = (int32_t)r * r * OW_CELL / OWQ * OW_CELL / OWQ;
            for (i = 0; i < ow_npc_count; i++) {
                int32_t dx, dy, d;
                if (done[i] || ow_npcs[i].kind != NK_RESIDENT) continue;
                dx = (ow_npcs[i].x - n->x) / OWQ; dy = (ow_npcs[i].y - n->y) / OWQ;
                d = dx * dx + dy * dy;
                if (d <= bd) { bd = d; best = i; }
            }
            if (best < 0) break;
            done[best] = 1;
            emote_pop(&ow_npcs[best], EMO_BANG, order * 3);
        }
        return 1; }
    default: break;
    }
    a = act_slot(k);
    if (!a) return 0;
    a->npc = (int8_t)k;
    a->act = (uint8_t)act;
    a->t = 0;
    a->arg = (int16_t)arg;
    a->ax = (int16_t)(n->x / OWQ);
    a->ay = (int16_t)(n->y / OWQ);
    a->dur = 0;
    n->sflags |= NSF_SCRIPTED;
    switch (act) {
    case NACT_WALK_TO:
        n->tx = (int32_t)(arg & 0xFF) * OW_CELL + OW_CELL / 2;
        n->ty = (int32_t)((arg >> 8) & 0xFF) * OW_CELL + OW_CELL / 2;
        n->state = NS_WALK;
        a->dur = 600;
        break;
    case NACT_HOP: a->dur = TW_HOP2.n ? TW_HOP2.n : 8; audio_sfx_synth(SFX_BOING); break;
    case NACT_FLEE: a->dur = 48 + (uint16_t)(id % 7); n->dir = (uint8_t)(arg & 3); break;
    case NACT_HEAD_TURN:
        a->dur = (uint16_t)DOOMO_MAX(1, (arg >> 8) & 0xFF);
        if ((arg & 0xFF) >= 64) audio_sfx_synth(SFX_CREAK);     /* a quarter turn or more creaks */
        break;
    case NACT_DOUBLE_TAKE: a->dur = RUNG_R3 + 2 + 2; break;
    default: act_end(a); return 0;
    }
    return 1;
}

static void act_tick(OwAct *a)
{
    OwNpc *n = &ow_npcs[(int)a->npc];
    int t = a->t;
    switch (a->act) {
    case NACT_WALK_TO: {
        int32_t dx = n->tx - n->x, dy = n->ty - n->y, sp = n->speed + 8;
        int32_t sx = DOOMO_CLAMP(dx, -sp, sp), sy = DOOMO_CLAMP(dy, -sp, sp);
        int fh = ow_floor_h(n->x / OW_CELL, n->y / OW_CELL);
        if (sx && ow_walkable_cell((n->x + sx) / OW_CELL, n->y / OW_CELL, fh)) n->x += sx; else sx = 0;
        if (sy && ow_walkable_cell(n->x / OW_CELL, (n->y + sy) / OW_CELL, fh)) n->y += sy; else sy = 0;
        if (sx || sy) n->dir = (uint8_t)dir_toward(sx, sy);
        n->state = NS_WALK;
        if ((!sx && !sy) || (n->x == n->tx && n->y == n->ty)) {
            n->hx = n->x; n->hy = n->y;
            act_end(a);
            return;
        }
        break; }
    case NACT_HOP:
        n->hop = (int8_t)(-tw_tab(&TW_HOP2, t) * 4);
        break;
    case NACT_FLEE: {
        /* x = 2a + 0.35a^2 px (CT sec.3.9), after a stagger of id % 7 frames */
        int stagger = n->id % 7, f = t - stagger;
        int32_t d;
        static const int8_t ddx[4] = { 0, -1, 1, 0 }, ddy[4] = { 1, 0, 0, -1 };
        if (f < 0) break;
        d = (int32_t)(2 * f + (35 * f * f) / 100) * OWQ;
        {
            int32_t nx = (int32_t)a->ax * OWQ + ddx[a->arg & 3] * d, ny = (int32_t)a->ay * OWQ + ddy[a->arg & 3] * d;
            int fh = ow_floor_h(n->x / OW_CELL, n->y / OW_CELL);
            if (ow_walkable_cell(nx / OW_CELL, ny / OW_CELL, fh)) { n->x = nx; n->y = ny; }
        }
        n->state = NS_WALK;
        if ((f % 3) == 0) emote_pop(n, EMO_SWEAT, 0), n->emote_t = 50;  /* sweat drops every 3rd frame */
        break; }
    case NACT_HEAD_TURN: {
        int steps = (a->arg & 0xFF) / 64;                /* quarter turns */
        static const uint8_t cw[4] = { 1, 3, 0, 2 };     /* down->left->up->right */
        if (t + 1 >= a->dur) {
            int k, d = n->dir;
            for (k = 0; k < steps; k++) d = cw[d];
            n->dir = (uint8_t)d;
        }
        break; }
    case NACT_DOUBLE_TAKE:
        /* stop R3, the head turn (2 f), "!" + tik, then the bubble pops */
        if (t == RUNG_R3) ownpc_face_player(a->npc);
        if (t == RUNG_R3 + 2) {
            emote_pop(n, EMO_BANG, 0);
            n->hop = (int8_t)(12 * tw_bounce_amount() / 256);
            audio_sfx_synth(SFX_TIK);
        }
        break;
    default: break;
    }
    a->t++;
    if (a->dur && a->t >= a->dur) act_end(a);
}

void ownpc_acts_update(void)
{
    int i;
    if (!ow_scratch_mine()) return;
    for (i = 0; i < OW_ACT_MAX; i++) {
        OwAct *a = &ow_scr->acts[i];
        if (a->act == 0xFF) continue;
        if (a->npc < 0 || a->npc >= ow_npc_count) { a->act = 0xFF; a->npc = -1; continue; }
        act_tick(a);
    }
}

void ownpc_acts_reset(void)
{
    int i;
    for (i = 0; i < OW_ACT_MAX; i++) { ow_scr->acts[i].act = 0xFF; ow_scr->acts[i].npc = -1; }
}

/* The walk-by double-take (CT sec.6.3): an ambient citizen the player passes
 * within 2 cells may stop, turn, bang "!" and tik - once per citizen per
 * visit, about one in three of them, never during a scripted beat. */
void ownpc_double_take_check(void)
{
    int i;
    if (!ow_scratch_mine() || story_talking() || beat_running() || ui_dialog_active()) return;
    if ((ow.t & 3) != 0) return;
    for (i = 0; i < ow_npc_count; i++) {
        OwNpc *n = &ow_npcs[i];
        int32_t dx, dy;
        if (n->kind != NK_RESIDENT || (n->flags & (NPCF_MAIN | NPCF_SIDE | NPCF_READY | NPCF_FIXED)) ||
            (n->sflags & (NSF_TOOK | NSF_SCRIPTED))) continue;
        dx = (n->x - ow.px) / OWQ; dy = (n->y - ow.py) / OWQ;
        if (dx * dx + dy * dy > (2 * DOOMO_CELL_UNITS) * (2 * DOOMO_CELL_UNITS)) continue;
        n->sflags |= NSF_TOOK;
        if (doomo_hash32((uint32_t)n->id * 2654435761u ^ doomo_town.day) % 3u) continue;
        ownpc_act(n->id, NACT_DOUBLE_TAKE, 0);
        return;                                        /* one per moment */
    }
}
