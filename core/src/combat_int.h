/* combat_int.h - internals shared by the shooter's files (owner: action):
 * actor.c (actors, demon AI, projectiles, effects, damage), combat.c
 * (session, player, weapons, camera, view), combat_hud.c, combat_bot.c
 * (debug autopilot), combat_scene.c and arena.c. Not part of the contract.
 */
#ifndef DOOMO_COMBAT_INT_H
#define DOOMO_COMBAT_INT_H

#include "doomo/doomo.h"

#define CELLU        DOOMO_CELL_UNITS
#define PLAYER_OWNER 255          /* DoomoActor.owner of the player's shots  */
#define NO_TARGET    (-1)
#define TARGET_PLAYER (-2)        /* DoomoActor.target: the player           */
#define SKY_CEIL     30000        /* "ceiling" of open-sky cells             */
#define PLAYER_RADIUS 14

/* DoomoActor.flags */
enum { AF_USED = 1, AF_SOLID = 2, AF_SHOOTABLE = 4, AF_FLYING = 8,
       AF_COUNTKILL = 16, AF_CORPSE = 32, AF_BOSS = 64, AF_PICKUP = 128 };

/* Internal actor kinds (DoomoActor.type beyond THING_COUNT). */
enum { AK_PROJECTILE = 0x60, AK_EFFECT, AK_GIB };

/* Demon AI states (DoomoActor.state). */
enum { AST_IDLE, AST_CHASE, AST_WINDUP, AST_MELEE, AST_CHARGE, AST_PAIN,
       AST_DYING, AST_DEAD, AST_DORMANT, AST_APPEAR, AST_PHASE };

/* DoomoActor.status_fx bits */
enum { ST_BURN = 1, ST_FROZEN = 2, ST_CHARMED = 4, ST_DAZZLED = 8,
       ST_ANGRY = 16, ST_HOLY = 32 };

/* Custom little sprites (actor_art.c), used via DoomoActor.spr >= ART_BASE */
#define ART_BASE 128
enum { ART_DART, ART_PLUSH, ART_YARN, ART_SAW, ART_SLIME, ART_SPLAT_GREEN,
       ART_WATER, ART_ICE, ART_NAIL, ART_STICKER, ART_SHELL_CASING, ART_COUNT };
const DoomoImage *actor_art_image(int art, int frame);
int  actor_art_frames(int art);

/* AI personalities */
enum { AI_CHASER, AI_SKIRMISH, AI_FLYER, AI_CHARGER, AI_DIVER, AI_SNIPER,
       AI_TANK, AI_BOSS };

typedef struct DemonInfo {
    const char *name;
    const char *blurb;
    uint8_t  spr;
    uint16_t hp;
    uint8_t  radius, height;
    uint16_t scale;          /* billboard scale 8.8 */
    uint8_t  speed;          /* world units / tick x 8 */
    uint8_t  ai;             /* AI_* */
    uint8_t  pain;           /* pain chance 0..255 */
    uint8_t  melee_dmg, melee_range;
    uint8_t  pk;             /* ranged projectile PK_* (0 none) */
    uint8_t  shot_dmg, shot_speed, shot_cool, burst;
    uint8_t  mass;           /* knockback resistance */
    uint8_t  score;
    uint8_t  sight;          /* sight range in cells */
} DemonInfo;
extern const DemonInfo combat_demon_info[THING_LORD_VAPULA - THING_IMPLING + 1];
#define IS_DEMON(t) ((t) >= THING_IMPLING && (t) <= THING_LORD_VAPULA)
#define DEMON(t) (&combat_demon_info[(t) - THING_IMPLING])

/* Per-projectile-kind look and physics (actor.c). */
typedef struct ProjInfo {
    uint8_t spr;             /* SPR_* or ART_BASE + ART_* */
    uint16_t scale;
    uint8_t gravity;         /* x16 of normal gravity */
    uint8_t radius;
    uint8_t bounces;
    uint8_t fullbright;
    uint8_t impact;          /* SPR_* impact effect */
    uint8_t trail;           /* SPR_* trail puff (0 none) */
} ProjInfo;
extern const ProjInfo combat_proj_info[PK_COUNT];

/* Controls for one tick (from the pad or the autopilot). */
typedef struct CombatCtl {
    int8_t  move, strafe;    /* -1..1 */
    int16_t turn;            /* angle units this tick (signed) */
    int16_t look;            /* pitch units this tick (signed) */
    uint8_t fire, fire_pressed, fire_released;
    uint8_t jump, sprint, roll, crouch_toggle, lock, lock_pressed;
    uint8_t next_weapon, camera, pause;
    int8_t  roll_dir;        /* -1 left, 1 right, 2 back, 0 forward/auto */
} CombatCtl;

/* Extended player state (the public DoomoPlayerState plus internals). */
typedef struct PlayerX {
    fix16   vx, vy;          /* velocity, world units / tick */
    fix16   view_h;          /* smoothed eye height above feet */
    int     bob_phase, bob_amp;
    int     sway_x, sway_y;  /* weapon sway (pixels x16) */
    int     kick;            /* weapon kick-back (pixels x16) */
    int     raise;           /* weapon lower/raise 0..256 (256 = down) */
    int     switch_to;       /* pending weapon slot or -1 */
    int     reload_t;        /* ticks left in reload */
    int     charge_t, spin;  /* charge ticks / minigun spin 0..256 */
    int     burst_left, burst_t;
    int     shots_fired;     /* for underbarrel procs */
    uint16_t mag[DOOMO_MAX_WEAPONS];
    int     flash;           /* muzzle flash ticks */
    int     shake;           /* screen shake strength */
    int     hitstop;
    int     hit_marker, hit_kill, hit_head;
    int     dmg_dir, dmg_dir_t;  /* direction of the last hit (angle), timer */
    int     roll_dx, roll_dy;
    int     roll_cool;
    int     lift_wait;
    int     lava_t;
    int     dead_t;
    int     tap_dir, tap_t;  /* double-tap detection */
    int     cam_x, cam_y, cam_z;   /* third-person camera (fix16) */
    int     cam_ok;
    int     lock_ui;         /* lock-on reticle animation */
    int     zoom_t;          /* ADS zoom 0..256 */
    int     ads;             /* aiming down sights */
    int     face_expr, face_t;
    int     combo, combo_t;
    int     msg_t;
    const char *msg;
    int     heartbeat;
    int     invuln;          /* i-frames */
    int     anim;            /* third-person run cycle */
    int     land_t;          /* landing squash */
    int     last_cx, last_cy;
    int     stand_still;
    int     secrets_seen;
    uint8_t cute_only;
    uint8_t lift_used;
} PlayerX;

typedef struct DmgNum {
    fix16 x, y, z;
    int16_t value;
    uint8_t t, crit;
} DmgNum;
#define DMGNUM_MAX 8

typedef struct CombatSession {
    int   map_id, goal, result, active;
    int   finished;          /* 0 playing, 1 done, -1 dead */
    int   boss;              /* actor index of the boss (-1) */
    int   boss_max;
    int   exit_t;            /* level-complete delay */
    int   hell;              /* 0..256 cute -> hell HUD blend */
    uint8_t fired[32];       /* event tags fired (bitset) */
    uint8_t secret_cells[64];/* secret cells visited (bitset by index/64) */
} CombatSession;

extern CombatSession combat_s;
extern PlayerX       combat_px;
extern CombatCtl     combat_ctl;
extern DoomoCombatStats combat_st;
extern DmgNum        combat_dmgnums[DMGNUM_MAX];
extern DoomoWeaponStats combat_ws;   /* stats of the equipped weapon */

/* ---- world queries (actor.c) ---- */
int  cw_floor(int cx, int cy);                   /* world units */
int  cw_ceil(int cx, int cy);                    /* SKY_CEIL for sky */
int  cw_solid(int cx, int cy);
/* Is a body of radius r / height h with feet at z allowed at (x, y)?
 * step = how high it may step up. Returns the floor / ceiling it sees. */
int  cw_fits(fix16 x, fix16 y, int r, int z, int h, int step, int *fl, int *ce);
/* Line of sight between two points (world fix16, z world units). */
int  cw_los(fix16 x0, fix16 y0, int z0, fix16 x1, fix16 y1, int z1);
/* Trace a ray against the world only; returns the distance (fix16) and
 * the hit point. */
fix16 cw_trace(fix16 x0, fix16 y0, fix16 z0, fix16 dx, fix16 dy, fix16 slope,
               fix16 maxd, fix16 *hx, fix16 *hy, fix16 *hz);

/* ---- actors (actor.c) ---- */
void actors_clear(void);
int  actor_alloc(int effect);                    /* -1 if full */
int  actor_spawn_thing(const DoomoThing *t, int appear);
int  actor_spawn_demon(int type, fix16 x, fix16 y, int appear, int tflags, int arg);
void actors_tick(void);
int  actor_fx(int spr, fix16 x, fix16 y, fix16 z, int speed_ticks);
int  actor_gib(int spr, fix16 x, fix16 y, fix16 z, int vx, int vy, int vz);
int  actor_projectile(int pk, int owner, fix16 x, fix16 y, fix16 z, int angle,
                      fix16 slope, int speed, int damage, int fx, int splash, int target);
/* Damage an actor; src = actor index, PLAYER_OWNER or -1. Returns 1 if it died. */
int  actor_damage(int idx, int dmg, int src, int angle, int knock, int fx, int headshot);
void actor_splash(fix16 x, fix16 y, int z, int radius, int dmg, int src, int fx);
void actor_wake_all(int noise_cx, int noise_cy, int radius_cells);
void actor_event(int tag);                       /* dormant spawns */
int  actor_demons_alive(void);
int  actor_kill_group_alive(int arg);
void actor_gore(int idx, int overkill);
int  combat_gore_style(void);                    /* WSTYLE_* for the current kill */
int  actor_demon_frame(const DoomoActor *a);
const DoomoImage *actor_image(const DoomoActor *a, int *flags, int *scale, int *zoff);

/* ---- player hooks (combat.c) ---- */
void combat_hurt_player(int dmg, int src_angle, int knock, int src);
void combat_player_eye(fix16 *x, fix16 *y, int *z);
int  combat_player_z(void);
void combat_toast(const char *msg);
void combat_add_dmgnum(fix16 x, fix16 y, fix16 z, int value, int crit);
void combat_on_kill(int idx, int by_player, int headshot);
int  combat_difficulty(void);
void combat_shake(int amount);

int  combat_set_result_quit(void);               /* result -2 unless already set */

/* ---- autopilot (combat_bot.c) ---- */
extern void (*combat_bot_fn)(CombatCtl *ctl);

/* ---- hud (combat_hud.c) ---- */
void combat_hud_project(fix16 x, fix16 y, fix16 z, int *sx, int *sy, int *visible);
void combat_hud_reset(void);
int  combat_hud_style(void);                    /* 0 cute .. 256 hell */
void combat_hud_set_view(const DoomoView3D *v);

#endif
