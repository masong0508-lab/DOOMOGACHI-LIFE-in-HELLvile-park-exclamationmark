/* combat_bot.c - the debug autopilot (owner: action). It plays a level by
 * itself for tests (core/tests/test_combat.c) and attract modes: a BFS
 * over the cell grid (step-up, drops, crouch height, doors and keys,
 * lifts, avoiding lava) picks the next cell toward the goal - the exit,
 * else a key, else the nearest demon (to clear kill-events), else an
 * unfired trigger - and it turns to and shoots any demon it can see.
 * Fills the same CombatCtl the pad does, so it exercises the real code.
 * Only linked into a build that calls combat_autopilot().
 *
 * (H10) The BFS's 4 KB work area holds data only inside one plan() call,
 * which runs in an update (combat_tick). On the GBA, where every byte of
 * .bss is EWRAM, it is not a static: it is the raycaster's clip
 * checkpoints in the arena (gba/src/bigbuf.c gba_bot_work), which hold
 * data only inside plat_view3d(), a draw; plan() claims them first.
 * Elsewhere it is three statics, as before (the PS1's ps1/tools/
 * ovl_plan.py BORROW lends their sections to the house group by name).
 */
#include "combat_int.h"

#define BOT_Q          768
#define BOT_DIR_BYTES  (DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H / 2)   /* nibble per cell */
#define BOT_BITS_BYTES (DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H / 8)   /* bit per cell    */
/* the size the GBA lends (gba/src/gba_plat.h GBA_BIG_BOT; bigbuf.c asserts
 * that it stays inside the clip checkpoints) */
DOOMO_STATIC_ASSERT(BOT_DIR_BYTES + BOT_BITS_BYTES + BOT_Q * 2 == 4096, bot_work_is_4k);

#if defined(DOOMO_PLATFORM_GBA)
extern uint8_t gba_bot_work[];                         /* gba/src/bigbuf.c */
void gba_bot_claim(void);
#define BOT_DIR   gba_bot_work
#define BOT_BITS  (gba_bot_work + BOT_DIR_BYTES)
#define BOT_QUEUE ((uint16_t *)(void *)(gba_bot_work + BOT_DIR_BYTES + BOT_BITS_BYTES))
#else
static uint8_t  first_dir[BOT_DIR_BYTES];
static uint8_t  target_bits[BOT_BITS_BYTES];
static uint16_t queue[BOT_Q];
#define BOT_DIR   first_dir
#define BOT_BITS  target_bits
#define BOT_QUEUE queue
#endif

static int      bot_on;
static int      way_x = -1, way_y = -1, replan_t, stuck_t, unstick_t, strafe_dir = 1;
static fix16    last_x, last_y;
static int      wait_lift;

static const int8_t ddx[4] = { 1, 0, -1, 0 };
static const int8_t ddy[4] = { 0, 1, 0, -1 };

static int nib_get(int i) { return (BOT_DIR[i >> 1] >> ((i & 1) * 4)) & 15; }
static void nib_set(int i, int v)
{
    BOT_DIR[i >> 1] = (uint8_t)((BOT_DIR[i >> 1] & ~(15 << ((i & 1) * 4))) | (v << ((i & 1) * 4)));
}

static void floor_range(int cx, int cy, int *lo, int *hi)
{
    if (!map_lift_range(&doomo_map, cx, cy, lo, hi)) *lo = *hi = cw_floor(cx, cy);
}

/* Only the BFS calls this, after bounds checking both tile coordinates.
 * Passing direct live-map cells avoids two safe-lookup calls per edge and
 * lets the caller reuse the source cell across all four neighbours. */
static int passable(int cx, int cy, int nx, int ny,
                    const DoomoCell *cur, const DoomoCell *n, int allow_lava)
{
    int f0lo, f0hi, f1lo, f1hi, ceil;
    if (n->wall) return 0;
    if ((n->flags & CELL_LAVA) && !allow_lava) return 0;
    if (n->flags & CELL_DOOR) {
        int open = map_door_open(&doomo_map, nx, ny);
        if (n->tag >= DOOMO_DOOR_TAG_REMOTE && open < 200) return 0;
        if (n->tag == DOOMO_DOOR_TAG_RED && !(doomo_player.keys & 1) && open < 200) return 0;
        if (n->tag == DOOMO_DOOR_TAG_BLUE && !(doomo_player.keys & 2) && open < 200) return 0;
        /* the door's open height (map.c's door mover; the definition's
         * cells are not readable on every platform, map.h) */
        ceil = map_door_top(&doomo_map, nx, ny);
        if (!ceil) ceil = cw_ceil(nx, ny);
    } else {
        /* Static map cells (most of the level) don't have animated
         * height. Their packed 8-byte cell already contains the same
         * height cw_ceil would calculate; avoid a second map lookup. */
        ceil = n->ceil_tex == DOOMO_TEX_SKY ? SKY_CEIL
                                             : n->ceil_h * DOOMO_HEIGHT_STEP;
    }
    /* The old path called map_lift_range() twice for every BFS edge;
     * even a normal tile made that function inspect the whole mover
     * table before consulting cw_floor(). On huge dungeon maps that
     * dominated GBA autopilot CPU time. Only actual lifts need it.
     * Keep the exact dynamic floor range for moving lifts and the
     * ordinary cell height for every other tile. No cached map data. */
    if (cur->flags & CELL_LIFT) floor_range(cx, cy, &f0lo, &f0hi);
    else f0lo = f0hi = cur->floor_h * DOOMO_HEIGHT_STEP;
    if (n->flags & CELL_LIFT) floor_range(nx, ny, &f1lo, &f1hi);
    else f1lo = f1hi = n->floor_h * DOOMO_HEIGHT_STEP;
    if (f1lo - f0hi > DOOMO_STEP_UP) return 0;
    if (ceil - DOOMO_MAX(f1lo, f0lo) < DOOMO_CROUCH_HEIGHT) return 0;
    return 1;
}

/* One prioritized BFS for ALL goals. Previously plan() ran a BFS for
 * health, exit, keys, demons, triggers and pickups separately (and retried
 * each across lava). A far-away or unreachable high-priority target thus
 * caused several complete map scans before choosing a waypoint.
 *
 * The 4 KB scratch area stays exactly the same:
 *   BOT_DIR: 4-bit target priority per cell (0 none, 1 health .. 6 pickup)
 *   BOT_BITS: visited flag per cell, reset for each BFS
 *   BOT_QUEUE: packed x(6) y(6) first-step(2), one 16-bit entry each.
 * One scan can find the shortest reachable path for each goal priority;
 * a second pass across lava is only needed if it can improve the priority.
 * This is *only* debug/demo autopilot routing, not enemy AI or the player's
 * collision/physics. The priority order and safe-before-lava preference
 * match the old planner. No heap, framebuffer, or extra static data. */
static void mark1(int cx, int cy, int priority, int *earliest_priority)
{
    int i, old;
    if (cx < 0 || cy < 0 || cx >= doomo_map.w || cy >= doomo_map.h) return;
    i = cy * doomo_map.w + cx;
    old = nib_get(i);
    if (!old || priority < old) nib_set(i, priority);
    if (priority < *earliest_priority) *earliest_priority = priority;
}

/* Pickups / demons over lava are targeted through safe adjacent tiles. */
static void mark(int cx, int cy, int priority, int *earliest_priority)
{
    int k;
    if (!(map_cell_safe(&doomo_map, cx, cy)->flags & CELL_LAVA)) {
        mark1(cx, cy, priority, earliest_priority);
        return;
    }
    for (k = 0; k < 8; k++) {
        static const int8_t ox[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
        static const int8_t oy[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
        int nx = cx + ox[k], ny = cy + oy[k];
        const DoomoCell *c = map_cell_safe(&doomo_map, nx, ny);
        if (!c->wall && !(c->flags & CELL_LAVA)) mark1(nx, ny, priority, earliest_priority);
    }
}

/* Returns best reachable goal priority and its first move direction.
 * BFS order is identical to the old four-neighbour traversal, so the
 * chosen path for a given priority and lava rule remains shortest. */
static int bfs_priority(int allow_lava, int *first_dir, int earliest_priority)
{
    int w = doomo_map.w, h = doomo_map.h;
    int px = FIX2INT(doomo_player.x) / CELLU, py = FIX2INT(doomo_player.y) / CELLU;
    int head = 0, tail = 0, start = py * w + px;
    int best_priority = 7, best_dir = -1;
    memset(BOT_BITS, 0, BOT_BITS_BYTES);
    BOT_BITS[start >> 3] |= (uint8_t)(1u << (start & 7));
    BOT_QUEUE[tail++] = (uint16_t)(px | (py << 6));
    while (head != tail) {
        int packed = BOT_QUEUE[head], cx = packed & 63, cy = (packed >> 6) & 63;
        int dir = (packed >> 12) & 3, c = cy * w + cx, k;
        const DoomoCell *cur = &doomo_map.cells[c];
        int priority = nib_get(c);
        if (++head == BOT_Q) head = 0;
        if (c != start && priority && priority < best_priority) {
            best_priority = priority;
            best_dir = dir;
            if (priority == earliest_priority) break; /* optimal priority found */
        }
        for (k = 0; k < 4; k++) {
            int nx = cx + ddx[k], ny = cy + ddy[k], ni;
            int next = tail + 1;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            ni = ny * w + nx;
            if (BOT_BITS[ni >> 3] & (1u << (ni & 7))) continue;
            if (!passable(cx, cy, nx, ny, cur, &doomo_map.cells[ni], allow_lava)) continue;
            if (next == BOT_Q) next = 0;
            if (next == head) continue;
            BOT_BITS[ni >> 3] |= (uint8_t)(1u << (ni & 7));
            BOT_QUEUE[tail] = (uint16_t)(nx | (ny << 6) | ((c == start ? k : dir) << 12));
            tail = next;
        }
    }
    *first_dir = best_dir;
    return best_priority;
}

static int plan(void)
{
    int i, x, y, safe_rank, lava_rank;
    int safe_dir, lava_dir, px, py, result_dir, earliest_priority = 7;
#if defined(DOOMO_PLATFORM_GBA)
    gba_bot_claim();   /* Same 4 KB borrowed scratch, zero persistent RAM. */
#endif
    memset(BOT_DIR, 0, BOT_DIR_BYTES);
    /* Scan only once for stationary exits/triggers. Coordinates advance
     * naturally by row instead of dividing by the map width for every
     * tile (ARM7TDMI has no integer divide instruction). */
    for (y = 0, i = 0; y < doomo_map.h; y++) {
        for (x = 0; x < doomo_map.w; x++, i++) {
            uint8_t flags = doomo_map.cells[i].flags;
            if ((flags & CELL_EXIT) && combat_s.goal == CGOAL_EXIT)
                mark(x, y, 2, &earliest_priority);
            if (flags & CELL_TRIGGER)
                mark(x, y, 5, &earliest_priority);
        }
    }
    for (i = 0; i < ACTOR_MAX; i++) {
        const DoomoActor *a = &doomo_actors[i];
        int x = FIX2INT(a->x) / CELLU, y = FIX2INT(a->y) / CELLU;
        if (a->flags & AF_PICKUP) {
            mark(x, y, 6, &earliest_priority);
            if (a->type == THING_KEY_RED || a->type == THING_KEY_BLUE) {
                mark(x, y, 3, &earliest_priority);
            }
            if (doomo_player.hp < 50 &&
                (a->type == THING_HEALTH_SMALL || a->type == THING_HEALTH_BIG ||
                 a->type == THING_SOUL_ORB || a->type == THING_ARMOR)) {
                mark(x, y, 1, &earliest_priority);
            }
        }
        if ((a->flags & AF_SHOOTABLE) && IS_DEMON(a->type) && a->hp > 0) {
            mark(x, y, 4, &earliest_priority);
        }
    }
    if (earliest_priority == 7) return 0;
    safe_rank = bfs_priority(0, &safe_dir, earliest_priority);
    if (safe_rank > earliest_priority) {
        lava_rank = bfs_priority(1, &lava_dir, earliest_priority);
        /* A higher-priority goal reachable through lava wins, as it did
         * when the original planner retried each category across lava. */
        if (lava_rank < safe_rank) {
            safe_rank = lava_rank;
            safe_dir = lava_dir;
        }
    }
    if (safe_rank == 7 || safe_dir < 0) return 0;
    result_dir = safe_dir;
    px = FIX2INT(doomo_player.x) / CELLU;
    py = FIX2INT(doomo_player.y) / CELLU;
    way_x = px + ddx[result_dir];
    way_y = py + ddy[result_dir];
    return 1;
}

static int visible_demon(fix16 *dist_out)
{
    int i, best = -1;
    fix16 bd = INT2FIX(14 * CELLU);
    fix16 ex, ey;
    int ez;
    combat_player_eye(&ex, &ey, &ez);
    for (i = 0; i < ACTOR_MAX; i++) {
        const DoomoActor *a = &doomo_actors[i];
        fix16 d;
        if (!(a->flags & AF_SHOOTABLE) || !IS_DEMON(a->type) || a->hp <= 0) continue;
        if (a->state == AST_DORMANT || (a->status_fx & ST_CHARMED)) continue;
        d = fx_dist(a->x - ex, a->y - ey);
        if (d >= bd) continue;
        if (!cw_los(ex, ey, ez, a->x, a->y, FIX2INT(a->z) + a->height / 2)) continue;
        bd = d;
        best = i;
    }
    if (dist_out) *dist_out = bd;
    return best;
}

/* is it safe to sidestep that way (no lava, no big drop, no wall)? */
static int safe_side(int dir)
{
    int ang = (doomo_player.angle + (dir > 0 ? 1024 : 3072)) & DOOMO_ANG_MASK;
    fix16 x = doomo_player.x + FIXMUL(fx_cos(ang), INT2FIX(44));
    fix16 y = doomo_player.y + FIXMUL(fx_sin(ang), INT2FIX(44));
    int cx = FIX2INT(x) / CELLU, cy = FIX2INT(y) / CELLU;
    const DoomoCell *c = map_cell_safe(&doomo_map, cx, cy);
    if (c->wall || (c->flags & CELL_LAVA)) return 0;
    if (cw_floor(cx, cy) < FIX2INT(doomo_player.z) - DOOMO_STEP_UP) return 0;
    return 1;
}

static int turn_to(int want)
{
    int diff = ((want - doomo_player.angle + 2048) & DOOMO_ANG_MASK) - 2048;
    combat_ctl.turn = (int16_t)DOOMO_CLAMP(diff, -110, 110);
    return diff < 0 ? -diff : diff;
}

/* the best weapon for the situation: shotguns / melee up close, the
 * highest damage-per-second otherwise, never an empty one */
static void choose_weapon(CombatCtl *c, int foe, fix16 dist)
{
    int i, best = -1, best_score = -1;
    static int cool;
    if (cool > 0) { cool--; return; }
    if (combat_px.switch_to >= 0 || combat_px.reload_t) return;
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++) {
        const DoomoWeapon *w = &doomo_town.weapons[i];
        DoomoWeaponStats s;
        int score, have;
        if (!(w->flags & WF_VALID) || w->frame >= WFRAME_COUNT) continue;
        weld_stats(w, &s);
        have = s.ammo_type == 0xFF ? 1 : (combat_px.mag[i] >= s.ammo_use || combat_ammo(s.ammo_type) >= s.ammo_use);
        if (!have) continue;
        score = weld_dps(&s);
        if (s.behaviour == WB_MELEE) score = (foe >= 0 && dist < INT2FIX(90)) ? score : 1;
        if (s.wclass == WCLASS_SHOTGUN && foe >= 0 && dist > INT2FIX(5 * CELLU)) score /= 3;
        if (s.splash && foe >= 0 && dist < INT2FIX(2 * CELLU)) score /= 4;
        if (s.charge) score /= 2;
        if (score > best_score) { best_score = score; best = i; }
    }
    if (best >= 0 && best != doomo_player.weapon_slot) {
        combat_select_weapon(best);
        cool = 45;
    }
    (void)c;
}

static void bot_fill(CombatCtl *c)
{
    int cx = FIX2INT(doomo_player.x) / CELLU, cy = FIX2INT(doomo_player.y) / CELLU;
    fix16 dd;
    int foe;
    memset(c, 0, sizeof(*c));
    if (doomo_player.hp <= 0) return;
    if (--replan_t <= 0 || cx != (int)(last_x >> 22) || way_x < 0) {
        if (!plan()) { way_x = -1; way_y = -1; }
        replan_t = 12;
    }
    foe = visible_demon(&dd);
    choose_weapon(c, foe, dd);
    if (foe >= 0) {
        const DoomoActor *a = &doomo_actors[foe];
        int want = fx_atan2(a->y - doomo_player.y, a->x - doomo_player.x);
        int err = turn_to(want);
        if (err < 90) {
            c->fire = 1;
            c->fire_pressed = (uint8_t)((combat_st.ticks & 1) == 0);
            c->fire_released = (uint8_t)!c->fire_pressed;
        }
        if (combat_ws.charge) { c->fire = (uint8_t)((combat_st.ticks % (combat_ws.charge + 4)) < combat_ws.charge); }
        /* always keep moving sideways while fighting (dodges fireballs);
         * flip direction now and then or when blocked */
        if ((combat_st.ticks % 28) == 0 || fx_dist_lt(doomo_player.x - last_x, doomo_player.y - last_y, FIX_ONE / 2))
            if ((combat_st.ticks & 3) == 0) strafe_dir = -strafe_dir;
        if (!safe_side(strafe_dir)) strafe_dir = -strafe_dir;
        if (dd < INT2FIX(5 * CELLU)) {
            c->strafe = (int8_t)(safe_side(strafe_dir) ? strafe_dir : 0);
            if (dd < INT2FIX(2 * CELLU) + INT2FIX(a->radius)) c->move = -1;
            last_x = doomo_player.x;
            last_y = doomo_player.y;
            return;
        }
    }
    /* standing in lava: jump out */
    if ((map_cell_safe(&doomo_map, cx, cy)->flags & CELL_LAVA) && doomo_player.on_ground) c->jump = 1;
    if (way_x < 0) return;
    {
        fix16 tx = INT2FIX(way_x * CELLU + CELLU / 2), ty = INT2FIX(way_y * CELLU + CELLU / 2);
        int want = fx_atan2(ty - doomo_player.y, tx - doomo_player.x);
        int lo, hi, nf = cw_floor(way_x, way_y);
        /* lifts: stand still until the lift has carried us high enough */
        wait_lift = 0;
        if (map_lift_range(&doomo_map, cx, cy, &lo, &hi) && nf - FIX2INT(doomo_player.z) > DOOMO_STEP_UP) {
            fix16 mx = INT2FIX(cx * CELLU + CELLU / 2), my = INT2FIX(cy * CELLU + CELLU / 2);
            wait_lift = 1;
            if (fx_dist_gt(mx - doomo_player.x, my - doomo_player.y, INT2FIX(10))) {
                turn_to(fx_atan2(my - doomo_player.y, mx - doomo_player.x));
                c->move = 1;
            }
            return;
        }
        if (foe < 0) {
            int err = turn_to(want);
            c->move = (int8_t)(err < 600 ? 1 : 0);
        } else {
            /* keep walking while shooting: move along the path relative to our aim */
            int rel = ((want - doomo_player.angle) & DOOMO_ANG_MASK);
            if (rel < 700 || rel > 4096 - 700) c->move = 1;
            else if (rel > 2048 - 700 && rel < 2048 + 700) c->move = -1;
            else c->strafe = (int8_t)(rel < 2048 ? 1 : -1);
            if (!c->strafe && safe_side(strafe_dir)) c->strafe = (int8_t)strafe_dir;
        }
    }
    /* stuck? jump and wiggle */
    if (unstick_t > 0) {
        unstick_t--;
        c->strafe = (int8_t)strafe_dir;
        if (unstick_t == 10) c->jump = 1;
    } else if (!wait_lift) {
        if (fx_dist_lt(doomo_player.x - last_x, doomo_player.y - last_y, INT2FIX(2))) {
            if (++stuck_t > 40) { stuck_t = 0; unstick_t = 16; strafe_dir = -strafe_dir; replan_t = 0; }
        } else {
            stuck_t = 0;
        }
    }
    last_x = doomo_player.x;
    last_y = doomo_player.y;
}

void combat_autopilot(int on)
{
    bot_on = on ? 1 : 0;
    combat_bot_fn = bot_on ? bot_fill : NULL;
    way_x = way_y = -1;
    replan_t = 0;
    stuck_t = unstick_t = 0;
    wait_lift = 0;
}

int combat_autopilot_on(void) { return bot_on; }
