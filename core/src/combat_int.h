/* combat_int.h - internals shared by the shooter's files (owner: action):
 * actor.c (actors, demon AI, projectiles, effects, damage), combat.c
 * (session, player, weapons, camera, view), combat_hud.c, combat_bot.c
 * (debug autopilot), combat_scene.c and arena.c. Not part of the contract.
 */
#ifndef DOOMO_COMBAT_INT_H
#define DOOMO_COMBAT_INT_H

#include "doomo/doomo.h"
#include "doomo/plat_ext.h"

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
    int16_t fmove, fstrafe;  /* analog -256..256 (0 = use move / strafe) */
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
    int     fire_buf;        /* buffered trigger press (feel) */
    int     turn_vel;        /* smoothed turn rate (feel) */
    int     cam_x, cam_y, cam_z;   /* third-person camera (fix16) */
    int     cam_ok;
    int     cam_dist;        /* eased third-person camera distance (feel) */
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
int  combat_gore_mixed(void);                    /* 1 = cute fx AND gore together */
int  actor_demon_frame(const DoomoActor *a);
const DoomoImage *actor_image(const DoomoActor *a, int *flags, int *scale, int *zoff);

/* ---- player hooks (combat.c) ---- */
int  combat_feel(void);          /* 0 classic, 1 punchy, 2 brutal (0 off PS1) */
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
/* clarity C10: the HUD's icons with their names on one line (the control card's last line) */
void combat_hud_legend(int x, int y, int w, uint8_t col);

/* ---- the control card (combat_card.c, clarity C10) ----
 * ccard_lines() flows the current scheme's controls into at most `max` lines of `width` px
 * (returns the count); ccard_draw() is the card's panel: an optional goal line, an optional
 * rule line, the controls and the HUD legend; ccard_height() its height for the same text. */
#define CCARD_LINE_CAP 64
#define CCARD_MAX_LINES 4
enum { CCARD_RULE_NONE, CCARD_RULE_CUTE, CCARD_RULE_BRUTAL, CCARD_RULE_TIME };
int  ccard_lines(char lines[][CCARD_LINE_CAP], int max, int width);
const char *ccard_mode_rule(int rule);
const char *ccard_goal(int cgoal);                /* the card's goal line for a CGOAL_* */
int  ccard_height(int w, const char *goal, const char *rule);
void ccard_draw(int x, int y, int w, int style, const char *goal, const char *rule);
void ccard_draw_above(int cx, int bottom, int w, int style, const char *goal, const char *rule);   /* (S2-2) */

/* (S2-2) a menu window (x, y, w, h) held over the still combat view with S2-1's modal hold (ui.h ui_modal_hold):
 * mode 0 asks at the top of the draw (1: draw only the window, then ui_modal_hold_end(), and return), mode 1 marks
 * the end of a full frame. `up`: the menu is up and done popping; `sig` (< 128): what changes the picture around the
 * window (the camera, the HUD's weapon, which hint line the selected row shows): another value goes back to a full
 * frame (64..127: another card of the same scene). The world does not move under these menus; the HUD's own idle
 * motion (a blinking badge) stands still while
 * one is held (the owner's held-frame default). */
static inline int combat_modal_hold(int mode, int up, int x, int y, int w, int h, uint32_t sig)
{
#if defined(DOOMO_PLATFORM_PS1)
    (void)mode; (void)up; (void)x; (void)y; (void)w; (void)h; (void)sig;
    return 0;                                 /* (the PS1 holds 30 here; ui_modal_* is compiled out) */
#else
    static const char keys[128] = { 1 };
    const void *key = &keys[sig & 127];
    if (mode) {
        ui_modal_no_band();                   /* (the page gaps hold part of the held view: viewhold.c) */
        ui_modal_full(key, up);
        return 0;
    }
    return ui_modal_hold(key, up, x, y, w, h);
#endif
}

static inline int combat_pause_hold(int mode, int paused, int n, int t, uint32_t sig)
{
    int x, y, w, h;
    pause_menu_rect(n, &x, &y, &w, &h);
    return combat_modal_hold(mode, paused && t >= 12, x, y, w, h, sig);
}

/* (S2-2) the 3D view under a card. A hash of everything the picture plat_view3d draws for v depends on that can
 * change while a scene shows it standing still (the camera, the billboards, the texture clock; `salt` adds what v
 * does not show, e.g. the world's tick count for the doors). Static inline: the level select and the combat
 * scenes each get their own copy (no shared code goes resident on the PS1). */
static inline uint32_t combat_view_key(const DoomoView3D *v, uint32_t salt)
{
    uint32_t h = 2166136261u ^ salt;
    int i;
#define CVK_(x) (h = (h ^ (uint32_t)(x)) * 16777619u)
    CVK_((uintptr_t)v->map); CVK_(v->x); CVK_(v->y); CVK_(v->z);
    CVK_((uint16_t)v->angle | (uint32_t)(uint16_t)v->pitch << 16);
    CVK_((uint16_t)v->bob | (uint32_t)(uint16_t)v->vx << 16);
    CVK_((uint16_t)v->vy | (uint32_t)(uint16_t)v->vw << 16);
    CVK_((uint16_t)v->vh | (uint32_t)v->tint << 16 | (uint32_t)v->tint_level << 24);
    CVK_(v->time | (uint32_t)v->camera << 16 | (uint32_t)(uint8_t)v->player_sprite << 24);
    CVK_(v->sprite_count);
    for (i = 0; i < v->sprite_count; i++) {
        const DoomoBillboard *b = &v->sprites[i];
        CVK_(b->x); CVK_(b->y); CVK_(b->z);
        CVK_((uintptr_t)b->img); CVK_(b->img ? (uintptr_t)b->img->pix ^ b->img->uid : 0);
        CVK_(b->scale | (uint32_t)b->flags << 16 | (uint32_t)b->light << 24);
    }
#undef CVK_
    return h | 1u;
}

/* (S2-2) draw the view: when `still` (the world does not move this frame: a card, a pause menu, a results or
 * death screen) a picture the platform kept from an earlier frame with the same key is put back instead of
 * rendered again (plat_ext.h plat_view_hold); the 2D over it is the caller's, every frame as before. `shade` >= 0:
 * the caller's first 2D call is plat_shade_rect(0, 0, screen w, screen h, shade), which the platform may fold into
 * the put-back picture. Returns 2 when it did (the caller skips that shade), 1 when the view was put back plainly,
 * 0 when it was rendered. */
static inline int combat_view_draw(const DoomoView3D *v, int still, uint32_t salt, int shade)
{
    uint32_t key;
#if defined(DOOMO_PLATFORM_PS1)
    still = 0;                                /* (the weak default: no resident call for it on the PS1) */
#endif
    if (!still) { plat_view3d(v); return 0; }
    key = combat_view_key(v, salt);
    if (shade >= 0 && plat_view_hold(PLAT_VIEW_PUT_SHADED(shade), key)) return 2;
    if (plat_view_hold(PLAT_VIEW_PUT, key)) return 1;
    plat_view3d(v);
    plat_view_hold(PLAT_VIEW_KEEP, key);
    return 0;
}
/* tests: the next story fight (SCENE_COMBAT | COMBAT_ARG_CARD) shows its card again, as after power-on */
void combat_scene_card_reset(void);

/* (perf, X5) the billboard cull + nearest-first order of combat_build_view: the AF_USED actors in front of the eye
 * (ex, ey) looking along (fx, fy) and within 30 cells, nearest first exactly as an insertion sort by fx_dist()
 * (ties keep index order), with key[k] = (rx^2 + ry^2) >> BB_KEY_SHIFT. Returns the count. combat.c */
#define BB_KEY_SHIFT 22
int  combat_bb_sort(fix16 ex, fix16 ey, fix16 fx, fix16 fy, int16_t *order, uint32_t *key);

#endif
