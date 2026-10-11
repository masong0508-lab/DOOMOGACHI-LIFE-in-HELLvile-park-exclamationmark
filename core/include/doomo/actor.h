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

#endif
