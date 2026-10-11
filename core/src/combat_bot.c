/* combat_bot.c - the debug autopilot (owner: action). It plays a level by
 * itself for tests (core/tests/test_combat.c) and attract modes: a BFS
 * over the cell grid (step-up, drops, crouch height, doors and keys,
 * lifts, avoiding lava) picks the next cell toward the goal - the exit,
 * else a key, else the nearest demon (to clear kill-events), else an
 * unfired trigger - and it turns to and shoots any demon it can see.
 * Fills the same CombatCtl the pad does, so it exercises the real code.
 * Only linked into a build that calls combat_autopilot().
 */
#include "combat_int.h"

#define BOT_Q 768

static int      bot_on;
static uint8_t  first_dir[DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H / 2];   /* nibble per cell */
static uint8_t  target_bits[DOOMO_MAP_MAX_W * DOOMO_MAP_MAX_H / 8];
static uint16_t queue[BOT_Q];
static int      way_x = -1, way_y = -1, replan_t, stuck_t, unstick_t, strafe_dir = 1;
static fix16    last_x, last_y;
static int      wait_lift;

static const int8_t ddx[4] = { 1, 0, -1, 0 };
static const int8_t ddy[4] = { 0, 1, 0, -1 };

static int nib_get(int i) { return (first_dir[i >> 1] >> ((i & 1) * 4)) & 15; }
static void nib_set(int i, int v)
{
    first_dir[i >> 1] = (uint8_t)((first_dir[i >> 1] & ~(15 << ((i & 1) * 4))) | (v << ((i & 1) * 4)));
}

static void floor_range(int cx, int cy, int *lo, int *hi)
{
    if (!map_lift_range(&doomo_map, cx, cy, lo, hi)) *lo = *hi = cw_floor(cx, cy);
}

static int passable(int cx, int cy, int nx, int ny, int allow_lava)
{
    const DoomoCell *n = map_cell_safe(&doomo_map, nx, ny);
    int f0lo, f0hi, f1lo, f1hi, ceil;
    if (n->wall || nx < 0 || ny < 0 || nx >= doomo_map.w || ny >= doomo_map.h) return 0;
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
        ceil = cw_ceil(nx, ny);
    }
    floor_range(cx, cy, &f0lo, &f0hi);
    floor_range(nx, ny, &f1lo, &f1hi);
    if (f1lo - f0hi > DOOMO_STEP_UP) return 0;
    if (ceil - DOOMO_MAX(f1lo, f0lo) < DOOMO_CROUCH_HEIGHT) return 0;
    return 1;
}

/* BFS from the player's cell; returns 1 and sets way_x/way_y to the first
 * step toward the nearest marked target. */
static int bfs_to_targets(int allow_lava)
{
    int w = doomo_map.w, h = doomo_map.h;
    int px = FIX2INT(doomo_player.x) / CELLU, py = FIX2INT(doomo_player.y) / CELLU;
    int head = 0, tail = 0, start = py * w + px;
    memset(first_dir, 0, sizeof(first_dir));
    nib_set(start, 15);
    queue[tail++] = (uint16_t)start;
    while (head != tail) {
        int c = queue[head], cx = c % w, cy = c / w, k;
        head = (head + 1) % BOT_Q;
        if (c != start && (target_bits[c >> 3] & (1 << (c & 7)))) {
            int d = nib_get(c) - 1;
            way_x = px + ddx[d];
            way_y = py + ddy[d];
            return 1;
        }
        for (k = 0; k < 4; k++) {
            int nx = cx + ddx[k], ny = cy + ddy[k], ni;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            ni = ny * w + nx;
            if (nib_get(ni)) continue;
            if (!passable(cx, cy, nx, ny, allow_lava)) continue;
            nib_set(ni, c == start ? k + 1 : nib_get(c));
            if ((tail + 1) % BOT_Q == head) continue;          /* queue full */
            queue[tail] = (uint16_t)ni;
            tail = (tail + 1) % BOT_Q;
        }
    }
    return 0;
}

static void mark1(int cx, int cy)
{
    int i;
    if (cx < 0 || cy < 0 || cx >= doomo_map.w || cy >= doomo_map.h) return;
    i = cy * doomo_map.w + cx;
    target_bits[i >> 3] |= (uint8_t)(1 << (i & 7));
}

/* mark a target cell; things over lava (or flying above it) are marked
 * by the safe cells around them instead */
static void mark(int cx, int cy)
{
    int k;
    if (!(map_cell_safe(&doomo_map, cx, cy)->flags & CELL_LAVA)) { mark1(cx, cy); return; }
    for (k = 0; k < 8; k++) {
        static const int8_t ox[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, oy[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
        int nx = cx + ox[k], ny = cy + oy[k];
        const DoomoCell *c = map_cell_safe(&doomo_map, nx, ny);
        if (!c->wall && !(c->flags & CELL_LAVA)) mark1(nx, ny);
    }
}

static int plan(void)
{
    int i, n, kind;
    for (kind = -1; kind < 5; kind++) {
        memset(target_bits, 0, sizeof(target_bits));
        n = 0;
        if (kind == -1) {
            /* hurt: go for health first */
            if (doomo_player.hp >= 50) continue;
            for (i = 0; i < ACTOR_MAX; i++) {
                const DoomoActor *a = &doomo_actors[i];
                if ((a->flags & AF_PICKUP) && (a->type == THING_HEALTH_SMALL || a->type == THING_HEALTH_BIG ||
                                               a->type == THING_SOUL_ORB || a->type == THING_ARMOR)) {
                    mark(FIX2INT(a->x) / CELLU, FIX2INT(a->y) / CELLU); n++;
                }
            }
        } else if (kind == 0 && combat_s.goal == CGOAL_EXIT) {
            for (i = 0; i < doomo_map.w * doomo_map.h; i++)
                if (doomo_map.cells[i].flags & CELL_EXIT) { mark(i % doomo_map.w, i / doomo_map.w); n++; }
        } else if (kind == 1) {
            for (i = 0; i < ACTOR_MAX; i++) {
                const DoomoActor *a = &doomo_actors[i];
                if ((a->flags & AF_PICKUP) && (a->type == THING_KEY_RED || a->type == THING_KEY_BLUE)) {
                    mark(FIX2INT(a->x) / CELLU, FIX2INT(a->y) / CELLU); n++;
                }
            }
        } else if (kind == 2) {
            for (i = 0; i < ACTOR_MAX; i++) {
                const DoomoActor *a = &doomo_actors[i];
                if ((a->flags & AF_SHOOTABLE) && IS_DEMON(a->type) && a->hp > 0) {
                    mark(FIX2INT(a->x) / CELLU, FIX2INT(a->y) / CELLU); n++;
                }
            }
        } else if (kind == 3) {
            for (i = 0; i < doomo_map.w * doomo_map.h; i++)
                if (doomo_map.cells[i].flags & CELL_TRIGGER) { mark(i % doomo_map.w, i / doomo_map.w); n++; }
        } else if (kind == 4) {
            for (i = 0; i < ACTOR_MAX; i++) {
                const DoomoActor *a = &doomo_actors[i];
                if ((a->flags & AF_PICKUP)) { mark(FIX2INT(a->x) / CELLU, FIX2INT(a->y) / CELLU); n++; }
            }
        }
        if (!n) continue;
        if (bfs_to_targets(0) || bfs_to_targets(1)) return 1;
    }
    return 0;
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
        if ((combat_st.ticks % 28) == 0 || fx_dist(doomo_player.x - last_x, doomo_player.y - last_y) < FIX_ONE / 2)
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
            if (fx_dist(mx - doomo_player.x, my - doomo_player.y) > INT2FIX(10)) {
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
        if (fx_dist(doomo_player.x - last_x, doomo_player.y - last_y) < INT2FIX(2)) {
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
