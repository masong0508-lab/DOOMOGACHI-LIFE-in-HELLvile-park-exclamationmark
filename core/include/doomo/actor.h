/* actor.h - the shooter: player, demons, projectiles, pickups
 * (owner: action). Pure game logic on doomo_map; the platform only renders
 * what combat_build_view() hands it.
 *
 * It must feel like a genuinely good FIRST-PERSON *and* THIRD-PERSON
 * shooter (toggle with SELECT, both fully playable):
 *  - tight 30 Hz movement with acceleration/friction, strafing (L/R in
 *    first person or held-L strafe mode), sprint (hold B), jump, crouch,
 *    dodge-roll (double-tap direction / PS1 Square), step-up/drop ledges,
 *    lifts, doors, knockback
 *  - vertical aim assist + soft lock-on (R in third person snaps to the
 *    nearest visible demon, reticle shows lock), look up/down (pitch)
 *  - per-weapon recoil, view kick, muzzle flash light, screen shake,
 *    hit-stop on kills, damage numbers / hit markers, directional damage
 *    indicator, low-health heartbeat
 *  - third person: over-the-shoulder camera with smooth follow, wall
 *    collision (camera pulls in), shoulder swap, the player's own
 *    customised citizen visibly holding the welded weapon (fighter sprite)
 *  - demons with real AI: chase, strafe, flank, ranged + melee, infighting,
 *    flying (puffs/wraiths use z), pain states, gibs per gore setting
 *  - 2 control schemes per platform (classic / modern), invert-look option
 */
#ifndef DOOMO_ACTOR_H
#define DOOMO_ACTOR_H

#include "types.h"
#include "platform.h"

/* Map thing types (DoomoThing.type). */
enum {
    THING_NONE, THING_PLAYER_START, THING_NPC,      /* arg = roster id - see story */
    THING_IMPLING, THING_VAPE_WRAITH, THING_PUFF_DEMON, THING_HELLHOUND_PUP,
    THING_BONE_JESTER, THING_SLIME_BELCHER, THING_GARGOYLE, THING_CYBER_PIGGY,
    THING_BRUTE, THING_CLOUD_SPIDER, THING_LORD_VAPULA,
    THING_HEALTH_SMALL, THING_HEALTH_BIG, THING_ARMOR, THING_AMMO_BULLETS,
    THING_AMMO_SHELLS, THING_AMMO_CELLS, THING_AMMO_ROCKETS, THING_KEY_RED,
    THING_KEY_BLUE, THING_SCRAP, THING_SOUL_ORB, THING_CANDY,
    THING_PROP_TREE, THING_PROP_LAMP, THING_PROP_BENCH, THING_PROP_FOUNTAIN,
    THING_PROP_BARREL, THING_PROP_SKULLS, THING_PROP_TORCH, THING_PROP_SIGN,
    THING_PROP_FLOWERS, THING_PROP_MAILBOX, THING_VAPE_PORTAL, THING_SPAWNER,
    THING_COUNT
};

#define ACTOR_MAX 128

typedef struct DoomoActor {
    fix16    x, y, z;        /* z = feet height                    */
    fix16    vx, vy, vz;
    int16_t  angle;
    int16_t  hp;
    uint8_t  type;           /* THING_* */
    uint8_t  state;          /* AST_* */
    uint8_t  frame;
    uint8_t  flags;
    uint16_t timer;
    int16_t  target;         /* actor index or -1 */
    uint8_t  radius, height; /* world units */
    uint8_t  status_fx;      /* burning / frozen / charmed */
    uint8_t  owner;          /* who fired a projectile */
    /* ---- action additions ---- */
    uint8_t  arg;            /* DoomoThing.arg (kill group / spawn tag)   */
    uint8_t  tflags;         /* THF_* from the map                        */
    uint8_t  cooldown;       /* attack cooldown / effect speed            */
    int8_t   strafe;         /* AI strafe direction / bounces left        */
    uint16_t anim;           /* animation clock                           */
    uint8_t  spr;            /* sprite (SPR_* or ACTOR_ART_* + 128)       */
    uint8_t  aux;            /* per-kind scratch (projectile kind, phase) */
} DoomoActor;

/* Combat session (one Hell Breach level or arena run). */
typedef struct DoomoPlayerState {
    fix16   x, y, z, vz;
    int16_t angle, pitch;
    int16_t hp, armor;
    uint8_t weapon_slot;     /* index into doomo_town.weapons */
    uint8_t on_ground, crouch;
    uint16_t refire, hurt_flash, pickup_flash;
    uint8_t keys;
    uint8_t camera;          /* CAM_* */
    uint8_t shoulder;        /* 0 right, 1 left (third person) */
    int16_t lock_target;     /* actor index or -1 */
    uint8_t roll_timer, sprint;
    int16_t recoil;          /* current view kick (pitch units) */
    int16_t resident_id;     /* whose body/look the player uses */
} DoomoPlayerState;

extern DoomoActor       doomo_actors[ACTOR_MAX];
extern DoomoPlayerState doomo_player;

void combat_start(int map_id);          /* load map, spawn things     */
void combat_tick(void);                 /* uses input.h state         */
int  combat_finished(void);             /* 0 playing, 1 exit, -1 dead */
/* Fill a DoomoView3D (and its billboard list) for the current frame. */
void combat_build_view(DoomoView3D *v);
/* HUD + weapon overlay, drawn with plat_* 2D calls after plat_view3d. */
void combat_draw_hud(void);

/* ======================================================================
 * action additions (owner: action; additive)
 * ==================================================================== */

/* What finishes a combat session (combat_finished() == 1). */
enum { CGOAL_EXIT,        /* reach a CELL_EXIT cell (Hell Breach levels)  */
       CGOAL_KILL_ALL,    /* defeat every demon (invasion encounters)     */
       CGOAL_ENDLESS };   /* never (the arena decides itself)             */
/* combat_start() picks CGOAL_EXIT for dungeons, KILL_ALL for overworld
 * maps, ENDLESS for arenas. */
void combat_start_ex(int map_id, int goal);
/* Start at a given cell / facing (0..4095) instead of the map's start. */
void combat_start_at(int map_id, int goal, int cx, int cy, int angle);
/* Spawn a thing at a cell centre (story encounters, arena waves). With
 * appear = 1 demons pop out of a vape puff. Returns actor index or -1. */
int  combat_spawn(int thing_type, int cx, int cy, int appear);
int  combat_demons_alive(void);
/* Fire a map event (remote doors, dormant demons). */
void combat_event(int tag);
/* The session result after combat_finished() != 0, also kept after the
 * scene closes: 1 exit/cleared, -1 died, -2 quit. */
int  combat_last_result(void);

typedef struct DoomoCombatStats {
    uint16_t kills, kills_total;
    uint16_t secrets, secrets_total;
    uint16_t items, items_total;
    uint16_t shots, hits, headshots;
    uint16_t damage_taken;
    uint32_t ticks;
    uint32_t score;
    uint16_t best_combo;
    uint16_t money;          /* H$ earned this session */
} DoomoCombatStats;
const DoomoCombatStats *combat_stats(void);

/* Weapons: the armory slot in use (doomo_town.weapons) and switching. */
const DoomoWeapon *combat_weapon(void);
void combat_select_weapon(int slot);
void combat_next_weapon(int dir);
/* Make sure the armory has something to shoot with (a cute pistol, fists,
 * a bubble wand). Returns the number of weapons added. */
int  combat_give_starter_weapons(void);
/* Default ammo pools for a fresh story / arena run. */
void combat_refill_ammo(int percent);
int  combat_ammo(int ammo_type);
int  combat_ammo_max(int ammo_type);
void combat_toggle_camera(void);

/* Debug autopilot (tests, attract mode): plays the level by itself -
 * paths to keys and the exit with a BFS over the grid (step-up, lifts,
 * doors), aims at and shoots visible demons. */
void combat_autopilot(int on);
int  combat_autopilot_on(void);

/* Demon catalogue (names / gallery). */
const char *combat_demon_name(int thing_type);
/* One-line description of a demon for galleries. */
const char *combat_demon_blurb(int thing_type);

#endif
