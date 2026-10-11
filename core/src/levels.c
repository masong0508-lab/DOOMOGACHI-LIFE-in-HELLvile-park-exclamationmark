/* levels.c - the levels expansion's data and shared runtime (owner:
 * levels): the seven Hell Breach levels of the level select (the two
 * story breaches + five new ones), the mini-boss variants, ranks and
 * records (packed into PROG_BREACH_RANKS), the conveyor belts and level
 * scripts that run on top of a combat session, and the activities table
 * (Arena + Hellvile). See levels_int.h.
 */
#include "levels_int.h"

DOOMO_STATIC_ASSERT(PROG_COUNT <= 32, progress_fits_the_save);
DOOMO_STATIC_ASSERT(LV_COUNT * 3 <= 21, ranks_fit_their_bits);

/* ---- the levels ------------------------------------------------------------- */
const LevelInfo lv_info[LV_COUNT] = {
    { "The Vape Cellar", "HELL BREACH 1",
      "Under Cloud Nine Vapor: crates,\na crawlspace and a vape wraith.",
      "Smells like Brimstone Blast down here",
      MAPID_VAPE_CELLAR, LB_CELLAR_WRAITH, 150, 95, SONG_HELL_BREACH, WIN_CUTE,
      PAL_PAINT(PAINT_VAPE_PURPLE, 1), ICON_CLOUD, 6, 5, 3, 255 },
    { "Cloud Nine Spire", "HELL BREACH 2",
      "Vex's tower in the vape storm.\nLord Vapula waits at the top.",
      "Three phases of vapour and spite.",
      MAPID_CLOUD_NINE_SPIRE, LB_VAPULA, 300, 200, SONG_BOSS, WIN_HELL,
      PAL_UI_PURPLE, ICON_BOLT, 28, 50, 6, 255 },
    { "Sugar Rush Sewers", "HELL BREACH 3",
      "Strawberry soda canals... that\nturn into something fleshy.",
      "The soda down here tastes... meaty.",
      MAPID_SUGAR_RUSH_SEWERS, LB_GUTTERKING, 210, 140, SONG_HELL_BREACH, WIN_MINT,
      PAL_PAINT(PAINT_BUBBLEGUM, 1), ICON_DROP, 6, 9, 5, 200 },
    { "Plushie Factory", "HELL BREACH 4",
      "Conveyor belts, stuffing vats\nand plushies with demon eyes.",
      "Now hiring: souls. No experience!",
      MAPID_PLUSHIE_FACTORY, LB_FLUFFNUTS, 220, 150, SONG_INVASION, WIN_CREAM,
      PAL_UI_GOLD, ICON_WRENCH, 24, 9, 6, 200 },
    { "Graveyard Shift", "HELL BREACH 5",
      "A night at the cemetery. Crypts,\nghosts, a weeping in the garden.",
      "Somebody is crying in the garden...",
      MAPID_GRAVEYARD_SHIFT, LB_WIDOW, 260, 175, SONG_HELLVILE, WIN_DARK,
      PAL_PAINT(PAINT_ECTOPLASM, 1), ICON_GHOST, 17, 37, 5, 230 },
    { "Bone Cathedral", "HELL BREACH 6",
      "A gothic hell church that goes\nUP. Bell tower. Pipe organ.",
      "Let us pray. Loudly. With rockets.",
      MAPID_BONE_CATHEDRAL, LB_BONEBOTTOM, 280, 190, SONG_HELL_BREACH, WIN_HELL,
      PAL_RAMP(RAMP_BONE, 6), ICON_CROSS, 22, 33, 10, 200 },
    { "Hellvile Underbelly", "HELL BREACH 7",
      "Below the bad city: a market,\npipes, pistons, the hell gate.",
      "Mind the gap. And the gate.",
      MAPID_HELLVILE_UNDERBELLY, LB_GRUMBLEGUTS, 300, 200, SONG_INVASION, WIN_HELL,
      PAL_UI_RED, ICON_PENTAGRAM, 40, 7, 6, 230 },
};

/* ---- the mini-bosses ------------------------------------------------------------ */
const LevelBoss lv_boss[LB_COUNT] = {
    { "GUMDROP GUTTERKING", "Sticky Sovereign of the Sewers",
      "\"WHO STIRRED MY SODA?!\"",
      "The Gutterking melts into a sad syrup puddle!",
      THING_SLIME_BELCHER, 563, 10, 2, LBF_SUMMON | LBF_PULSE, THING_IMPLING,
      TINT_GREEN, 2, PAL_PAINT(PAINT_SLIME, 1), SPR_SLIME_BELCHER },
    { "FOREMAN FLUFFNUTS", "Middle Management of the Damned",
      "\"BREAK TIME IS OVER, NEW HIRE!\"",
      "Foreman Fluffnuts clocks out. Forever.",
      THING_CYBER_PIGGY, 486, 9, 0, LBF_SUMMON, THING_HELLHOUND_PUP,
      TINT_GOLD, 1, PAL_UI_GOLD, SPR_CYBER_PIGGY },
    { "THE WEEPING WIDOW", "She Lost Her Plushie in 1887",
      "\"Have you come to... cry with me?\"",
      "The Widow stops crying, waves, and fades away.",
      THING_VAPE_WRAITH, 614, 10, 5, LBF_GHOST | LBF_BLINK | LBF_SUMMON, THING_IMPLING,
      TINT_PURPLE, 2, PAL_UI_PURPLE, SPR_VAPE_WRAITH },
    { "BISHOP BONEBOTTOM", "The Organ Grinder",
      "\"LET US PRAY... FOR YOU!\"",
      "The organ plays one last, very sour note.",
      THING_BONE_JESTER, 589, 10, 1, LBF_ORGAN | LBF_SUMMON, THING_GARGOYLE,
      TINT_RED, 1, PAL_RAMP(RAMP_BONE, 6), SPR_BONE_JESTER },
    { "GATEKEEPER GRUMBLEGUTS", "Keeper of the Sealed Gate",
      "\"NOBODY CLOSES MY GATE!\"",
      "The gate engine sputters out. Hellvile breathes.",
      THING_BRUTE, 435, 8, 3, LBF_PORTALS | LBF_PULSE, THING_IMPLING,
      TINT_RED, 2, PAL_UI_RED, SPR_BRUTE },
    { "THE CELLAR WRAITH", "Brimstone Blast Flavoured",
      "\"Wanna try a puff? ...No?\"",
      "The Cellar Wraith dissolves into a fruity cloud.",
      THING_VAPE_WRAITH, 384, 8, 3, LBF_GHOST, THING_IMPLING,
      TINT_PURPLE, 1, PAL_PAINT(PAINT_VAPE_PURPLE, 1), SPR_VAPE_WRAITH },
    { "LORD VAPULA", "Duke of the Ninth Cloud",
      "\"You dare climb MY spire?\"",
      "LORD VAPULA IS DEFEATED!",
      THING_LORD_VAPULA, 256, 8, 0, 0, THING_IMPLING,
      TINT_PURPLE, 1, PAL_UI_PURPLE, SPR_LORD_VAPULA },
};

const char *const lv_rank_names[5] = { "-", "C", "B", "A", "S" };
const char *const lv_mode_names[LVM_COUNT] = { "Hell Breach", "Time Trial", "Cute-Only Run", "Brutal-Only Run" };

/* ======================================================================
 * ranks & records
 * ==================================================================== */
#define RANK_BITS(l)   ((l) * 3)
#define HORDE_SHIFT    21
#define HORDE_MASK     0x7FFu

static uint32_t packed(void) { return progress_get(PROG_BREACH_RANKS); }

int lv_rank(int level)
{
    int v;
    if (level < 0 || level >= LV_COUNT) return RANK_NONE;
    v = (int)((packed() >> RANK_BITS(level)) & 7u);
    return v > RANK_S ? RANK_S : v;
}

int lv_set_rank(int level, int rank)
{
    uint32_t p;
    if (level < 0 || level >= LV_COUNT || rank <= lv_rank(level) || rank > RANK_S) return 0;
    p = packed() & ~(7u << RANK_BITS(level));
    progress_set(PROG_BREACH_RANKS, p | ((uint32_t)rank << RANK_BITS(level)));
    return 1;
}

int lv_horde_best(void) { return (int)((packed() >> HORDE_SHIFT) & HORDE_MASK); }

void lv_set_horde_best(int wave)
{
    uint32_t p;
    if (wave <= lv_horde_best()) return;
    if (wave > (int)HORDE_MASK) wave = (int)HORDE_MASK;
    p = packed() & ~(HORDE_MASK << HORDE_SHIFT);
    progress_set(PROG_BREACH_RANKS, p | ((uint32_t)wave << HORDE_SHIFT));
}

int lv_gold(int level) { return level >= 0 && level < LV_COUNT && unlock_has(LVU_GOLD0 + level); }

/* who opens what: the cellar is always open; the spire and the sewers
 * after the cellar; then one after the other */
static const int8_t lv_prereq[LV_COUNT] = { -1, LV_VAPE_CELLAR, LV_VAPE_CELLAR, LV_SEWERS,
                                            LV_FACTORY, LV_GRAVEYARD, LV_CATHEDRAL };

int lv_prereq_of(int level) { return level >= 0 && level < LV_COUNT ? lv_prereq[level] : -1; }

int lv_unlocked(int level)
{
    int p;
    if (level < 0 || level >= LV_COUNT) return 0;
    p = lv_prereq[level];
    if (p < 0 || lv_rank(p) != RANK_NONE || lv_rank(level) != RANK_NONE) return 1;
    /* the story already took you through the first breaches */
    if (p == LV_VAPE_CELLAR && unlock_has(530)) return 1;
    return 0;
}

int lv_cleared_count(void)
{
    int i, n = 0;
    for (i = 0; i < LV_COUNT; i++) n += lv_rank(i) != RANK_NONE;
    return n;
}

uint8_t lv_rank_color(int rank)
{
    switch (rank) {
    case RANK_S: return PAL_UI_GOLD;
    case RANK_A: return PAL_PAINT(PAINT_BUBBLEGUM, 1);
    case RANK_B: return PAL_PAINT(PAINT_MINT, 1);
    case RANK_C: return PAL_PAINT(PAINT_LAVENDER, 1);
    default: return PAL_GRAY;
    }
}

void lv_score(int level, const DoomoCombatStats *s, LvScore *o)
{
    int secs = (int)(s->ticks / DOOMO_TICK_HZ), par = lv_info[level].par;
    memset(o, 0, sizeof(*o));
    if (secs <= par) o->time_pts = 40;
    else if (secs <= par + par / 4) o->time_pts = 30;
    else if (secs <= par + par / 2) o->time_pts = 20;
    else if (secs <= par * 2) o->time_pts = 10;
    o->kill_pts = s->kills_total ? DOOMO_MIN(35, 35 * s->kills / s->kills_total) : 35;
    o->secret_pts = s->secrets_total ? DOOMO_MIN(25, 25 * s->secrets / s->secrets_total) : 25;
    o->total = o->time_pts + o->kill_pts + o->secret_pts;
    o->rank = o->total >= 90 ? RANK_S : o->total >= 70 ? RANK_A : o->total >= 45 ? RANK_B : RANK_C;
}

void lv_check_monster(void)
{
    if (opt_get(OPT_MONSTER_MODE)) unlock_grant(LVU_MAYHEM);
}

int lv_finish(int level, int mode, const DoomoCombatStats *s, LvScore *score, int *bonus)
{
    static const int16_t rank_bonus[5] = { 0, 25, 75, 150, 300 };
    int news = 0, i, all_s = 1, all_gold = 1, money;
    int first = lv_rank(level) == RANK_NONE;
    lv_score(level, s, score);
    if (lv_set_rank(level, score->rank)) news |= LVR_NEW_RANK;
    if (first) news |= LVR_FIRST_CLEAR;
    money = rank_bonus[score->rank] + (first ? 200 : 0);
    unlock_grant(LVU_TIME_TRIAL);
    if (lv_cleared_count() >= 3) unlock_grant(LVU_CHALLENGES);
    if (lv_cleared_count() >= LV_COUNT) unlock_grant(LVU_ALL_CLEAR);
    if (score->rank == RANK_S) unlock_grant(LVU_FIRST_S);
    if (mode == LVM_TIME_TRIAL && (int)(s->ticks / DOOMO_TICK_HZ) <= lv_info[level].dev) {
        if (unlock_grant(LVU_GOLD0 + level)) money += 250;
        news |= LVR_GOLD;
    }
    if (mode == LVM_CUTE_ONLY) { news |= LVR_CHALLENGE; money += 150; unlock_grant(LVU_CUTE_CLEAR); }
    if (mode == LVM_BRUTAL_ONLY) { news |= LVR_CHALLENGE; money += 150; unlock_grant(LVU_BRUTAL_CLEAR); }
    for (i = 0; i < LV_COUNT; i++) {
        if (lv_rank(i) != RANK_S) all_s = 0;
        if (!lv_gold(i)) all_gold = 0;
    }
    if (all_s) unlock_grant(LVU_ALL_S);
    if (all_gold) unlock_grant(LVU_ALL_GOLD);
    doomo_town.money += (uint32_t)money;
    progress_add(PROG_MONEY_EARNED, money);
    if (bonus) *bonus = money;
    return news;
}

/* ======================================================================
 * the shared runtime: variants, mechanics, conveyors, music
 * ==================================================================== */
LvRun lv_run;
static uint8_t lv_conveyors;          /* the level has conveyor cells */

static int lv_cell_floor(int cx, int cy) { return cw_floor(cx, cy); }

static void find_sources(void)
{
    const DoomoMapDef *d = doomo_map.def;
    int i, n = doomo_map.w * doomo_map.h, k, step;
    int organ = 0, portal = 0;
    lv_run.organ_n = lv_run.portal_n = 0;
    lv_conveyors = 0;
    for (i = 0; i < n; i++) {
        const DoomoCell *c = &doomo_map.cells[i];
        if (c->wall == TEX_RUST_METAL && i + doomo_map.w < n && !doomo_map.cells[i + doomo_map.w].wall &&
            doomo_map.cells[i + doomo_map.w].ceil_tex == DOOMO_TEX_SKY)
            organ++;
        if (!c->wall && (c->flags & CELL_PORTAL)) portal++;
        if (!c->wall && c->tag >= LV_TAG_CONVEYOR_E && c->tag <= LV_TAG_CONVEYOR_N &&
            !(c->flags & (CELL_DOOR | CELL_LIFT | CELL_TRIGGER)))
            lv_conveyors = 1;
    }
    /* sample up to 6 of each, evenly */
    step = organ / 6 + 1;
    for (i = 0, k = 0; i < n && lv_run.organ_n < 6; i++) {
        const DoomoCell *c = &doomo_map.cells[i];
        if (c->wall == TEX_RUST_METAL && i + doomo_map.w < n && !doomo_map.cells[i + doomo_map.w].wall &&
            doomo_map.cells[i + doomo_map.w].ceil_tex == DOOMO_TEX_SKY && (k++ % step) == step / 2) {
            lv_run.organ_x[lv_run.organ_n] = (uint8_t)(i % doomo_map.w);
            lv_run.organ_y[lv_run.organ_n++] = (uint8_t)(i / doomo_map.w + 1);
        }
    }
    step = portal / 6 + 1;
    for (i = 0, k = 0; i < n && lv_run.portal_n < 6; i++) {
        const DoomoCell *c = &doomo_map.cells[i];
        if (!c->wall && (c->flags & CELL_PORTAL) && (k++ % step) == 0) {
            lv_run.portal_x[lv_run.portal_n] = (uint8_t)(i % doomo_map.w);
            lv_run.portal_y[lv_run.portal_n++] = (uint8_t)(i / doomo_map.w);
        }
    }
    /* no portals / pipes of its own (the arena): the spawn marks */
    if (d && d->things && (!lv_run.portal_n || !lv_run.organ_n)) {
        int np = lv_run.portal_n, no = lv_run.organ_n;
        for (i = 0; i < d->thing_count; i++) {
            const DoomoThing *t = &d->things[i];
            if (t->type != THING_SPAWNER && t->type != THING_VAPE_PORTAL) continue;
            if (!np && lv_run.portal_n < 6) {
                lv_run.portal_x[lv_run.portal_n] = t->x;
                lv_run.portal_y[lv_run.portal_n++] = t->y;
            }
            if (!no && lv_run.organ_n < 6) {
                lv_run.organ_x[lv_run.organ_n] = t->x;
                lv_run.organ_y[lv_run.organ_n++] = t->y;
            }
        }
    }
}

void lv_runtime_begin(int level)
{
    int i;
    memset(&lv_run, 0, sizeof(lv_run));
    for (i = 0; i < LV_TRACK; i++) lv_run.track[i].actor = -1;
    lv_run.bar = -1;
    lv_run.intro_boss = LB_NONE;
    lv_run.level = (uint8_t)(level >= 0 && level < LV_COUNT ? level : 255);
    lv_run.base_music = (uint16_t)audio_music_current();
    find_sources();
    if (lv_run.level == 255) return;
    /* the map's own boss becomes the level's mini-boss variant */
    if (lv_info[level].boss != LB_NONE) {
        const LevelBoss *b = &lv_boss[lv_info[level].boss];
        for (i = 0; i < ACTOR_MAX; i++) {
            DoomoActor *a = &doomo_actors[i];
            if ((a->flags & AF_USED) && (a->flags & AF_BOSS) && a->type == b->type) {
                lv_make_boss(i, lv_info[level].boss, 8);
                break;
            }
        }
    }
}

int lv_make_boss(int a, int boss, int hp_scale8)
{
    DoomoActor *ac;
    const LevelBoss *b;
    int i, slot = -1, clear, h, r;
    if (a < 0 || a >= ACTOR_MAX || boss < 0 || boss >= LB_COUNT) return -1;
    for (i = 0; i < LV_TRACK; i++)
        if (lv_run.track[i].actor < 0) { slot = i; break; }
    if (slot < 0) return -1;
    ac = &doomo_actors[a];
    b = &lv_boss[boss];
    ac->flags |= AF_BOSS;
    ac->hp = (int16_t)DOOMO_CLAMP((int32_t)ac->hp * b->hp8 * hp_scale8 / 64, 1, 30000);
    /* a bigger body, but never wider than a corridor or taller than its room */
    r = DOOMO_MIN(ac->radius * b->scale / 256, 30);
    if (r > ac->radius) ac->radius = (uint8_t)r;
    h = ac->height * DOOMO_MIN(b->scale, 384) / 256;
    {
        int cx = FIX2INT(ac->x) / CELLU, cy = FIX2INT(ac->y) / CELLU;
        int ce = cw_ceil(cx, cy);
        clear = ce >= SKY_CEIL ? 200 : ce - FIX2INT(ac->z) - 4;
    }
    if (h > clear) h = clear;
    if (h > ac->height) ac->height = (uint8_t)DOOMO_MIN(h, 200);
    memset(&lv_run.track[slot], 0, sizeof(LvTrack));
    lv_run.track[slot].actor = (int8_t)a;
    lv_run.track[slot].boss = (uint8_t)boss;
    lv_run.track[slot].hp_max = ac->hp;
    lv_run.track[slot].t = (uint16_t)rng_range(&doomo_rng, 60);
    lv_run.size_cls[a] = LVS_NONE;
    combat_s.boss = a;
    combat_s.boss_max = ac->hp;
    return slot;
}

int lv_spawn_boss(int boss, int cx, int cy, int hp_scale8)
{
    int a;
    if (boss < 0 || boss >= LB_COUNT) return -1;
    a = actor_spawn_demon(lv_boss[boss].type, INT2FIX(cx * CELLU + CELLU / 2), INT2FIX(cy * CELLU + CELLU / 2),
                          1, THF_BOSS, 0);
    if (a < 0) return -1;
    doomo_actors[a].target = TARGET_PLAYER;
    if (lv_make_boss(a, boss, hp_scale8) < 0) return a;
    return a;
}

void lv_size_demon(int a, int cls)
{
    DoomoActor *ac;
    if (a < 0 || a >= ACTOR_MAX) return;
    lv_run.size_cls[a] = (uint8_t)cls;
    ac = &doomo_actors[a];
    switch (cls) {
    case LVS_TINY:
        ac->hp = (int16_t)DOOMO_MAX(ac->hp / 2, 1);
        ac->radius = (uint8_t)DOOMO_MAX(ac->radius * 3 / 5, 8);
        ac->height = (uint8_t)DOOMO_MAX(ac->height * 3 / 5, 20);
        break;
    case LVS_BIG:
        ac->hp = (int16_t)DOOMO_MIN(ac->hp * 2, 30000);
        ac->radius = (uint8_t)DOOMO_MIN(ac->radius * 5 / 4, 30);
        ac->height = (uint8_t)DOOMO_MIN(ac->height * 5 / 4, 120);
        break;
    case LVS_HUGE:
        ac->hp = (int16_t)DOOMO_MIN(ac->hp * 4, 30000);
        ac->radius = (uint8_t)DOOMO_MIN(ac->radius * 3 / 2, 30);
        ac->height = (uint8_t)DOOMO_MIN(ac->height * 3 / 2, 140);
        break;
    default:
        break;
    }
}

static int track_alive(const LvTrack *t)
{
    const DoomoActor *a;
    if (t->actor < 0) return 0;
    a = &doomo_actors[t->actor];
    return (a->flags & AF_USED) && IS_DEMON(a->type) && a->type == lv_boss[t->boss].type && a->hp > 0 &&
           a->state != AST_DYING && a->state != AST_DEAD;
}

static int track_awake(const LvTrack *t)
{
    int st;
    if (!track_alive(t)) return 0;
    st = doomo_actors[t->actor].state;
    return st != AST_IDLE && st != AST_DORMANT && st != AST_APPEAR;
}

int lv_bosses_alive(int awake)
{
    int i, n = 0;
    for (i = 0; i < LV_TRACK; i++)
        if (lv_run.track[i].actor >= 0 && (awake ? track_awake(&lv_run.track[i]) : track_alive(&lv_run.track[i]))) n++;
    return n;
}

/* ---- conveyors (before combat_tick) ---------------------------------------- */
static const int8_t conv_dx[4] = { 1, 0, -1, 0 }, conv_dy[4] = { 0, 1, 0, -1 };

static int conveyor_dir(int cx, int cy)
{
    const DoomoCell *c = map_cell_safe(&doomo_map, cx, cy);
    if (c->wall || c->tag < LV_TAG_CONVEYOR_E || c->tag > LV_TAG_CONVEYOR_N) return -1;
    if (c->flags & (CELL_DOOR | CELL_LIFT | CELL_TRIGGER)) return -1;
    return c->tag - LV_TAG_CONVEYOR_E;
}

static void conveyors(void)
{
    int cx = FIX2INT(doomo_player.x) / CELLU, cy = FIX2INT(doomo_player.y) / CELLU, d, i;
    if (doomo_player.hp > 0 && doomo_player.on_ground && (d = conveyor_dir(cx, cy)) >= 0 &&
        FIX2INT(doomo_player.z) <= lv_cell_floor(cx, cy) + 2) {
        fix16 nx = doomo_player.x + INT2FIX(2 * conv_dx[d]), ny = doomo_player.y + INT2FIX(2 * conv_dy[d]);
        int fl, ce, z = FIX2INT(doomo_player.z);
        if (cw_fits(nx, ny, PLAYER_RADIUS, z, doomo_player.crouch ? DOOMO_CROUCH_HEIGHT : DOOMO_PLAYER_HEIGHT,
                    DOOMO_STEP_UP, &fl, &ce) && fl <= z + 2) {
            doomo_player.x = nx;
            doomo_player.y = ny;
        }
    }
    /* the goodies ride the belts too (and pile up at the end) */
    if ((doomo_map.time & 1) == 0) return;
    for (i = 0; i < ACTOR_MAX; i++) {
        DoomoActor *a = &doomo_actors[i];
        int acx, acy, nx, ny;
        if (!(a->flags & AF_PICKUP)) continue;
        acx = FIX2INT(a->x) / CELLU;
        acy = FIX2INT(a->y) / CELLU;
        if ((d = conveyor_dir(acx, acy)) < 0) continue;
        nx = (FIX2INT(a->x) + 15 * conv_dx[d]) / CELLU;            /* 15 units ahead */
        ny = (FIX2INT(a->y) + 15 * conv_dy[d]) / CELLU;
        if (cw_solid(nx, ny) || lv_cell_floor(nx, ny) > lv_cell_floor(acx, acy)) continue;
        a->x += INT2FIX(3 * conv_dx[d]);
        a->y += INT2FIX(3 * conv_dy[d]);
        if (lv_cell_floor(nx, ny) < FIX2INT(a->z)) a->z = INT2FIX(lv_cell_floor(nx, ny));
    }
}

void lv_runtime_pre(void)
{
    if (lv_conveyors) conveyors();
}

/* ---- mechanics (after combat_tick) -------------------------------------------- */
static void summon(const LvTrack *t, const LevelBoss *b)
{
    const DoomoActor *a = &doomo_actors[t->actor];
    int tries, n = 1 + (t->hp_max > 0 && a->hp < t->hp_max / 3);
    for (tries = 0; tries < 6 && n > 0; tries++) {
        int ang = rng_range(&doomo_rng, 4096), fl, ce;
        fix16 x = a->x + FIXMUL(fx_cos(ang), INT2FIX(80)), y = a->y + FIXMUL(fx_sin(ang), INT2FIX(80));
        int cx = FIX2INT(x) / CELLU, cy = FIX2INT(y) / CELLU, k;
        if (cw_solid(cx, cy) || (map_cell_safe(&doomo_map, cx, cy)->flags & CELL_LAVA)) continue;
        if (!cw_fits(x, y, 18, cw_floor(cx, cy), 56, 24, &fl, &ce)) continue;
        k = actor_spawn_demon(b->add, x, y, 1, 0, 0);
        if (k >= 0) { doomo_actors[k].target = TARGET_PLAYER; n--; }
    }
}

static void blink(const LvTrack *t)
{
    DoomoActor *a = &doomo_actors[t->actor];
    int pcx = FIX2INT(doomo_player.x) / CELLU, pcy = FIX2INT(doomo_player.y) / CELLU, tries;
    int pz = combat_player_z();
    for (tries = 0; tries < 10; tries++) {
        int cx = pcx + rng_between(&doomo_rng, -6, 6), cy = pcy + rng_between(&doomo_rng, -6, 6), fl, ce, z;
        int ddx = cx - pcx, ddy = cy - pcy;
        fix16 x = INT2FIX(cx * CELLU + CELLU / 2), y = INT2FIX(cy * CELLU + CELLU / 2);
        if (ddx * ddx + ddy * ddy < 9 || cw_solid(cx, cy)) continue;
        fl = cw_floor(cx, cy);
        if (fl < pz - 48 || fl > pz + 48) continue;
        z = fl + ((a->flags & AF_FLYING) ? 24 : 0);
        if (!cw_fits(x, y, a->radius, z, a->height, 24, &fl, &ce)) continue;
        if (!cw_los(x, y, z + a->height / 2, doomo_player.x, doomo_player.y, pz + 40)) continue;
        actor_fx(SPR_PUFF, a->x, a->y, a->z + INT2FIX(a->height / 3), 3);
        a->x = x;
        a->y = y;
        a->z = INT2FIX(z);
        a->vx = a->vy = 0;
        actor_fx(SPR_PUFF, a->x, a->y, a->z + INT2FIX(a->height / 3), 3);
        actor_fx(SPR_SPARKLE, a->x, a->y, a->z + INT2FIX(a->height), 3);
        audio_sfx(SFX_POSSESS);
        return;
    }
}

static void organ_fire(const LvTrack *t)
{
    int k, n = lv_run.organ_n;
    if (!n) return;
    for (k = 0; k < 2; k++) {
        int s = rng_range(&doomo_rng, n), z0, ang, tz;
        fix16 x = INT2FIX(lv_run.organ_x[s] * CELLU + CELLU / 2), y = INT2FIX(lv_run.organ_y[s] * CELLU + 6);
        fix16 dist, slope;
        if (cw_solid(lv_run.organ_x[s], lv_run.organ_y[s])) y = INT2FIX(lv_run.organ_y[s] * CELLU + CELLU / 2);
        z0 = cw_floor(lv_run.organ_x[s], lv_run.organ_y[s]) + 70 + rng_range(&doomo_rng, 60);
        tz = combat_player_z() + 32;
        ang = fx_atan2(doomo_player.y - y, doomo_player.x - x) + rng_between(&doomo_rng, -60, 60);
        dist = fx_dist(doomo_player.x - x, doomo_player.y - y);
        if (dist < INT2FIX(16)) dist = INT2FIX(16);
        slope = FIXDIV(INT2FIX(tz - z0), dist);
        actor_projectile(PK_FIREBALL, t->actor, x, y, INT2FIX(z0), ang & DOOMO_ANG_MASK, slope, 11, 7, 0, 0, NO_TARGET);
        actor_fx(SPR_EXPLOSION, x, y, INT2FIX(z0), 2);
    }
    audio_sfx(SFX_BELL);
    combat_shake(3);
}

static void portal_spawn(const LevelBoss *b)
{
    int s, a, fl, ce;
    fix16 x, y;
    if (!lv_run.portal_n || actor_demons_alive() >= 10) return;
    s = rng_range(&doomo_rng, lv_run.portal_n);
    x = INT2FIX(lv_run.portal_x[s] * CELLU + CELLU / 2);
    y = INT2FIX(lv_run.portal_y[s] * CELLU + CELLU / 2);
    if (fx_dist(x - doomo_player.x, y - doomo_player.y) < INT2FIX(2 * CELLU)) return;
    if (!cw_fits(x, y, 18, cw_floor(lv_run.portal_x[s], lv_run.portal_y[s]), 56, 24, &fl, &ce)) return;
    a = actor_spawn_demon(b->add, x, y, 1, 0, 0);
    if (a >= 0) doomo_actors[a].target = TARGET_PLAYER;
}

static void boss_died(LvTrack *t)
{
    const LevelBoss *b = &lv_boss[t->boss];
    t->dead = 1;
    t->actor = -1;
    combat_toast(b->death);
    if (t->boss <= LB_GRUMBLEGUTS) unlock_grant(LVU_BOSS0 + t->boss);
    combat_shake(14);
    audio_sfx(SFX_DEMON_DIE);
}

static void music_tick(void)
{
    int bosses = lv_bosses_alive(1);
    if (lv_run.level == 255) return;
    if (!lv_run.music_hell && combat_s.hell >= lv_info[lv_run.level].hell_at) {
        lv_run.music_hell = 1;
        if (!lv_run.in_boss) audio_music(lv_info[lv_run.level].hell_song);
    }
    if (bosses && !lv_run.in_boss) {
        lv_run.in_boss = 1;
        audio_music(SONG_BOSS);
    } else if (!bosses && lv_run.in_boss && !combat_s.finished) {
        lv_run.in_boss = 0;
        audio_music(lv_run.music_hell ? lv_info[lv_run.level].hell_song : lv_run.base_music);
    }
}

void lv_runtime_post(void)
{
    int i;
    if (lv_run.intro_t) lv_run.intro_t--;
    for (i = 0; i < LV_TRACK; i++) {
        LvTrack *t = &lv_run.track[i];
        const LevelBoss *b;
        if (t->actor < 0) continue;
        b = &lv_boss[t->boss];
        if (!track_alive(t)) {
            boss_died(t);
            if (lv_run.bar == i) lv_run.bar = -1;
            continue;
        }
        if (!track_awake(t)) continue;
        if (!t->woke) {
            t->woke = 1;
            lv_run.intro_boss = t->boss;
            lv_run.intro_t = 80;
            audio_sfx(SFX_DEMON_ROAR);
            combat_shake(8);
        }
        if (lv_run.bar < 0 || !track_alive(&lv_run.track[lv_run.bar])) lv_run.bar = (int8_t)i;
        t->t++;
        if ((b->flags & LBF_SUMMON) && t->t % 210 == 105 && doomo_actors[t->actor].hp < t->hp_max * 4 / 5 &&
            actor_demons_alive() < 10)
            summon(t, b);
        if ((b->flags & LBF_BLINK) && t->t % 150 == 75) blink(t);
        if ((b->flags & LBF_ORGAN) && t->t % 75 == 40) organ_fire(t);
        if ((b->flags & LBF_PORTALS) && t->t % 180 == 90) portal_spawn(b);
    }
    music_tick();
}

/* ---- the view: bigger, glowing, ghostly variants + their tint ------------------- */
static DoomoBillboard *bb_of(DoomoView3D *v, const DoomoActor *a)
{
    int k;
    /* the billboards are built from the actors (combat_build_view): the
     * same feet position identifies one */
    DoomoBillboard *bbs = (DoomoBillboard *)v->sprites;
    for (k = 0; k < v->sprite_count; k++)
        if (bbs[k].x == a->x && bbs[k].y == a->y && k != v->player_sprite) return &bbs[k];
    return NULL;
}

void lv_runtime_view(DoomoView3D *v)
{
    static const uint16_t size_scale[4] = { 256, 150, 384, 540 };
    int i, tint = -1, lvl = 0;
    uint16_t time = doomo_map.time;
    for (i = 0; i < LV_TRACK; i++) {
        const LvTrack *t = &lv_run.track[i];
        const LevelBoss *b;
        const DoomoActor *a;
        DoomoBillboard *bb;
        if (t->actor < 0) continue;
        a = &doomo_actors[t->actor];
        b = &lv_boss[t->boss];
        bb = bb_of(v, a);
        if (bb) {
            int glow = b->glow;
            bb->scale = (uint16_t)DOOMO_MIN((uint32_t)bb->scale * b->scale / 256, 2000u);
            if (b->flags & LBF_PULSE) glow += 2 + ui_wave(time, 24, 2);
            if (b->flags & LBF_GHOST) {
                bb->flags |= BB_TRANSLUCENT;
                if (((time >> 3) % 5) == 0) bb->flags &= (uint8_t)~BB_TRANSLUCENT;   /* flicker */
            }
            bb->light = (uint8_t)DOOMO_CLAMP(bb->light + glow, 0, 15);
        }
        if (track_awake(t) && b->tint_lvl) { tint = b->tint; lvl = b->tint_lvl + (ui_wave(time, 40, 2) > 0); }
    }
    /* mayhem sizes */
    for (i = 0; i < ACTOR_MAX; i++) {
        const DoomoActor *a;
        DoomoBillboard *bb;
        if (!lv_run.size_cls[i]) continue;
        a = &doomo_actors[i];
        if (!(a->flags & AF_USED) || !IS_DEMON(a->type)) { lv_run.size_cls[i] = 0; continue; }
        bb = bb_of(v, a);
        if (bb) bb->scale = (uint16_t)((uint32_t)bb->scale * size_scale[lv_run.size_cls[i] & 3] / 256);
    }
    if (tint >= 0 && v->tint_level == 0) {
        v->tint = (uint8_t)tint;
        v->tint_level = (uint8_t)(opt_get(OPT_REDUCE_FLASH) ? lvl / 2 : lvl);
    }
}

void lv_runtime_hud(void)
{
    int saved = combat_s.boss, show = lv_run.bar >= 0 && track_awake(&lv_run.track[lv_run.bar]);
    if (show) combat_s.boss = -1;                     /* hide the stock bar */
    combat_draw_hud();
    combat_s.boss = saved;
    if (show) {
        const LvTrack *t = &lv_run.track[lv_run.bar];
        lvd_boss_bar(t->boss, doomo_actors[t->actor].hp, t->hp_max, doomo_map.time);
    }
    if (lv_run.intro_t && lv_run.intro_boss != LB_NONE) lvd_boss_intro(lv_run.intro_boss, 80 - lv_run.intro_t);
}

/* ======================================================================
 * the activities (Arena + Hellvile, free play and story)
 * ==================================================================== */
#define FP ACTF_FREEPLAY
#define BOTH (ACTF_FREEPLAY | ACTF_STORY)
static const DoomoActivity lv_activities[] = {
    { "Hell Breach Levels", "Seven levels. Ranks S to C. Go!", ACTLOC_ARENA, BOTH,
      ICON_PENTAGRAM, 3, ACT_ALWAYS, &SCENE_LEVEL_SELECT, LVM_NORMAL },
    { "Boss Rush", "Seven bosses, back to back", ACTLOC_ARENA, BOTH,
      ICON_SKULL, 3, 531, &SCENE_ARENA_MODE, AM_BOSS_RUSH },
    { "Horde Mode", "Endless waves + shop breaks", ACTLOC_ARENA, BOTH,
      ICON_FIRE, 3, 530, &SCENE_ARENA_MODE, AM_HORDE },
    { "Time Trial", "Speedrun cleared levels for gold", ACTLOC_ARENA, BOTH,
      ICON_BOLT, 3, LVU_TIME_TRIAL, &SCENE_LEVEL_SELECT, LVM_TIME_TRIAL },
    { "Cute-Only Run", "Cute weapons only. Confetti!", ACTLOC_ARENA, FP,
      ICON_HEART, 0, LVU_CHALLENGES, &SCENE_LEVEL_SELECT, LVM_CUTE_ONLY },
    { "Brutal-Only Run", "Brutal weapons only. Gibs!", ACTLOC_ARENA, FP,
      ICON_BOMB, 0, LVU_CHALLENGES, &SCENE_LEVEL_SELECT, LVM_BRUTAL_ONLY },
    { "Monster Mode Mayhem", "Three minutes of pure chaos", ACTLOC_ARENA, FP,
      ICON_GHOST, 0, LVU_MAYHEM, &SCENE_ARENA_MODE, AM_MAYHEM },
    { "Underbelly Gate", "Down the manhole: Hell Breach 7", ACTLOC_HELLVILE, BOTH,
      ICON_PENTAGRAM, 4, ACT_ALWAYS, &SCENE_LEVEL_SELECT, LVM_NORMAL | ((LV_UNDERBELLY + 1) << 4) },
    { "Graveyard Shift", "Night shift at the cemetery", ACTLOC_HELLVILE, BOTH | ACTF_NIGHT_ONLY,
      ICON_GHOST, 4, ACT_ALWAYS, &SCENE_LEVEL_SELECT, LVM_NORMAL | ((LV_GRAVEYARD + 1) << 4) },
    { NULL, NULL, 0, 0, 0, 0, 0, NULL, 0 }
};
const DoomoActivity *const doomo_activities_combat = lv_activities;
