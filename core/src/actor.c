/* actor.c - everything that lives in a combat map except the player
 * (owner: action): demons and their AI, projectiles, effects, gibs,
 * pickups and props, plus the world queries the shooter uses (fits,
 * line of sight, ray traces) and the damage / death / gore rules.
 *
 * Demon AI (per type, see combat_demon_info):
 *   CHASER    walks at you, claws in melee
 *   SKIRMISH  approaches while strafing left/right, shoots often
 *   FLYER     hovers at eye height (uses z), circles, ranged volleys
 *   CHARGER   sprints and lunges (hellhound pups)
 *   DIVER     flies high and dive-bombs (gargoyles)
 *   SNIPER    keeps its distance, flanks, fires bursts (cyber piggy)
 *   TANK      slow, heavy melee + lobbed globs
 *   BOSS      Lord Vapula, three phases
 * Everyone: wakes on sight / noise, pain chance, infighting (a demon hurt
 * by another demon's shot turns on it), charm (fights for you), freeze /
 * dazzle (slowed, can't attack), burning, death with CUTE or BRUTAL gore.
 * AI thinks are staggered so the per-tick cost stays bounded.
 */
#include "combat_int.h"

DoomoActor doomo_actors[ACTOR_MAX];

static uint8_t  ai_move[ACTOR_MAX];     /* ticks to keep the current move dir */
static int16_t  ai_dir[ACTOR_MAX];      /* current move direction (angle)      */
static uint8_t  ai_flash[ACTOR_MAX];    /* hit flash ticks                     */
static uint8_t  ai_los[ACTOR_MAX];      /* cached line of sight to the target  */
static int      corpse_count;

#define GRAVITY_FX   ((fix16)75366)          /* 1.15 world units / tick^2 */

/* ---- tables --------------------------------------------------------------- */
const DemonInfo combat_demon_info[THING_LORD_VAPULA - THING_IMPLING + 1] = {
    /* name, blurb, spr, hp, r, h, scale, speed, ai, pain, mdmg, mrange, pk, sdmg, sspd, scool, burst, mass, score, sight */
    { "Impling", "A plush little devil who throws hot fireballs and hugs with claws.",
      SPR_IMPLING, 60, 16, 50, 310, 24, AI_SKIRMISH, 200, 9, 56, PK_FIREBALL, 10, 14, 45, 1, 50, 10, 20 },
    { "Vape Wraith", "A ghost made of Brimstone Blast flavour. Floats, spits vape bolts.",
      SPR_VAPE_WRAITH, 110, 18, 56, 330, 32, AI_FLYER, 140, 12, 56, PK_VAPE_BOLT, 10, 16, 50, 3, 40, 25, 22 },
    { "Puff Demon", "A floating cyclops puffball. Slow, sturdy, bites hard.",
      SPR_PUFF_DEMON, 200, 26, 50, 340, 19, AI_FLYER, 128, 15, 64, PK_VAPE_BOLT, 16, 13, 55, 1, 120, 40, 22 },
    { "Hellhound Pup", "Too fast, too floofy. Sprints and lunges.",
      SPR_HELLHOUND_PUP, 100, 18, 44, 300, 44, AI_CHARGER, 180, 12, 56, PK_NONE, 0, 0, 40, 1, 90, 20, 20 },
    { "Bone Jester", "Juggles homing orbs and dodges like it rehearsed.",
      SPR_BONE_JESTER, 180, 16, 64, 310, 32, AI_SKIRMISH, 100, 20, 60, PK_ORB, 18, 12, 60, 1, 100, 50, 24 },
    { "Slime Belcher", "Burps arcing slime globs. Squishy, smelly, persistent.",
      SPR_SLIME_BELCHER, 150, 20, 50, 310, 16, AI_TANK, 120, 12, 60, PK_SLIME, 20, 13, 50, 1, 140, 35, 18 },
    { "Gargoyle", "A stone bat that wakes up grumpy and dives from above.",
      SPR_GARGOYLE, 140, 20, 56, 330, 40, AI_DIVER, 140, 18, 60, PK_NONE, 0, 0, 50, 1, 100, 40, 24 },
    { "Cyber Piggy", "Oink with an arm cannon. Keeps its distance, fires bursts.",
      SPR_CYBER_PIGGY, 220, 20, 58, 330, 20, AI_SNIPER, 90, 15, 60, PK_FIREBALL, 10, 16, 70, 4, 160, 60, 28 },
    { "Brute", "A wall of angry muscle. Punches, throws acid, never skips leg day.",
      SPR_BRUTE, 400, 24, 88, 384, 24, AI_TANK, 50, 35, 72, PK_SLIME, 25, 15, 60, 1, 250, 100, 24 },
    { "Cloud Spider", "Eight legs, one fluffy cloud. Scuttles and sprays vape.",
      SPR_CLOUD_SPIDER, 250, 26, 46, 330, 28, AI_SKIRMISH, 80, 15, 64, PK_VAPE_BOLT, 8, 18, 40, 5, 180, 80, 24 },
    { "Lord Vapula", "Duke of the Ninth Cloud. Three phases of vapour and spite.",
      SPR_LORD_VAPULA, 3000, 36, 130, 460, 20, AI_BOSS, 20, 40, 96, PK_VAPE_BOLT, 15, 15, 30, 3, 255, 250, 40 },
};

const ProjInfo combat_proj_info[PK_COUNT] = {
    /* spr, scale, gravity x16, radius, bounces, fullbright, impact, trail */
    { SPR_SPARKLE, 256, 0, 4, 0, 1, SPR_PUFF, 0 },                 /* NONE */
    { SPR_FIREBALL, 320, 0, 8, 0, 1, SPR_EXPLOSION, 0 },           /* FIREBALL */
    { SPR_VAPE_BOLT, 300, 0, 8, 0, 1, SPR_PUFF, 0 },               /* VAPE_BOLT */
    { SPR_VAPE_BOLT, 400, 0, 10, 0, 1, SPR_EXPLOSION, SPR_SPARKLE },/* ORB */
    { ART_BASE + ART_SLIME, 380, 12, 8, 0, 0, ART_BASE + ART_SPLAT_GREEN, 0 }, /* SLIME */
    { SPR_PLASMA, 280, 0, 8, 0, 1, SPR_SPARKLE, 0 },               /* PLASMA */
    { SPR_ROCKET, 300, 0, 8, 0, 1, SPR_EXPLOSION, SPR_PUFF },      /* ROCKET */
    { SPR_ROCKET, 220, 16, 6, 3, 1, SPR_EXPLOSION, 0 },            /* GRENADE */
    { ART_BASE + ART_NAIL, 300, 0, 4, 0, 0, SPR_SPARKLE, 0 },      /* NAIL */
    { ART_BASE + ART_DART, 300, 2, 4, 0, 0, SPR_SPARKLE, 0 },      /* ARROW */
    { SPR_BUBBLE, 420, 0, 10, 0, 0, SPR_HOLY_SPLASH, 0 },          /* BUBBLE */
    { ART_BASE + ART_PLUSH, 380, 14, 8, 3, 0, SPR_STUFFING, 0 },   /* PLUSH */
    { SPR_HEARTS, 160, 0, 8, 0, 1, SPR_HEARTS, 0 },                /* HEART */
    { SPR_GLITTER_SHOT, 220, 0, 8, 0, 1, SPR_SPARKLE, 0 },         /* GLITTER */
    { ART_BASE + ART_WATER, 300, 4, 6, 0, 0, SPR_HOLY_SPLASH, 0 }, /* WATER */
    { ART_BASE + ART_STICKER, 300, 0, 6, 0, 0, SPR_CONFETTI, 0 },  /* STICKER */
    { ART_BASE + ART_YARN, 340, 2, 8, 0, 0, SPR_STUFFING, 0 },     /* YARN */
    { ART_BASE + ART_SAW, 380, 0, 8, 3, 0, SPR_SPARKLE, 0 },       /* SAW */
    { SPR_FIREBALL, 220, 0, 10, 0, 1, SPR_PUFF, 0 },               /* FLAME */
    { SPR_PLASMA, 900, 0, 16, 0, 1, SPR_EXPLOSION, SPR_SPARKLE },  /* BFG */
    { SPR_CANDY, 220, 0, 6, 0, 0, SPR_SPARKLE, 0 },                /* CANDY */
    { ART_BASE + ART_WATER, 340, 6, 8, 0, 1, SPR_HOLY_SPLASH, 0 }, /* HOLY */
    { SPR_CONFETTI, 160, 0, 8, 0, 1, SPR_CONFETTI, 0 },            /* CONFETTI */
    { ART_BASE + ART_ICE, 300, 0, 6, 0, 1, SPR_SPARKLE, 0 },       /* ICE */
};

/* ---- world queries ----------------------------------------------------------- */
int cw_floor(int cx, int cy) { return map_floor_z(&doomo_map, cx, cy); }

int cw_ceil(int cx, int cy)
{
    const DoomoCell *c = map_cell_safe(&doomo_map, cx, cy);
    if (c->ceil_tex == DOOMO_TEX_SKY && !c->wall) return SKY_CEIL;
    return map_ceil_z(&doomo_map, cx, cy);
}

int cw_solid(int cx, int cy) { return map_cell_safe(&doomo_map, cx, cy)->wall != 0; }

int cw_fits(fix16 x, fix16 y, int r, int z, int h, int step, int *fl, int *ce)
{
    int x0 = (FIX2INT(x) - r) / CELLU, x1 = (FIX2INT(x) + r) / CELLU;
    int y0 = (FIX2INT(y) - r) / CELLU, y1 = (FIX2INT(y) + r) / CELLU;
    int cx, cy, maxf = -30000, minc = SKY_CEIL;
    if (x < INT2FIX(r) || y < INT2FIX(r)) return 0;
    for (cy = y0; cy <= y1; cy++)
        for (cx = x0; cx <= x1; cx++) {
            int f, c;
            if (cw_solid(cx, cy)) return 0;
            f = cw_floor(cx, cy);
            c = cw_ceil(cx, cy);
            if (f > z + step) return 0;
            if (f > maxf) maxf = f;
            if (c < minc) minc = c;
        }
    if (fl) *fl = maxf;
    if (ce) *ce = minc;
    return minc - DOOMO_MAX(maxf, z) >= h;
}

/* Grid march along a 2D direction: calls back nothing, returns the world
 * hit distance (fix16) of a ray with slope (dz per unit of distance, fix16)
 * starting at z0 (fix16), stopping at maxd. */
fix16 cw_trace(fix16 x0, fix16 y0, fix16 z0, fix16 dx, fix16 dy, fix16 slope,
               fix16 maxd, fix16 *hx, fix16 *hy, fix16 *hz)
{
    int cx = FIX2INT(x0) / CELLU, cy = FIX2INT(y0) / CELLU;
    int stepx = dx < 0 ? -1 : 1, stepy = dy < 0 ? -1 : 1, i;
    int64_t adx = dx < 0 ? -(int64_t)dx : dx, ady = dy < 0 ? -(int64_t)dy : dy;
    int64_t ddx = adx ? (((int64_t)CELLU << 32) / adx) : ((int64_t)1 << 40);  /* fix16 dist per cell */
    int64_t ddy = ady ? (((int64_t)CELLU << 32) / ady) : ((int64_t)1 << 40);
    int64_t sx, sy, d = 0, dhit = maxd;
    int64_t cellx0 = (int64_t)cx * CELLU << 16, celly0 = (int64_t)cy * CELLU << 16;
    if (dx < 0) sx = adx ? ((int64_t)(x0 - cellx0) << 16) / adx : ((int64_t)1 << 40);
    else sx = adx ? (((cellx0 + ((int64_t)CELLU << 16)) - x0) << 16) / adx : ((int64_t)1 << 40);
    if (dy < 0) sy = ady ? ((int64_t)(y0 - celly0) << 16) / ady : ((int64_t)1 << 40);
    else sy = ady ? (((celly0 + ((int64_t)CELLU << 16)) - y0) << 16) / ady : ((int64_t)1 << 40);
    for (i = 0; i < 160; i++) {
        int64_t dnext = sx < sy ? sx : sy;
        int fl = cw_floor(cx, cy), ce = cw_ceil(cx, cy);
        /* (integration) "* 65536" instead of "<< 16" on values that can be
         * negative (UBSan: left shift of negative value); same code. */
        int64_t zf = (int64_t)fl * 65536, zc = (int64_t)ce * 65536;
        /* floor / ceiling planes inside this cell */
        if (slope < 0) {
            int64_t dfl = ((zf - z0) * 65536) / slope;        /* both negative -> positive */
            if (z0 + ((slope * d) >> 16) <= zf) { dhit = d; break; }
            if (dfl >= d && dfl < dnext) { dhit = dfl; break; }
        } else if (slope > 0 && ce < SKY_CEIL) {
            int64_t dce = ((zc - z0) * 65536) / slope;
            if (z0 + ((slope * d) >> 16) >= zc) { dhit = d; break; }
            if (dce >= d && dce < dnext) { dhit = dce; break; }
        } else {
            int64_t zz = z0;
            if (zz < zf || (ce < SKY_CEIL && zz > zc)) { dhit = d; break; }
        }
        if (dnext >= maxd) { dhit = maxd; break; }
        d = dnext;
        if (sx < sy) { sx += ddx; cx += stepx; } else { sy += ddy; cy += stepy; }
        if (cw_solid(cx, cy)) { dhit = d; break; }
        {
            int64_t z = z0 + ((slope * d) >> 16);
            int nf = cw_floor(cx, cy), nc = cw_ceil(cx, cy);
            if (z < ((int64_t)nf * 65536) || (nc < SKY_CEIL && z > ((int64_t)nc * 65536))) { dhit = d; break; }
        }
    }
    if (dhit > maxd) dhit = maxd;
    if (dhit < 0) dhit = 0;
    if (hx) *hx = x0 + (fix16)((dx * dhit) >> 16);
    if (hy) *hy = y0 + (fix16)((dy * dhit) >> 16);
    if (hz) *hz = z0 + (fix16)((slope * dhit) >> 16);
    return (fix16)dhit;
}

int cw_los(fix16 x0, fix16 y0, int z0, fix16 x1, fix16 y1, int z1)
{
    fix16 ddx = x1 - x0, ddy = y1 - y0;
    fix16 dist = fx_dist(ddx, ddy), d;
    fix16 dirx, diry, slope;
    if (dist < INT2FIX(4)) return 1;
    dirx = FIXDIV(ddx, dist);
    diry = FIXDIV(ddy, dist);
    slope = FIXDIV(INT2FIX(z1 - z0), dist);
    d = cw_trace(x0, y0, INT2FIX(z0), dirx, diry, slope, dist, NULL, NULL, NULL);
    return d >= dist - INT2FIX(2);
}

/* ---- allocation --------------------------------------------------------------- */
void actors_clear(void)
{
    memset(doomo_actors, 0, sizeof(doomo_actors));
    memset(ai_move, 0, sizeof(ai_move));
    memset(ai_flash, 0, sizeof(ai_flash));
    memset(ai_los, 0, sizeof(ai_los));
    corpse_count = 0;
}

int actor_alloc(int effect)
{
    int i, oldest = -1, oldest_t = -1;
    for (i = 0; i < ACTOR_MAX; i++)
        if (!(doomo_actors[i].flags & AF_USED)) {
            memset(&doomo_actors[i], 0, sizeof(DoomoActor));
            doomo_actors[i].flags = AF_USED;
            doomo_actors[i].target = NO_TARGET;
            ai_move[i] = 0;
            ai_flash[i] = 0;
            return i;
        }
    /* full: recycle the oldest effect / gib (never for important actors
     * unless the caller is one: then recycle an effect anyway) */
    for (i = 0; i < ACTOR_MAX; i++) {
        DoomoActor *a = &doomo_actors[i];
        if ((a->type == AK_EFFECT || a->type == AK_GIB) && a->timer > oldest_t) {
            oldest = i;
            oldest_t = a->timer;
        }
    }
    if (oldest < 0 && !effect) {
        /* last resort: the oldest corpse */
        for (i = 0; i < ACTOR_MAX; i++)
            if (doomo_actors[i].flags & AF_CORPSE) { oldest = i; corpse_count--; break; }
    }
    if (oldest < 0) return -1;
    memset(&doomo_actors[oldest], 0, sizeof(DoomoActor));
    doomo_actors[oldest].flags = AF_USED;
    doomo_actors[oldest].target = NO_TARGET;
    return oldest;
}

static int cell_center(int c) { return c * CELLU + CELLU / 2; }

int actor_fx(int spr, fix16 x, fix16 y, fix16 z, int speed_ticks)
{
    int i = actor_alloc(1);
    DoomoActor *a;
    if (i < 0) return -1;
    a = &doomo_actors[i];
    a->type = AK_EFFECT;
    a->spr = (uint8_t)spr;
    a->x = x; a->y = y; a->z = z;
    a->cooldown = (uint8_t)(speed_ticks > 0 ? speed_ticks : 3);
    a->hp = 0;
    return i;
}

int actor_gib(int spr, fix16 x, fix16 y, fix16 z, int vx, int vy, int vz)
{
    int i = actor_alloc(1);
    DoomoActor *a;
    if (i < 0) return -1;
    a = &doomo_actors[i];
    a->type = AK_GIB;
    a->spr = (uint8_t)spr;
    a->x = x; a->y = y; a->z = z;
    a->vx = vx; a->vy = vy; a->vz = vz;
    a->hp = 240;                                    /* lifetime ticks */
    a->strafe = (int8_t)(rng_range(&doomo_rng, 4));
    return i;
}

static int difficulty_skip(int tflags)
{
    int d = combat_difficulty();
    if ((tflags & THF_NOT_EASY) && d == DIFF_PLUSHIE) return 1;
    if ((tflags & THF_HARD) && d < DIFF_NIGHTMARE) return 1;
    return 0;
}

int actor_spawn_demon(int type, fix16 x, fix16 y, int appear, int tflags, int arg)
{
    const DemonInfo *di;
    DoomoActor *a;
    int i, fl;
    if (!IS_DEMON(type)) return -1;
    i = actor_alloc(0);
    if (i < 0) return -1;
    di = DEMON(type);
    a = &doomo_actors[i];
    a->type = (uint8_t)type;
    a->x = x; a->y = y;
    fl = cw_floor(FIX2INT(x) / CELLU, FIX2INT(y) / CELLU);
    a->z = INT2FIX(fl);
    if (di->ai == AI_FLYER || di->ai == AI_DIVER || di->ai == AI_BOSS) {
        a->flags |= AF_FLYING;
        a->z = INT2FIX(fl + 24);
    }
    a->hp = (int16_t)di->hp;
    if (tflags & THF_BOSS) {
        a->flags |= AF_BOSS;
        if (type != THING_LORD_VAPULA) a->hp = (int16_t)(di->hp * 4);
    }
    if (combat_difficulty() == DIFF_PLUSHIE) a->hp = (int16_t)(a->hp * 3 / 4);
    if (combat_difficulty() == DIFF_ULTRA_HELLVILE) a->hp = (int16_t)(a->hp * 5 / 4);
    a->radius = di->radius;
    a->height = di->height;
    if (opt_get(OPT_CHEAT_TINY_DEMONS)) {
        a->radius = (uint8_t)(a->radius * 3 / 5);
        a->height = (uint8_t)(a->height * 3 / 5);
    }
    a->flags |= AF_SOLID | AF_SHOOTABLE | AF_COUNTKILL;
    a->spr = di->spr;
    a->tflags = (uint8_t)tflags;
    a->arg = (uint8_t)arg;
    a->strafe = (int8_t)(rng_range(&doomo_rng, 2) ? 1 : -1);
    a->cooldown = (uint8_t)(20 + rng_range(&doomo_rng, 30));
    a->angle = (int16_t)rng_range(&doomo_rng, 4096);
    a->anim = (uint16_t)rng_range(&doomo_rng, 64);
    a->target = NO_TARGET;
    combat_st.kills_total++;
    if (tflags & THF_DORMANT) {
        a->state = AST_DORMANT;
        a->flags &= (uint8_t)~(AF_SOLID | AF_SHOOTABLE);
    } else if (appear) {
        a->state = AST_APPEAR;
        a->flags &= (uint8_t)~(AF_SOLID | AF_SHOOTABLE);
        a->target = TARGET_PLAYER;
        actor_fx(SPR_PUFF, a->x, a->y, a->z + INT2FIX(a->height / 3), 3);
    } else {
        a->state = AST_IDLE;
    }
    return i;
}

static int pickup_spr(int type)
{
    switch (type) {
    case THING_HEALTH_SMALL: return SPR_HEALTH_SMALL;
    case THING_HEALTH_BIG: return SPR_HEALTH_BIG;
    case THING_ARMOR: return SPR_ARMOR;
    case THING_AMMO_BULLETS: return SPR_AMMO_BULLETS;
    case THING_AMMO_SHELLS: return SPR_AMMO_SHELLS;
    case THING_AMMO_CELLS: return SPR_AMMO_CELLS;
    case THING_AMMO_ROCKETS: return SPR_AMMO_ROCKETS;
    case THING_KEY_RED: return SPR_KEY_RED;
    case THING_KEY_BLUE: return SPR_KEY_BLUE;
    case THING_SCRAP: return SPR_SCRAP;
    case THING_SOUL_ORB: return SPR_SOUL_ORB;
    case THING_CANDY: return SPR_CANDY;
    default: return 0;
    }
}

static int prop_spr(int type, int *solid, int *radius)
{
    *solid = 1;
    *radius = 12;
    switch (type) {
    case THING_PROP_TREE: *radius = 18; return SPR_TREE;
    case THING_PROP_LAMP: *radius = 8; return SPR_LAMP;
    case THING_PROP_BENCH: *radius = 14; return SPR_BENCH;
    case THING_PROP_FOUNTAIN: *radius = 24; return SPR_FOUNTAIN;
    case THING_PROP_BARREL: *radius = 10; return SPR_BARREL;
    case THING_PROP_SKULLS: *radius = 12; return SPR_SKULL_PILE;
    case THING_PROP_TORCH: *radius = 6; return SPR_TORCH;
    case THING_PROP_SIGN: *radius = 8; return SPR_SIGN;
    case THING_PROP_FLOWERS: *solid = 0; return SPR_FLOWER_BED;
    case THING_PROP_MAILBOX: *radius = 8; return SPR_MAILBOX;
    case THING_VAPE_PORTAL: *solid = 0; return SPR_VAPE_PORTAL;
    default: *solid = 0; return 0;
    }
}

int actor_spawn_thing(const DoomoThing *t, int appear)
{
    fix16 x = INT2FIX(cell_center(t->x)), y = INT2FIX(cell_center(t->y));
    int i, spr, solid, radius;
    DoomoActor *a;
    if (difficulty_skip(t->flags)) return -1;
    if (IS_DEMON(t->type)) {
        i = actor_spawn_demon(t->type, x, y, appear, t->flags, t->arg);
        if (i >= 0) doomo_actors[i].angle = (int16_t)(t->angle * 16);
        return i;
    }
    spr = pickup_spr(t->type);
    if (spr) {
        i = actor_alloc(0);
        if (i < 0) return -1;
        a = &doomo_actors[i];
        a->type = t->type;
        a->x = x; a->y = y;
        a->z = INT2FIX(cw_floor(t->x, t->y));
        a->spr = (uint8_t)spr;
        a->flags |= AF_PICKUP;
        a->tflags = t->flags;
        a->arg = t->arg;
        a->radius = 16;
        a->anim = (uint16_t)(t->x * 7 + t->y * 13);
        return i;
    }
    spr = prop_spr(t->type, &solid, &radius);
    if (!spr) return -1;
    i = actor_alloc(0);
    if (i < 0) return -1;
    a = &doomo_actors[i];
    a->type = t->type;
    a->x = x; a->y = y;
    a->z = INT2FIX(cw_floor(t->x, t->y));
    a->spr = (uint8_t)spr;
    a->radius = (uint8_t)radius;
    a->height = 48;
    if (solid) a->flags |= AF_SOLID;
    if (t->type == THING_PROP_BARREL) { a->flags |= AF_SHOOTABLE; a->hp = 20; a->height = 32; }
    a->anim = (uint16_t)(t->x * 5 + t->y * 3);
    return i;
}

/* ---- helpers ------------------------------------------------------------------- */
static void target_pos(const DoomoActor *a, fix16 *x, fix16 *y, int *z)
{
    if (a->target >= 0 && a->target < ACTOR_MAX && (doomo_actors[a->target].flags & AF_USED)) {
        const DoomoActor *t = &doomo_actors[a->target];
        *x = t->x; *y = t->y; *z = FIX2INT(t->z) + t->height / 2;
    } else {
        int pz;
        combat_player_eye(x, y, &pz);
        *z = pz - 8;
    }
}

static int target_alive(const DoomoActor *a)
{
    if (a->target == TARGET_PLAYER) return doomo_player.hp > 0;
    if (a->target >= 0 && a->target < ACTOR_MAX) {
        const DoomoActor *t = &doomo_actors[a->target];
        return (t->flags & AF_USED) && IS_DEMON(t->type) && t->hp > 0 && t->state != AST_DYING &&
               t->state != AST_DEAD && t->state != AST_DORMANT;
    }
    return 0;
}

static int nearest_enemy_demon(int self)
{
    int i, best = -1;
    fix16 bd = INT2FIX(30000);
    const DoomoActor *s = &doomo_actors[self];
    for (i = 0; i < ACTOR_MAX; i++) {
        const DoomoActor *o = &doomo_actors[i];
        fix16 d;
        if (i == self || !(o->flags & AF_USED) || !IS_DEMON(o->type) || o->hp <= 0) continue;
        if (o->state >= AST_DYING || (o->status_fx & ST_CHARMED)) continue;
        d = fx_dist(o->x - s->x, o->y - s->y);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static int demon_speed(const DoomoActor *a)
{
    const DemonInfo *di = DEMON(a->type);
    int s = di->speed;                                  /* x8 */
    if (combat_difficulty() >= DIFF_ULTRA_HELLVILE) s = s * 5 / 4;
    if (a->status_fx & (ST_FROZEN | ST_DAZZLED)) s /= 2;
    if ((a->flags & AF_BOSS) && a->type == THING_LORD_VAPULA && a->owner >= 2) s = s * 3 / 2;
    return s;
}

/* Try to move a demon by (dx, dy) world fix16. Returns 1 on success. */
static int demon_try_move(int idx, fix16 dx, fix16 dy)
{
    DoomoActor *a = &doomo_actors[idx];
    fix16 nx = a->x + dx, ny = a->y + dy;
    int fl, ce, z = FIX2INT(a->z), i;
    int flying = (a->flags & AF_FLYING) != 0;
    if (flying) {
        if (!cw_fits(nx, ny, a->radius, z, a->height, 255, &fl, &ce)) return 0;
        if (z < fl) return 0;
    } else {
        if (!cw_fits(nx, ny, a->radius, z, a->height, DOOMO_STEP_UP, &fl, &ce)) return 0;
        /* walkers avoid lava and big drops unless chasing hard */
        {
            const DoomoCell *c = map_cell_safe(&doomo_map, FIX2INT(nx) / CELLU, FIX2INT(ny) / CELLU);
            if ((c->flags & CELL_LAVA) && !(map_cell_safe(&doomo_map, FIX2INT(a->x) / CELLU,
                                                           FIX2INT(a->y) / CELLU)->flags & CELL_LAVA))
                return 0;
            if (fl < z - 64) return 0;
        }
    }
    /* other solid bodies */
    for (i = 0; i < ACTOR_MAX; i++) {
        const DoomoActor *o = &doomo_actors[i];
        fix16 ddx, ddy, rr;
        if (i == idx || (o->flags & (AF_USED | AF_SOLID)) != (AF_USED | AF_SOLID)) continue;
        ddx = o->x - nx; ddy = o->y - ny;
        rr = INT2FIX(o->radius + a->radius - 4);
        if (ddx > rr || ddx < -rr || ddy > rr || ddy < -rr) continue;
        if (FIX2INT(o->z) >= z + a->height || FIX2INT(o->z) + o->height <= z) continue;
        /* moving away from an overlap is fine */
        if (fx_dist(ddx, ddy) < rr && fx_dist(o->x - a->x, o->y - a->y) >= fx_dist(ddx, ddy))
            return 0;
    }
    /* the player */
    {
        fix16 ddx = doomo_player.x - nx, ddy = doomo_player.y - ny;
        fix16 rr = INT2FIX(PLAYER_RADIUS + a->radius - 2);
        int pz = FIX2INT(doomo_player.z);
        if (doomo_player.hp > 0 && ddx < rr && ddx > -rr && ddy < rr && ddy > -rr &&
            fx_dist(ddx, ddy) < rr && !(pz >= z + a->height || pz + DOOMO_PLAYER_HEIGHT <= z))
            return 0;
    }
    a->x = nx;
    a->y = ny;
    if (!flying) {
        if (fl > z) a->z = INT2FIX(fl);                  /* step up */
    }
    return 1;
}

static void demon_walk(int idx, int want_angle)
{
    DoomoActor *a = &doomo_actors[idx];
    int sp = demon_speed(a);
    fix16 step = INT2FIX(sp) / 8;
    static const int16_t tries[6] = { 0, 512, -512, 1024, -1024, 1536 };
    int k;
    if (ai_move[idx] > 0) {
        ai_move[idx]--;
        want_angle = ai_dir[idx];
    }
    for (k = 0; k < 6; k++) {
        int ang = (want_angle + (a->strafe > 0 ? tries[k] : -tries[k])) & DOOMO_ANG_MASK;
        fix16 dx = FIXMUL(fx_cos(ang), step), dy = FIXMUL(fx_sin(ang), step);
        if (demon_try_move(idx, dx, dy) || demon_try_move(idx, dx, 0) || demon_try_move(idx, 0, dy)) {
            if (k > 0) {
                ai_dir[idx] = (int16_t)ang;
                ai_move[idx] = (uint8_t)(8 + rng_range(&doomo_rng, 16));
            }
            a->angle = (int16_t)ang;
            return;
        }
    }
    /* stuck: pick a random direction for a while */
    ai_dir[idx] = (int16_t)rng_range(&doomo_rng, 4096);
    ai_move[idx] = 10;
}

static void demon_fire(int idx, int pk, int dmg, int speed, int spread)
{
    DoomoActor *a = &doomo_actors[idx];
    fix16 tx, ty;
    int tz, ang, z0 = FIX2INT(a->z) + a->height * 2 / 3;
    fix16 dist, slope;
    target_pos(a, &tx, &ty, &tz);
    ang = fx_atan2(ty - a->y, tx - a->x);
    ang += spread ? rng_between(&doomo_rng, -spread, spread) : 0;
    dist = fx_dist(tx - a->x, ty - a->y);
    if (dist < INT2FIX(8)) dist = INT2FIX(8);
    slope = FIXDIV(INT2FIX(tz - z0), dist);
    if (pk == PK_SLIME || pk == PK_GRENADE) {
        /* lob: aim the arc so it lands near the target */
        fix16 t = FIXDIV(dist, INT2FIX(speed));
        fix16 g = GRAVITY_FX * combat_proj_info[pk].gravity / 16;
        slope = FIXDIV(INT2FIX(tz - z0), dist) + FIXMUL(t, g) / (2 * speed);
    }
    if (combat_difficulty() >= DIFF_NIGHTMARE) speed = speed * 6 / 5;
    actor_projectile(pk, idx, a->x + FIXMUL(fx_cos(ang), INT2FIX(a->radius)),
                     a->y + FIXMUL(fx_sin(ang), INT2FIX(a->radius)), INT2FIX(z0), ang, slope,
                     speed, dmg, 0, pk == PK_SLIME ? 20 : 0,
                     pk == PK_ORB ? a->target : NO_TARGET);
}

static void demon_melee(int idx)
{
    DoomoActor *a = &doomo_actors[idx];
    const DemonInfo *di = DEMON(a->type);
    fix16 tx, ty;
    int tz;
    target_pos(a, &tx, &ty, &tz);
    if (fx_dist(tx - a->x, ty - a->y) > INT2FIX(di->melee_range + a->radius)) return;
    if (a->target == TARGET_PLAYER) {
        int ang = fx_atan2(doomo_player.y - a->y, doomo_player.x - a->x);
        combat_hurt_player(di->melee_dmg, (ang + 2048) & DOOMO_ANG_MASK, di->mass / 16 + 4, idx);
    } else if (a->target >= 0) {
        actor_damage(a->target, di->melee_dmg, idx, fx_atan2(ty - a->y, tx - a->x), 6, 0, 0);
    }
    audio_sfx(SFX_SPLAT);
}

/* ---- Lord Vapula ------------------------------------------------------------------ */
static void boss_think(int idx)
{
    DoomoActor *a = &doomo_actors[idx];
    int max = DEMON(a->type)->hp;
    int phase = a->hp > max * 2 / 3 ? 0 : a->hp > max / 3 ? 1 : 2;
    if (phase > a->owner) {
        int k;
        a->owner = (uint8_t)phase;
        a->state = AST_PHASE;
        a->timer = 0;
        audio_sfx(SFX_DEMON_ROAR);
        combat_shake(12);
        combat_toast(phase == 1 ? "Vapula: \"You dare?! Breathe deep!\"" :
                                  "Vapula: \"ENOUGH! THE NINTH CLOUD RISES!\"");
        /* the cloud coughs up supplies (a fair fight) */
        for (k = 0; k < 4; k++) {
            static const uint8_t drops[4] = { THING_AMMO_BULLETS, THING_AMMO_SHELLS, THING_CANDY, THING_HEALTH_BIG };
            int ang = k * 1024 + 512 + a->anim * 13;
            fix16 sx = a->x + FIXMUL(fx_cos(ang), INT2FIX(200));
            fix16 sy = a->y + FIXMUL(fx_sin(ang), INT2FIX(200));
            DoomoThing t;
            int p;
            memset(&t, 0, sizeof(t));
            t.x = (uint8_t)(FIX2INT(sx) / CELLU);
            t.y = (uint8_t)(FIX2INT(sy) / CELLU);
            t.type = drops[k];
            if (cw_solid(t.x, t.y) || (map_cell_safe(&doomo_map, t.x, t.y)->flags & CELL_LAVA)) continue;
            p = actor_spawn_thing(&t, 0);
            if (p >= 0) actor_fx(SPR_SPARKLE, doomo_actors[p].x, doomo_actors[p].y, doomo_actors[p].z + INT2FIX(12), 3);
        }
        /* summon helpers around the arena */
        for (k = 0; k < 2 + phase; k++) {
            int ang = k * 4096 / (2 + phase) + a->anim * 8;
            fix16 sx = a->x + FIXMUL(fx_cos(ang), INT2FIX(160));
            fix16 sy = a->y + FIXMUL(fx_sin(ang), INT2FIX(160));
            int type = phase == 1 ? THING_IMPLING : (k & 1 ? THING_PUFF_DEMON : THING_HELLHOUND_PUP);
            int fl, ce;
            if (cw_fits(sx, sy, 20, cw_floor(FIX2INT(sx) / CELLU, FIX2INT(sy) / CELLU), 56, 64, &fl, &ce))
                actor_spawn_demon(type, sx, sy, 1, 0, 0);
        }
    }
}

static void boss_attack(int idx)
{
    DoomoActor *a = &doomo_actors[idx];
    int k, phase = a->owner;
    switch ((a->anim / 40 + phase) % 3) {
    case 0:   /* fan of vape bolts */
        for (k = -2 - phase; k <= 2 + phase; k++) {
            fix16 tx, ty; int tz, ang;
            target_pos(a, &tx, &ty, &tz);
            ang = fx_atan2(ty - a->y, tx - a->x) + k * 110;
            actor_projectile(PK_VAPE_BOLT, idx, a->x, a->y, a->z + INT2FIX(a->height * 2 / 3), ang,
                             FIXDIV(INT2FIX(tz - FIX2INT(a->z) - a->height * 2 / 3),
                                    DOOMO_MAX(fx_dist(tx - a->x, ty - a->y), INT2FIX(16))),
                             15, 15, 0, 0, NO_TARGET);
        }
        break;
    case 1:   /* homing orbs */
        demon_fire(idx, PK_ORB, 22, 11, 200);
        if (phase >= 1) demon_fire(idx, PK_ORB, 22, 11, 200);
        break;
    default:  /* fireball rain (phase 2 also slime) */
        demon_fire(idx, PK_FIREBALL, 18, 17, 60);
        demon_fire(idx, PK_FIREBALL, 18, 17, 60);
        if (phase == 2) demon_fire(idx, PK_SLIME, 25, 14, 80);
        break;
    }
    audio_sfx(SFX_PLASMA);
}

/* ---- demon thinking -------------------------------------------------------------- */
static int can_see_target(int idx)
{
    DoomoActor *a = &doomo_actors[idx];
    fix16 tx, ty;
    int tz;
    target_pos(a, &tx, &ty, &tz);
    return cw_los(a->x, a->y, FIX2INT(a->z) + a->height * 3 / 4, tx, ty, tz);
}

static int looks_for_player(int idx)
{
    DoomoActor *a = &doomo_actors[idx];
    const DemonInfo *di = DEMON(a->type);
    fix16 px, py;
    int pz, ang, diff;
    fix16 d;
    if (doomo_player.hp <= 0) return 0;
    combat_player_eye(&px, &py, &pz);
    d = fx_dist(px - a->x, py - a->y);
    if (d > INT2FIX(di->sight * CELLU)) return 0;
    if (a->tflags & THF_AMBUSH) {
        ang = fx_atan2(py - a->y, px - a->x);
        diff = ((ang - a->angle + 2048) & DOOMO_ANG_MASK) - 2048;
        if ((diff < -1100 || diff > 1100) && d > INT2FIX(96)) return 0;
    }
    if (!cw_los(a->x, a->y, FIX2INT(a->z) + a->height * 3 / 4, px, py, pz)) return 0;
    return 1;
}

static void demon_wake(int idx)
{
    DoomoActor *a = &doomo_actors[idx];
    if (a->state != AST_IDLE) return;
    a->state = AST_CHASE;
    a->timer = 0;
    a->target = TARGET_PLAYER;
    if ((idx & 3) == 0 || (a->flags & AF_BOSS)) audio_sfx(SFX_DEMON_ROAR);
}

void actor_wake_all(int ncx, int ncy, int radius_cells)
{
    int i;
    for (i = 0; i < ACTOR_MAX; i++) {
        DoomoActor *a = &doomo_actors[i];
        int dx, dy;
        if (!(a->flags & AF_USED) || !IS_DEMON(a->type) || a->state != AST_IDLE) continue;
        if (a->tflags & THF_AMBUSH) continue;
        dx = FIX2INT(a->x) / CELLU - ncx;
        dy = FIX2INT(a->y) / CELLU - ncy;
        if (dx * dx + dy * dy <= radius_cells * radius_cells) demon_wake(i);
    }
}

static void demon_status_tick(int idx)
{
    DoomoActor *a = &doomo_actors[idx];
    if (a->status_fx && (a->anim & 7) == 0) {
        if (a->aux > 0) a->aux--;
        if (a->aux == 0) a->status_fx &= (uint8_t)~(ST_BURN | ST_FROZEN | ST_CHARMED | ST_DAZZLED);
    }
    if ((a->status_fx & ST_BURN) && (a->anim % 10) == 0) {
        actor_damage(idx, 3, -1, a->angle, 0, 0, 0);
        actor_fx(SPR_FIREBALL, a->x, a->y, a->z + INT2FIX(a->height / 2), 3);
    }
    if ((a->status_fx & ST_CHARMED) && (a->anim % 24) == 0)
        actor_fx(SPR_HEARTS, a->x, a->y, a->z + INT2FIX(a->height), 4);
    if ((a->status_fx & (ST_FROZEN | ST_DAZZLED)) && (a->anim % 20) == 0)
        actor_fx(SPR_SPARKLE, a->x, a->y, a->z + INT2FIX(a->height * 3 / 4), 3);
}

static void demon_think(int idx)
{
    DoomoActor *a = &doomo_actors[idx];
    const DemonInfo *di = DEMON(a->type);
    fix16 tx, ty, dist;
    int tz, ang, z = FIX2INT(a->z);
    int think = ((a->anim + idx) & 3) == 0;
    a->anim++;
    if (ai_flash[idx]) ai_flash[idx]--;
    a->timer++;
    /* knockback velocity */
    if (a->vx || a->vy) {
        if (!demon_try_move(idx, a->vx, 0)) a->vx = 0;
        if (!demon_try_move(idx, 0, a->vy)) a->vy = 0;
        a->vx = a->vx * 3 / 4;
        a->vy = a->vy * 3 / 4;
        if (a->vx > -FIX_HALF / 4 && a->vx < FIX_HALF / 4) a->vx = 0;
        if (a->vy > -FIX_HALF / 4 && a->vy < FIX_HALF / 4) a->vy = 0;
    }
    /* gravity for walkers */
    if (!(a->flags & AF_FLYING) || a->state >= AST_DYING) {
        int fl = cw_floor(FIX2INT(a->x) / CELLU, FIX2INT(a->y) / CELLU), ce;
        cw_fits(a->x, a->y, a->radius, 30000, 0, 30000, &fl, &ce);
        if (a->z > INT2FIX(fl)) {
            a->vz -= GRAVITY_FX;
            a->z += a->vz;
            if (a->z <= INT2FIX(fl)) { a->z = INT2FIX(fl); a->vz = 0; }
        } else {
            a->z = INT2FIX(fl);
            a->vz = 0;
        }
        z = FIX2INT(a->z);
        if (a->state < AST_DYING) {
            const DoomoCell *c = map_cell_safe(&doomo_map, FIX2INT(a->x) / CELLU, FIX2INT(a->y) / CELLU);
            if ((c->flags & CELL_LAVA) && (a->anim & 15) == 0 && a->type != THING_LORD_VAPULA)
                actor_damage(idx, 5, -1, 0, 0, 0, 0);
        }
    }
    switch (a->state) {
    case AST_DORMANT:
        return;
    case AST_APPEAR:
        if (a->timer >= 14) {
            a->state = AST_CHASE;
            a->timer = 0;
            a->flags |= AF_SOLID | AF_SHOOTABLE;
            a->target = TARGET_PLAYER;
        }
        return;
    case AST_DYING:
        if (a->timer >= 20) {
            a->state = AST_DEAD;
            a->flags |= AF_CORPSE;
            corpse_count++;
            if (corpse_count > 28) {
                int k;
                for (k = 0; k < ACTOR_MAX; k++)
                    if (k != idx && (doomo_actors[k].flags & AF_CORPSE)) {
                        doomo_actors[k].flags = 0;
                        corpse_count--;
                        break;
                    }
            }
        }
        return;
    case AST_DEAD:
        return;
    default:
        break;
    }
    demon_status_tick(idx);
    if (a->type == THING_LORD_VAPULA) boss_think(idx);
    if (a->cooldown) a->cooldown--;
    if (a->state == AST_IDLE) {
        if (think && looks_for_player(idx)) demon_wake(idx);
        return;
    }
    if (a->state == AST_PHASE) {
        a->z += (a->owner >= 1 && z < cw_floor(FIX2INT(a->x) / CELLU, FIX2INT(a->y) / CELLU) + 40) ? INT2FIX(2) : 0;
        if (a->timer >= 40) { a->state = AST_CHASE; a->timer = 0; }
        return;
    }
    if (a->state == AST_PAIN) {
        if (a->timer >= 8) { a->state = AST_CHASE; a->timer = 0; }
        return;
    }
    /* targets: charm makes it fight other demons, infighting sticks */
    if (a->status_fx & ST_CHARMED) {
        if (!target_alive(a) || a->target == TARGET_PLAYER) {
            int t = nearest_enemy_demon(idx);
            a->target = (int16_t)(t >= 0 ? t : NO_TARGET);
        }
        if (a->target == NO_TARGET) return;     /* nothing to fight: waits happily */
    } else if (!target_alive(a)) {
        a->target = TARGET_PLAYER;
        a->status_fx &= (uint8_t)~ST_ANGRY;
    }
    if (a->target == TARGET_PLAYER && doomo_player.hp <= 0) {
        demon_walk(idx, a->angle);
        return;
    }
    target_pos(a, &tx, &ty, &tz);
    dist = fx_dist(tx - a->x, ty - a->y);
    ang = fx_atan2(ty - a->y, tx - a->x);

    switch (a->state) {
    case AST_WINDUP:
        a->angle = (int16_t)ang;
        if (a->timer == 10 || (di->burst > 1 && a->timer > 10 && (a->timer - 10) % 5 == 0 &&
                               (a->timer - 10) / 5 < di->burst)) {
            if (a->type == THING_LORD_VAPULA) boss_attack(idx);
            else demon_fire(idx, di->pk, di->shot_dmg, di->shot_speed, di->burst > 1 ? 40 : 16);
        }
        if (a->timer >= 12 + (di->burst - 1) * 5) {
            a->state = AST_CHASE;
            a->timer = 0;
            a->cooldown = (uint8_t)(di->shot_cool + rng_range(&doomo_rng, di->shot_cool / 2 + 1));
            if (combat_difficulty() == DIFF_PLUSHIE) a->cooldown = (uint8_t)(a->cooldown * 3 / 2);
        }
        return;
    case AST_MELEE:
        a->angle = (int16_t)ang;
        if (a->timer == 8) demon_melee(idx);
        if (a->timer >= 16) { a->state = AST_CHASE; a->timer = 0; a->cooldown = 20; }
        return;
    case AST_CHARGE: {
        /* lunge (pups) or dive (gargoyles) */
        fix16 step = INT2FIX(demon_speed(a)) / 8 * 2;
        int hit = 0;
        if (a->flags & AF_FLYING) {
            int want = tz - a->height / 2;
            if (z > want) a->z -= INT2FIX(3); else if (z < want) a->z += INT2FIX(2);
        }
        if (!demon_try_move(idx, FIXMUL(fx_cos(a->angle), step), FIXMUL(fx_sin(a->angle), step)))
            hit = 1;
        if (dist < INT2FIX(di->melee_range / 2 + a->radius + 8)) {
            demon_melee(idx);
            hit = 1;
        }
        if (hit || a->timer > 24) {
            a->state = AST_CHASE;
            a->timer = 0;
            a->cooldown = (uint8_t)(40 + rng_range(&doomo_rng, 30));
        }
        return;
    }
    default:
        break;
    }

    /* ---- AST_CHASE ---- */
    if (a->flags & AF_FLYING) {
        /* hover around the target's eye height (divers stay high) */
        int fl, ce, want = tz - a->height / 2;
        if (di->ai == AI_DIVER) want = tz + 40;
        if (a->type == THING_LORD_VAPULA) want = cw_floor(FIX2INT(a->x) / CELLU, FIX2INT(a->y) / CELLU) +
                                                 (a->owner >= 1 ? 32 + ui_wave(a->anim, 90, 12) : 4);
        want += ui_wave(a->anim + idx * 7, 64, 6);
        cw_fits(a->x, a->y, a->radius, 30000, 0, 30000, &fl, &ce);
        if (want < fl) want = fl;
        if (ce < SKY_CEIL && want + a->height > ce) want = ce - a->height;
        if (z < want) a->z += INT2FIX(z < want - 4 ? 2 : 1);
        else if (z > want) a->z -= INT2FIX(z > want + 4 ? 2 : 1);
    }
    {
        int frozen = (a->status_fx & (ST_FROZEN | ST_DAZZLED)) != 0;
        int in_melee = dist < INT2FIX(di->melee_range + a->radius);
        int los;
        if (think) ai_los[idx] = (uint8_t)can_see_target(idx);
        los = ai_los[idx];
        if (!frozen && a->cooldown == 0) {
            if (in_melee && di->melee_dmg && (dist < INT2FIX(di->melee_range + a->radius - 4) || !di->pk)) {
                a->state = AST_MELEE;
                a->timer = 0;
                return;
            }
            if (di->ai == AI_CHARGER && los && dist < INT2FIX(5 * CELLU) && dist > INT2FIX(80)) {
                a->state = AST_CHARGE; a->timer = 0; a->angle = (int16_t)ang;
                audio_sfx(SFX_DEMON_ROAR);
                return;
            }
            if (di->ai == AI_DIVER && los && dist < INT2FIX(6 * CELLU) && rng_chance(&doomo_rng, 6)) {
                a->state = AST_CHARGE; a->timer = 0; a->angle = (int16_t)ang;
                return;
            }
            if (di->pk && los && think) {
                int chance = di->ai == AI_SKIRMISH ? 30 : di->ai == AI_SNIPER ? 40 : di->ai == AI_BOSS ? 60 : 22;
                if (dist > INT2FIX(di->sight * CELLU)) chance = 0;
                if (rng_chance(&doomo_rng, chance)) {
                    a->state = AST_WINDUP;
                    a->timer = 0;
                    return;
                }
            }
        }
        /* movement */
        {
            int want = ang;
            switch (di->ai) {
            case AI_SKIRMISH:
                if ((a->anim % 45) == 0 && rng_chance(&doomo_rng, 50)) a->strafe = (int8_t)-a->strafe;
                if (los && dist < INT2FIX(6 * CELLU)) want = ang + a->strafe * 900;
                else if (los) want = ang + a->strafe * 300;
                break;
            case AI_SNIPER:
                if (los && dist < INT2FIX(4 * CELLU)) want = ang + 2048 + a->strafe * 400;   /* back off */
                else if (los && dist < INT2FIX(8 * CELLU)) want = ang + a->strafe * 1024;    /* flank */
                if ((a->anim % 60) == 0) a->strafe = (int8_t)-a->strafe;
                break;
            case AI_FLYER:
            case AI_BOSS:
                if (los && dist < INT2FIX(4 * CELLU)) want = ang + a->strafe * 1100;
                if ((a->anim % 70) == 0) a->strafe = (int8_t)-a->strafe;
                break;
            case AI_DIVER:
                if (los && dist < INT2FIX(3 * CELLU)) want = ang + a->strafe * 1300;
                break;
            case AI_TANK:
            case AI_CHASER:
            case AI_CHARGER:
            default:
                if (los && (a->anim % 50) < 12) want = ang + a->strafe * 500;   /* sidestep */
                break;
            }
            if (in_melee && !(a->flags & AF_FLYING) && di->ai != AI_SNIPER) return;   /* hold the line */
            demon_walk(idx, want & DOOMO_ANG_MASK);
        }
    }
}

/* ---- projectiles ---------------------------------------------------------------------- */
int actor_projectile(int pk, int owner, fix16 x, fix16 y, fix16 z, int angle, fix16 slope,
                     int speed, int damage, int fx, int splash, int target)
{
    int i = actor_alloc(0);
    DoomoActor *a;
    const ProjInfo *pi;
    fix16 sp;
    if (i < 0) return -1;
    if (pk <= PK_NONE || pk >= PK_COUNT) pk = PK_CANDY;
    pi = &combat_proj_info[pk];
    a = &doomo_actors[i];
    a->type = AK_PROJECTILE;
    a->aux = (uint8_t)pk;
    a->spr = pi->spr;
    a->owner = (uint8_t)owner;
    a->x = x; a->y = y; a->z = z;
    sp = INT2FIX(speed);
    a->vx = FIXMUL(fx_cos(angle), sp);
    a->vy = FIXMUL(fx_sin(angle), sp);
    if (slope > INT2FIX(3)) slope = INT2FIX(3);
    if (slope < -INT2FIX(3)) slope = -INT2FIX(3);
    a->vz = FIXMUL(slope, sp);
    a->angle = (int16_t)angle;
    a->hp = (int16_t)damage;
    a->status_fx = (uint8_t)fx;
    a->arg = (uint8_t)DOOMO_CLAMP(splash, 0, 255);
    a->strafe = (int8_t)(pi->bounces + ((fx & WFX_BOUNCE) ? 2 : 0));
    a->radius = pi->radius;
    a->target = (int16_t)target;
    a->frame = 0;
    a->tflags = (uint8_t)(owner == PLAYER_OWNER ? combat_ws.gore : WSTYLE_BRUTAL);
    a->cooldown = (uint8_t)(owner == PLAYER_OWNER ? combat_ws.knockback : 6);
    a->anim = (uint16_t)rng_range(&doomo_rng, 8);
    return i;
}

static void projectile_explode(int idx, int direct)
{
    DoomoActor *a = &doomo_actors[idx];
    const ProjInfo *pi = &combat_proj_info[a->aux];
    int z = FIX2INT(a->z);
    int src = a->owner == PLAYER_OWNER ? PLAYER_OWNER : a->owner;
    int impact = pi->impact;
    if (a->tflags == WSTYLE_CUTE && a->owner == PLAYER_OWNER && impact == SPR_EXPLOSION &&
        combat_gore_style() == WSTYLE_CUTE)
        impact = SPR_CONFETTI;
    if (a->arg) {
        actor_splash(a->x, a->y, z, a->arg * 2, a->hp, src, a->status_fx);
        if (a->aux == PK_BFG) {
            /* BFG tracers: hit every demon in sight nearby */
            int k;
            for (k = 0; k < ACTOR_MAX; k++) {
                DoomoActor *o = &doomo_actors[k];
                if (!(o->flags & AF_SHOOTABLE) || !IS_DEMON(o->type) || o->hp <= 0) continue;
                if (fx_dist(o->x - a->x, o->y - a->y) > INT2FIX(6 * CELLU)) continue;
                if (!cw_los(a->x, a->y, z, o->x, o->y, FIX2INT(o->z) + o->height / 2)) continue;
                actor_damage(k, a->hp / 3, src, a->angle, 10, a->status_fx, 0);
                actor_fx(SPR_PLASMA, o->x, o->y, o->z + INT2FIX(o->height / 2), 3);
            }
        }
        combat_shake(a->aux == PK_BFG ? 14 : 6);
        audio_sfx(SFX_EXPLODE);
    } else if (!direct && a->aux == PK_STICKER) {
        actor_splash(a->x, a->y, z, 24, a->hp, src, a->status_fx);
    }
    if (impact) actor_fx(impact, a->x, a->y, a->z, a->arg ? 3 : 2);
    if (a->aux == PK_SLIME) actor_fx(ART_BASE + ART_SPLAT_GREEN, a->x, a->y, a->z, 3);
    a->flags = 0;
}

static int projectile_hits_actor(int idx, fix16 nx, fix16 ny, int nz)
{
    DoomoActor *p = &doomo_actors[idx];
    int k;
    for (k = 0; k < ACTOR_MAX; k++) {
        DoomoActor *o = &doomo_actors[k];
        fix16 rr, ddx, ddy;
        int oz;
        if (k == idx || !(o->flags & AF_SHOOTABLE) || o->hp <= 0) continue;
        if (p->owner != PLAYER_OWNER && (k == p->owner ||
            (p->owner < ACTOR_MAX && doomo_actors[p->owner].type == o->type))) continue;
        if (p->frame == k + 1) continue;                    /* just pierced it */
        rr = INT2FIX(o->radius + p->radius);
        ddx = o->x - nx; ddy = o->y - ny;
        if (ddx > rr || ddx < -rr || ddy > rr || ddy < -rr) continue;
        if (fx_dist(ddx, ddy) > rr) continue;
        oz = FIX2INT(o->z);
        if (nz < oz - p->radius || nz > oz + o->height + p->radius) continue;
        return k;
    }
    return -1;
}

static void projectile_tick(int idx)
{
    DoomoActor *a = &doomo_actors[idx];
    const ProjInfo *pi = &combat_proj_info[a->aux];
    fix16 nx, ny, nz;
    int cx, cy, fl, ce, hit;
    a->anim++;
    a->timer++;
    if (a->timer > 140 || (a->aux == PK_FLAME && a->timer > 14) ||
        (a->aux == PK_GRENADE && a->timer > 60) ||
        ((a->aux == PK_GLITTER || a->aux == PK_WATER) && a->owner == PLAYER_OWNER &&
         combat_ws.behaviour == WB_STREAM && a->timer > 16)) {
        if (a->aux == PK_BUBBLE || a->aux == PK_STICKER || a->arg) projectile_explode(idx, 0);
        else a->flags = 0;
        return;
    }
    /* homing (orbs, homing mod) */
    if (a->target >= 0 || (a->status_fx & WFX_HOMING)) {
        fix16 tx = 0, ty = 0;
        int tz = 0, have = 0;
        if (a->target == TARGET_PLAYER || (a->owner != PLAYER_OWNER && a->target == NO_TARGET)) {
            combat_player_eye(&tx, &ty, &tz); tz -= 12; have = 1;
        } else if (a->target >= 0 && a->target < ACTOR_MAX &&
                   (doomo_actors[a->target].flags & AF_SHOOTABLE)) {
            DoomoActor *t = &doomo_actors[a->target];
            tx = t->x; ty = t->y; tz = FIX2INT(t->z) + t->height / 2; have = 1;
        } else if (a->owner == PLAYER_OWNER && (a->anim & 7) == 0) {
            /* pick the nearest demon ahead */
            int k, best = -1;
            fix16 bd = INT2FIX(8 * CELLU);
            for (k = 0; k < ACTOR_MAX; k++) {
                DoomoActor *o = &doomo_actors[k];
                fix16 d;
                if (!(o->flags & AF_SHOOTABLE) || !IS_DEMON(o->type) || o->hp <= 0) continue;
                d = fx_dist(o->x - a->x, o->y - a->y);
                if (d < bd && (FIXMUL(o->x - a->x, a->vx) + FIXMUL(o->y - a->y, a->vy)) > 0) { bd = d; best = k; }
            }
            if (best >= 0) a->target = (int16_t)best;
        }
        if (have && a->timer > 4) {
            int cur = fx_atan2(a->vy, a->vx), want = fx_atan2(ty - a->y, tx - a->x);
            int diff = ((want - cur + 2048) & DOOMO_ANG_MASK) - 2048;
            fix16 sp = fx_dist(a->vx, a->vy);
            int turn = a->aux == PK_ORB ? 40 : 60;
            cur += DOOMO_CLAMP(diff, -turn, turn);
            a->vx = FIXMUL(fx_cos(cur), sp);
            a->vy = FIXMUL(fx_sin(cur), sp);
            a->angle = (int16_t)cur;
            if (FIX2INT(a->z) < tz) a->vz += INT2FIX(1) / 4; else a->vz -= INT2FIX(1) / 4;
            a->vz = DOOMO_CLAMP(a->vz, -INT2FIX(4), INT2FIX(4));
        }
    }
    /* floaty bubbles slow down and drift up */
    if (a->aux == PK_BUBBLE) {
        a->vx = a->vx * 15 / 16;
        a->vy = a->vy * 15 / 16;
        a->vz = INT2FIX(1) / 3 + (fx_sin(a->anim * 120) >> 2);
    }
    if (pi->gravity) a->vz -= GRAVITY_FX * pi->gravity / 16;
    nx = a->x + a->vx;
    ny = a->y + a->vy;
    nz = a->z + a->vz;
    cx = FIX2INT(nx) / CELLU;
    cy = FIX2INT(ny) / CELLU;
    /* actors */
    hit = projectile_hits_actor(idx, nx, ny, FIX2INT(nz));
    if (hit >= 0) {
        DoomoActor *o = &doomo_actors[hit];
        int src = a->owner == PLAYER_OWNER ? PLAYER_OWNER : a->owner;
        int head = a->owner == PLAYER_OWNER && FIX2INT(nz) > FIX2INT(o->z) + o->height * 3 / 4;
        a->x = nx; a->y = ny; a->z = nz;
        actor_damage(hit, a->hp, src, a->angle, a->cooldown, a->status_fx, head);
        if (a->status_fx & WFX_PIERCE && a->strafe-- > -2) {
            a->frame = (uint8_t)(hit + 1);
            actor_fx(pi->impact, nx, ny, nz, 2);
            return;
        }
        if (a->aux == PK_PLUSH && a->strafe > 0) {      /* plushies bounce off demons */
            a->strafe--;
            a->vx = -a->vx / 2; a->vy = -a->vy / 2; a->vz = INT2FIX(5);
            a->frame = (uint8_t)(hit + 1);
            audio_sfx(SFX_SQUEAK);
            return;
        }
        projectile_explode(idx, 1);
        return;
    }
    /* the player */
    if (a->owner != PLAYER_OWNER && doomo_player.hp > 0) {
        fix16 ddx = doomo_player.x - nx, ddy = doomo_player.y - ny;
        fix16 rr = INT2FIX(PLAYER_RADIUS + a->radius);
        int pz = FIX2INT(doomo_player.z), z = FIX2INT(nz);
        if (ddx < rr && ddx > -rr && ddy < rr && ddy > -rr && fx_dist(ddx, ddy) < rr &&
            z >= pz - a->radius && z <= pz + DOOMO_PLAYER_HEIGHT + a->radius) {
            int from = fx_atan2(-a->vy, -a->vx);
            combat_hurt_player(a->hp, from, a->aux == PK_SLIME ? 6 : 4, a->owner);
            a->x = nx; a->y = ny; a->z = nz;
            if (a->arg) { a->arg = (uint8_t)(a->arg / 2); a->hp = (int16_t)(a->hp / 3); }
            projectile_explode(idx, 1);
            return;
        }
    }
    /* world */
    fl = cw_floor(cx, cy);
    ce = cw_ceil(cx, cy);
    if (cw_solid(cx, cy) || FIX2INT(nz) < fl || FIX2INT(nz) > ce) {
        if (a->strafe > 0 && a->aux != PK_STICKER) {
            /* bounce: find which component crossed */
            int ocx = FIX2INT(a->x) / CELLU, ocy = FIX2INT(a->y) / CELLU;
            a->strafe--;
            if (FIX2INT(nz) < fl && !cw_solid(cx, cy) && ocx == cx && ocy == cy) {
                a->vz = -a->vz / 2;
                a->vx = a->vx * 3 / 4; a->vy = a->vy * 3 / 4;
                a->z = INT2FIX(fl + 1);
            } else {
                if (cx != ocx && (cw_solid(cx, ocy) || cw_floor(cx, ocy) > FIX2INT(nz))) a->vx = -a->vx;
                if (cy != ocy && (cw_solid(ocx, cy) || cw_floor(ocx, cy) > FIX2INT(nz))) a->vy = -a->vy;
                if (cx != ocx && cy != ocy && !cw_solid(cx, ocy) && !cw_solid(ocx, cy)) { a->vx = -a->vx; a->vy = -a->vy; }
                if (FIX2INT(nz) > ce) a->vz = -a->vz / 2;
            }
            a->frame = 0;
            if (a->aux == PK_PLUSH) audio_sfx(SFX_SQUEAK);
            return;
        }
        if (a->aux == PK_STICKER && a->state == 0) {
            /* stick to the wall, pop a moment later */
            a->state = 1;
            a->vx = a->vy = a->vz = 0;
            a->timer = 120;
            return;
        }
        if (a->aux == PK_GRENADE && FIX2INT(nz) < fl && !cw_solid(cx, cy)) {
            a->vz = 0; a->vx = 0; a->vy = 0; a->z = INT2FIX(fl);
            return;
        }
        projectile_explode(idx, 0);
        return;
    }
    a->x = nx; a->y = ny; a->z = nz;
    if (pi->trail && (a->anim & 3) == 0) actor_fx(pi->trail, a->x - a->vx, a->y - a->vy, a->z, 3);
}

/* ---- effects ----------------------------------------------------------------------- */
static void effect_tick(int idx)
{
    DoomoActor *a = &doomo_actors[idx];
    int frames;
    a->timer++;
    a->x += a->vx; a->y += a->vy; a->z += a->vz;
    if (a->spr >= ART_BASE) frames = actor_art_frames(a->spr - ART_BASE) * 4;
    else frames = sprite_frames(a->spr);
    if (frames <= 0) frames = 1;
    if (a->timer >= frames * a->cooldown + (a->hp > 0 ? a->hp : 0)) a->flags = 0;
}

static void gib_tick(int idx)
{
    DoomoActor *a = &doomo_actors[idx];
    int fl;
    a->timer++;
    if (--a->hp <= 0) { a->flags = 0; return; }
    if (a->vx || a->vy || a->vz || a->z > INT2FIX(cw_floor(FIX2INT(a->x) / CELLU, FIX2INT(a->y) / CELLU))) {
        fix16 nx = a->x + a->vx, ny = a->y + a->vy;
        if (cw_solid(FIX2INT(nx) / CELLU, FIX2INT(ny) / CELLU)) { a->vx = -a->vx / 2; a->vy = -a->vy / 2; }
        else { a->x = nx; a->y = ny; }
        a->vz -= GRAVITY_FX;
        a->z += a->vz;
        fl = cw_floor(FIX2INT(a->x) / CELLU, FIX2INT(a->y) / CELLU);
        if (a->z <= INT2FIX(fl)) {
            a->z = INT2FIX(fl);
            if (a->vz < -INT2FIX(3)) { a->vz = -a->vz / 3; a->vx /= 2; a->vy /= 2; }
            else { a->vz = 0; a->vx = 0; a->vy = 0; }
        }
    }
}

/* ---- damage, death, gore ----------------------------------------------------------- */
int combat_gore_style(void)
{
    int g = opt_get(OPT_GORE);
    if (opt_get(OPT_CHEAT_CONFETTI) || opt_get(OPT_CHEAT_CUTE_DEMONS)) return WSTYLE_CUTE;
    if (opt_get(OPT_BLOOD_COLOR) >= 2) return WSTYLE_CUTE;            /* rainbow / confetti */
    if (g == GORE_MIXED) return WSTYLE_BRUTAL;               /* + the cute fx, see combat_gore_mixed() */
    if (combat_feel() > 1) return WSTYLE_BRUTAL;            /* Brutal combat feel = brutal gore */
    if (g == GORE_CUTE) return WSTYLE_CUTE;
    if (g == GORE_BRUTAL) return WSTYLE_BRUTAL;
    return combat_ws.gore;
}

/* "Cute + Gore": every kill / hit throws the cute confetti AND the blood. */
int combat_gore_mixed(void)
{
    return opt_get(OPT_GORE) == GORE_MIXED && combat_gore_style() != WSTYLE_CUTE;
}

void actor_gore(int idx, int overkill)
{
    DoomoActor *a = &doomo_actors[idx];
    int style = combat_gore_style();
    int amount = 1 + opt_get(OPT_CONFETTI_AMOUNT);       /* 1..3 */
    fix16 cz = a->z + INT2FIX(a->height / 2);
    int k, n;
    int mixed = combat_gore_mixed();
    if (style == WSTYLE_CUTE || mixed) {
        actor_fx(SPR_CONFETTI, a->x, a->y, cz, 3);
        actor_fx(SPR_HEARTS, a->x, a->y, a->z + INT2FIX(a->height * 3 / 4), 4);
        n = (overkill ? 4 : 2) * amount;
        if (mixed) n = n / 2 + 1;                           /* share the effect pool with the gibs */
        for (k = 0; k < n; k++) {
            int ang = rng_range(&doomo_rng, 4096);
            int sp = 1 + rng_range(&doomo_rng, 3);
            actor_gib(k & 1 ? SPR_STUFFING : SPR_CONFETTI, a->x, a->y, cz,
                      FIXMUL(fx_cos(ang), INT2FIX(sp)), FIXMUL(fx_sin(ang), INT2FIX(sp)),
                      INT2FIX(4 + rng_range(&doomo_rng, 5)));
        }
        if (amount >= 3) actor_fx(SPR_SPARKLE, a->x, a->y, cz + INT2FIX(8), 3);
        audio_sfx(SFX_CONFETTI);
        if (overkill && !mixed) {
            a->spr = SPR_CORPSE_CUTE;
            audio_sfx(SFX_SQUEAK);
        }
    }
    if (style != WSTYLE_CUTE) {
        int green = opt_get(OPT_BLOOD_COLOR) == 1;
        actor_fx(green ? ART_BASE + ART_SPLAT_GREEN : SPR_BLOOD_SPRAY, a->x, a->y, cz, 3);
        n = overkill ? 7 : 3;
        if (combat_feel() > 1) { n = overkill ? 12 : 5; combat_shake(overkill ? 5 : 2); }
        for (k = 0; k < n; k++) {
            int ang = rng_range(&doomo_rng, 4096);
            int sp = 1 + rng_range(&doomo_rng, overkill ? 5 : 3);
            actor_gib(green ? ART_BASE + ART_SLIME : SPR_GIBS, a->x, a->y, cz,
                      FIXMUL(fx_cos(ang), INT2FIX(sp)), FIXMUL(fx_sin(ang), INT2FIX(sp)),
                      INT2FIX(4 + rng_range(&doomo_rng, 6)));
        }
        if (overkill) {
            actor_fx(SPR_GIBS, a->x, a->y, cz, 3);
            a->spr = SPR_CORPSE_BRUTAL;
            unlock_grant(562);
        }
        audio_sfx(SFX_SPLAT);
    }
}

static void barrel_explode(int idx)
{
    DoomoActor *a = &doomo_actors[idx];
    int z = FIX2INT(a->z) + 16;
    a->flags = 0;
    actor_fx(combat_gore_style() == WSTYLE_CUTE ? SPR_CONFETTI : SPR_EXPLOSION, a->x, a->y,
             INT2FIX(z), 3);
    actor_fx(ART_BASE + ART_SPLAT_GREEN, a->x, a->y, a->z, 4);
    actor_splash(a->x, a->y, z, 96, 60, -1, WFX_BURN);
    combat_shake(8);
    audio_sfx(SFX_EXPLODE);
}

int actor_damage(int idx, int dmg, int src, int angle, int knock, int fx, int headshot)
{
    DoomoActor *a;
    const DemonInfo *di;
    int overkill, mass;
    if (idx < 0 || idx >= ACTOR_MAX) return 0;
    a = &doomo_actors[idx];
    if (!(a->flags & AF_SHOOTABLE) || a->hp <= 0) return 0;
    if (a->type == THING_PROP_BARREL) {
        a->hp = (int16_t)(a->hp - dmg);
        if (a->hp <= 0) barrel_explode(idx);
        return a->hp <= 0;
    }
    if (!IS_DEMON(a->type)) return 0;
    di = DEMON(a->type);
    if (a->state == AST_PHASE || a->state == AST_APPEAR) return 0;    /* invulnerable moments */
    /* holy and charm effects */
    if (fx & WFX_HOLY) {
        int mul = src == PLAYER_OWNER ? combat_ws.holy : 24;
        dmg = dmg * DOOMO_MAX(mul, 24) / 16;
        if ((a->anim & 3) == 0) actor_fx(SPR_HOLY_SPLASH, a->x, a->y, a->z + INT2FIX(a->height / 2), 2);
    }
    if (headshot) dmg = dmg * (src == PLAYER_OWNER ? combat_ws.crit : 24) / 16;
    if ((fx & WFX_CHARM) && !(a->flags & AF_BOSS) && rng_chance(&doomo_rng, 35)) {
        a->status_fx |= ST_CHARMED;
        a->aux = 30;                                    /* ~240 ticks */
        a->target = NO_TARGET;
    }
    if (fx & WFX_FREEZE) { a->status_fx |= ST_FROZEN; if (a->aux < 12) a->aux = 12; }
    if (fx & WFX_BURN) { a->status_fx |= ST_BURN; if (a->aux < 10) a->aux = 10; }
    a->hp = (int16_t)(a->hp - dmg);
    ai_flash[idx] = 3;
    /* knockback */
    mass = di->mass ? di->mass : 100;
    if (knock && !(a->flags & AF_BOSS)) {
        fix16 k = INT2FIX(knock) * 8 / mass;
        if (k > INT2FIX(12)) k = INT2FIX(12);
        a->vx += FIXMUL(fx_cos(angle), k);
        a->vy += FIXMUL(fx_sin(angle), k);
    }
    if (a->state == AST_IDLE) demon_wake(idx);
    /* infighting: hurt by another demon -> go after it */
    if (src >= 0 && src < ACTOR_MAX && src != idx && IS_DEMON(doomo_actors[src].type) &&
        doomo_actors[src].type != a->type && !(a->status_fx & ST_CHARMED)) {
        a->target = (int16_t)src;
        a->status_fx |= ST_ANGRY;
    }
    if (a->hp <= 0) {
        overkill = -a->hp >= (combat_feel() > 1 ? di->hp / 3 : di->hp / 2 + 10) || dmg >= 120;
        a->state = AST_DYING;
        a->timer = 0;
        a->flags &= (uint8_t)~(AF_SOLID | AF_SHOOTABLE);
        a->vz = INT2FIX(3);
        actor_gore(idx, overkill);
        audio_sfx(SFX_DEMON_DIE);
        combat_on_kill(idx, src == PLAYER_OWNER, headshot);
        return 1;
    }
    if (rng_range(&doomo_rng, 256) < di->pain && a->state != AST_CHARGE && a->state != AST_WINDUP) {
        a->state = AST_PAIN;
        a->timer = 0;
        if ((a->anim & 1) == 0) audio_sfx(SFX_DEMON_PAIN);
    }
    return 0;
}

void actor_splash(fix16 x, fix16 y, int z, int radius, int dmg, int src, int fx)
{
    int k;
    if (radius <= 0) return;
    for (k = 0; k < ACTOR_MAX; k++) {
        DoomoActor *o = &doomo_actors[k];
        fix16 d;
        int dd;
        if (!(o->flags & AF_SHOOTABLE) || o->hp <= 0) continue;
        d = fx_dist(o->x - x, o->y - y);
        dd = FIX2INT(d) - o->radius;
        if (dd < 0) dd = 0;
        if (dd > radius) continue;
        if (z > FIX2INT(o->z) + o->height + radius || z < FIX2INT(o->z) - radius) continue;
        actor_damage(k, dmg * (radius - dd * 3 / 4) / radius, src, fx_atan2(o->y - y, o->x - x),
                     dmg / 4, fx, 0);
    }
    /* the player takes reduced splash from their own explosions */
    if (doomo_player.hp > 0) {
        fix16 d = fx_dist(doomo_player.x - x, doomo_player.y - y);
        int dd = FIX2INT(d) - PLAYER_RADIUS, pz = FIX2INT(doomo_player.z);
        if (dd < 0) dd = 0;
        if (dd <= radius && z <= pz + DOOMO_PLAYER_HEIGHT + radius && z >= pz - radius) {
            int pd = dmg * (radius - dd * 3 / 4) / radius;
            if (src == PLAYER_OWNER) pd = pd / 3;
            if (pd > 0) combat_hurt_player(pd, fx_atan2(y - doomo_player.y, x - doomo_player.x),
                                           dmg / 6 + 2, src == PLAYER_OWNER ? -1 : src);
        }
    }
}

void actor_event(int tag)
{
    int i;
    for (i = 0; i < ACTOR_MAX; i++) {
        DoomoActor *a = &doomo_actors[i];
        if (!(a->flags & AF_USED) || a->state != AST_DORMANT || a->arg != tag) continue;
        a->state = AST_APPEAR;
        a->timer = 0;
        a->target = TARGET_PLAYER;
        actor_fx(SPR_PUFF, a->x, a->y, a->z + INT2FIX(a->height / 3), 3);
        actor_fx(SPR_SPARKLE, a->x, a->y, a->z + INT2FIX(a->height), 3);
    }
}

int actor_demons_alive(void)
{
    int i, n = 0;
    for (i = 0; i < ACTOR_MAX; i++) {
        const DoomoActor *a = &doomo_actors[i];
        /* appearing and phase-changing demons count; dormant ones wait */
        if ((a->flags & AF_USED) && IS_DEMON(a->type) && a->hp > 0 && a->state != AST_DYING &&
            a->state != AST_DEAD && a->state != AST_DORMANT)
            n++;
    }
    return n;
}

int actor_kill_group_alive(int arg)
{
    int i, n = 0;
    for (i = 0; i < ACTOR_MAX; i++) {
        const DoomoActor *a = &doomo_actors[i];
        if ((a->flags & AF_USED) && IS_DEMON(a->type) && (a->tflags & THF_KILL_EVENT) &&
            a->arg == arg && a->hp > 0 && a->state != AST_DYING && a->state != AST_DEAD)
            n++;
    }
    return n;
}

/* ---- per-tick update ------------------------------------------------------------------- */
void actors_tick(void)
{
    int i;
    for (i = 0; i < ACTOR_MAX; i++) {
        DoomoActor *a = &doomo_actors[i];
        if (!(a->flags & AF_USED)) continue;
        if (IS_DEMON(a->type)) demon_think(i);
        else if (a->type == AK_PROJECTILE) projectile_tick(i);
        else if (a->type == AK_EFFECT) effect_tick(i);
        else if (a->type == AK_GIB) gib_tick(i);
        else a->anim++;
    }
}

/* ---- images for billboards ------------------------------------------------------------- */
int actor_demon_frame(const DoomoActor *a)
{
    switch (a->state) {
    case AST_WINDUP: case AST_MELEE: return DFRAME_ATTACK;
    case AST_PAIN: case AST_PHASE: return DFRAME_PAIN;
    case AST_CHARGE: return (a->anim / 3) & 1 ? DFRAME_ATTACK : DFRAME_WALK1;
    case AST_DYING: return DFRAME_DEATH0 + DOOMO_MIN(a->timer / 5, 3);
    case AST_DEAD: return DFRAME_DEATH3;
    case AST_IDLE: return (a->anim / 16) & 1 ? DFRAME_WALK0 : DFRAME_WALK1;
    default: return (a->anim / 7) & 1 ? DFRAME_WALK1 : DFRAME_WALK0;
    }
}

const DoomoImage *actor_image(const DoomoActor *a, int *flags, int *scale, int *zoff)
{
    const DoomoImage *img;
    int sflags = 0, frames, f;
    *flags = 0;
    *zoff = 0;
    *scale = 256;
    if (IS_DEMON(a->type)) {
        const DemonInfo *di = DEMON(a->type);
        int idx = (int)(a - doomo_actors);
        if (a->state == AST_DORMANT) return NULL;
        if (a->spr == SPR_CORPSE_CUTE || a->spr == SPR_CORPSE_BRUTAL) {
            if (a->state == AST_DYING && a->timer < 6) img = sprite_image(di->spr, DFRAME_DEATH0);
            else img = sprite_image(a->spr, 0);
            *scale = 300;
            *flags = BB_SHADOW;
            return img;
        }
        img = sprite_image(di->spr, actor_demon_frame(a));
        *scale = di->scale;
        if (opt_get(OPT_CHEAT_TINY_DEMONS)) *scale = *scale * 3 / 5;
        sflags = sprite_flags(di->spr);
        if (sflags & SPRF_TRANSLUCENT) *flags |= BB_TRANSLUCENT;
        if (a->state < AST_DYING) *flags |= BB_SHADOW;
        if (a->state == AST_APPEAR) *flags |= (a->timer & 2) ? BB_TRANSLUCENT : 0;
        if (idx >= 0 && idx < ACTOR_MAX && ai_flash[idx]) *flags |= BB_FULLBRIGHT;
        if (a->state == AST_WINDUP && a->timer >= 8) *flags |= BB_FULLBRIGHT;
        return img;
    }
    if (a->type == AK_PROJECTILE) {
        const ProjInfo *pi = &combat_proj_info[a->aux];
        if (a->spr >= ART_BASE) {
            img = actor_art_image(a->spr - ART_BASE, a->anim / 3);
        } else {
            frames = sprite_frames(a->spr);
            img = sprite_image(a->spr, frames > 0 ? (a->anim / 3) % frames : 0);
        }
        *scale = pi->scale;
        if (a->aux == PK_FLAME) *scale = pi->scale + a->timer * 30;
        if (a->aux == PK_GLITTER || a->aux == PK_WATER) *scale = pi->scale + a->timer * 14;
        if (pi->fullbright) *flags |= BB_FULLBRIGHT;
        if (a->aux == PK_BUBBLE) *flags |= BB_TRANSLUCENT;
        *zoff = -(img->h * *scale / 512);
        return img;
    }
    if (a->type == AK_EFFECT || a->type == AK_GIB) {
        if (a->spr >= ART_BASE) {
            img = actor_art_image(a->spr - ART_BASE, a->timer / 4);
            *scale = 300;
        } else {
            sflags = sprite_flags(a->spr);
            frames = sprite_frames(a->spr);
            if (a->type == AK_GIB) f = a->strafe % (frames > 0 ? frames : 1);
            else f = frames > 0 ? DOOMO_MIN(a->timer / (a->cooldown ? a->cooldown : 1), frames - 1) : 0;
            img = sprite_image(a->spr, f);
            *scale = a->type == AK_GIB ? 160 : 300;
            if (sflags & SPRF_FULLBRIGHT) *flags |= BB_FULLBRIGHT;
            if (a->spr == SPR_PUFF) *flags |= BB_TRANSLUCENT;
        }
        if (a->type == AK_EFFECT) *zoff = -(img->h * *scale / 512);
        return img;
    }
    if (!a->spr) return NULL;
    sflags = sprite_flags(a->spr);
    frames = sprite_frames(a->spr);
    f = (sflags & SPRF_LOOP) && frames > 1 ? (a->anim / 8) % frames : 0;
    img = sprite_image(a->spr, f);
    if (a->flags & AF_PICKUP) {
        *scale = 300;
        *zoff = 4 + ui_wave(a->anim, 48, 3);
        *flags = BB_SHADOW;
        if (sflags & SPRF_FULLBRIGHT) *flags |= BB_FULLBRIGHT;
        if (a->type == THING_SOUL_ORB || a->type == THING_KEY_RED || a->type == THING_KEY_BLUE)
            *flags |= BB_FULLBRIGHT;
        return img;
    }
    switch (a->type) {
    case THING_PROP_TREE: *scale = 640; break;
    case THING_PROP_LAMP: *scale = 520; break;
    case THING_PROP_FOUNTAIN: *scale = 420; break;
    case THING_PROP_BARREL: *scale = 380; break;
    case THING_PROP_TORCH: *scale = 380; break;
    case THING_VAPE_PORTAL: *scale = 430; break;
    case THING_PROP_SKULLS: *scale = 360; break;
    default: *scale = 380; break;
    }
    *flags = BB_SHADOW;
    if (sflags & SPRF_FULLBRIGHT) *flags |= BB_FULLBRIGHT;
    return img;
}

/* ---- names for galleries ------------------------------------------------------------------ */
const char *combat_demon_name(int thing_type)
{
    return IS_DEMON(thing_type) ? DEMON(thing_type)->name : "";
}

const char *combat_demon_blurb(int thing_type)
{
    return IS_DEMON(thing_type) ? DEMON(thing_type)->blurb : "";
}
