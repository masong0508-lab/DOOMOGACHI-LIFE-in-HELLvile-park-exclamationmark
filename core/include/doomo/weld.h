/* weld.h - weapon welding (owner: action). Rivet's workshop lets you weld
 * a weapon from a frame + barrel + ammo core + mod + sight + stock +
 * magazine + underbarrel + charm, painted in two colours, in CUTE or
 * BRUTAL style. A welding minigame sets quality 0..100, which scales the
 * final stats. Millions of combinations, each with its own generated name
 * and first-person / third-person sprite.
 *
 * The arsenal covers all kinds of real-world style guns - pistols,
 * revolvers, SMGs, assault rifles, battle rifles, snipers, LMGs, miniguns,
 * every kind of shotgun, launchers, flamethrowers, railguns, a BFG-class
 * cannon, melee - AND dedicated cutesy weapons (bubble wand, plush
 * launcher, confetti cannon, lollipop hammer, heart bow, squeaky mallet,
 * glitter sprayer, water pistol, sticker gun, yarn crossbow). Any frame can
 * be welded CUTE (toy-like, pastel, stickers, squeaks, confetti hits) or
 * BRUTAL (gunmetal, spikes, meat, full Doom gore).
 */
#ifndef DOOMO_WELD_H
#define DOOMO_WELD_H

#include "types.h"
#include "town.h"

enum {
    /* sidearms */
    WFRAME_PISTOL, WFRAME_REVOLVER, WFRAME_DUAL_PISTOLS, WFRAME_MACHINE_PISTOL,
    /* automatics */
    WFRAME_SMG, WFRAME_ASSAULT_RIFLE, WFRAME_BATTLE_RIFLE, WFRAME_BULLPUP,
    WFRAME_LMG, WFRAME_MINIGUN,
    /* precision */
    WFRAME_DMR, WFRAME_SNIPER, WFRAME_RAILGUN, WFRAME_CROSSBOW, WFRAME_BOW,
    /* shotguns */
    WFRAME_PUMP_SHOTGUN, WFRAME_SUPER_SHOTGUN, WFRAME_AUTO_SHOTGUN,
    WFRAME_LEVER_SHOTGUN,
    /* heavy / energy */
    WFRAME_GRENADE_LAUNCHER, WFRAME_ROCKET_LAUNCHER, WFRAME_FLAMETHROWER,
    WFRAME_PLASMA_RIFLE, WFRAME_NAILGUN, WFRAME_BFG,
    /* melee */
    WFRAME_CHAINSAW, WFRAME_KATANA, WFRAME_SLEDGEHAMMER, WFRAME_FISTS,
    /* dedicated cutesy weapons */
    WFRAME_BUBBLE_WAND, WFRAME_PLUSH_LAUNCHER, WFRAME_CONFETTI_CANNON,
    WFRAME_LOLLIPOP_HAMMER, WFRAME_HEART_BOW, WFRAME_SQUEAKY_MALLET,
    WFRAME_GLITTER_SPRAYER, WFRAME_WATER_PISTOL, WFRAME_STICKER_GUN,
    WFRAME_YARN_CROSSBOW,
    WFRAME_COUNT
};
enum { WBARREL_STANDARD, WBARREL_SHORT, WBARREL_LONG, WBARREL_HEAVY,
       WBARREL_DOUBLE, WBARREL_TRIPLE, WBARREL_SPIRAL, WBARREL_WIDE,
       WBARREL_SAWED, WBARREL_SUPPRESSED, WBARREL_HEART, WBARREL_STAR,
       WBARREL_COUNT };
enum { WCORE_BULLET, WCORE_AP_BULLET, WCORE_EXPLOSIVE_ROUND, WCORE_SHELL,
       WCORE_SLUG, WCORE_FLECHETTE, WCORE_PLASMA, WCORE_ROCKET,
       WCORE_GRENADE, WCORE_NAIL, WCORE_RAIL_SLUG, WCORE_FUEL,
       WCORE_BUBBLE, WCORE_GLITTER, WCORE_HOLY_WATER, WCORE_HELLFIRE,
       WCORE_ICE, WCORE_LOVE, WCORE_SAWBLADE, WCORE_SLIME, WCORE_CANDY,
       WCORE_PLUSH, WCORE_COUNT };
enum { WMOD_NONE, WMOD_EXT_MAG, WMOD_SPIKES, WMOD_BAYONET, WMOD_STICKERS,
       WMOD_COOLER, WMOD_OVERCLOCK, WMOD_RICOCHET, WMOD_HOMING, WMOD_BURST,
       WMOD_FULL_AUTO, WMOD_CHARGE_SHOT, WMOD_LIFESTEAL, WMOD_SQUEAKER,
       WMOD_COUNT };
enum { WSIGHT_IRON, WSIGHT_RED_DOT, WSIGHT_HOLO, WSIGHT_SCOPE_4X,
       WSIGHT_SCOPE_8X, WSIGHT_THERMAL, WSIGHT_HEART_DOT, WSIGHT_LASER,
       WSIGHT_COUNT };
enum { WSTOCK_NONE, WSTOCK_FIXED, WSTOCK_FOLDING, WSTOCK_HEAVY,
       WSTOCK_SKELETON, WSTOCK_PLUSH, WSTOCK_COUNT };
enum { WMAG_STANDARD, WMAG_EXTENDED, WMAG_DRUM, WMAG_QUICK, WMAG_BELT,
       WMAG_CANDY_TUBE, WMAG_COUNT };
enum { WUNDER_NONE, WUNDER_GRIP, WUNDER_GRENADE, WUNDER_SHOTGUN,
       WUNDER_FLASHLIGHT, WUNDER_CHAINSAW, WUNDER_BUBBLE, WUNDER_COUNT };
enum { WCHARM_NONE, WCHARM_TEDDY, WCHARM_SKULL, WCHARM_DICE, WCHARM_HEART,
       WCHARM_EYEBALL, WCHARM_BELL, WCHARM_ROSARY, WCHARM_COUNT };
enum { WSTYLE_CUTE, WSTYLE_BRUTAL };
enum { AMMO_BULLETS, AMMO_SHELLS, AMMO_CELLS, AMMO_ROCKETS, AMMO_FUEL,
       AMMO_NAILS, AMMO_CANDY,   /* cute weapons run on candy */
       AMMO_COUNT };

/* Special effect bits on computed stats. */
enum { WFX_HOLY = 1,      /* bonus vs demons, can de-possess with LOVE   */
       WFX_BURN = 2, WFX_FREEZE = 4, WFX_CHARM = 8, WFX_PIERCE = 16,
       WFX_SPLASH = 32, WFX_HOMING = 64, WFX_BOUNCE = 128 };

typedef struct DoomoWeaponStats {
    int16_t damage;       /* per pellet */
    uint8_t pellets;      /* 1..12 */
    uint8_t spread;       /* angle units */
    uint8_t refire;       /* ticks between shots */
    uint8_t ammo_type;    /* AMMO_* or 0xFF = none (melee) */
    uint8_t ammo_use;
    uint8_t projectile;   /* 0 = hitscan, else SPR_* projectile sprite */
    int16_t speed;        /* projectile speed (world units/tick) */
    uint8_t range;        /* cells */
    uint8_t mag_size;     /* rounds before reload (0 = no reload) */
    uint8_t reload;       /* reload ticks */
    uint8_t recoil;       /* view kick */
    uint8_t zoom;         /* 0 none, else ADS zoom level (sights) */
    uint8_t auto_fire;    /* 1 = hold to fire */
    uint8_t knockback;
    uint8_t fx;           /* WFX_* */
    uint8_t sfx;          /* SFX_* */
    /* ---- action additions ---- */
    uint8_t behaviour;    /* WB_*: how the shot travels                  */
    uint8_t pkind;        /* PK_*: projectile look + physics (0 hitscan)  */
    uint8_t wclass;       /* WCLASS_*                                     */
    uint8_t splash;       /* splash radius in world units / 2 (0 none)    */
    uint8_t spin_up;      /* ticks to reach the full fire rate (minigun)  */
    uint8_t charge;       /* hold-to-charge ticks for a full shot (0 none)*/
    uint8_t burst;        /* shots per trigger pull (1 normal, 3 burst)   */
    uint8_t crit;         /* headshot multiplier x16 (16 = none)          */
    uint8_t weight;       /* 0..100 handling: slows walking & aiming      */
    uint8_t under_every;  /* underbarrel proc: every N shots (0 never)    */
    uint8_t lifesteal;    /* % of damage dealt returned as health         */
    uint8_t quiet;        /* 1 = suppressed: does not wake sleeping demons*/
    uint8_t gore;         /* WSTYLE_* used by OPT_GORE "per weapon"       */
    uint8_t holy;         /* x16 multiplier vs demons (16 = normal)       */
} DoomoWeaponStats;

/* How a shot travels (DoomoWeaponStats.behaviour). */
enum { WB_HITSCAN, WB_PROJECTILE, WB_LOB, WB_FLOAT, WB_STREAM, WB_RAIL,
       WB_MELEE, WB_COUNT };
/* Projectile kinds (DoomoWeaponStats.pkind, also used by demons). */
enum { PK_NONE, PK_FIREBALL, PK_VAPE_BOLT, PK_ORB, PK_SLIME, PK_PLASMA,
       PK_ROCKET, PK_GRENADE, PK_NAIL, PK_ARROW, PK_BUBBLE, PK_PLUSH,
       PK_HEART, PK_GLITTER, PK_WATER, PK_STICKER, PK_YARN, PK_SAW,
       PK_FLAME, PK_BFG, PK_CANDY, PK_HOLY, PK_CONFETTI, PK_ICE, PK_COUNT };
/* Weapon classes (DoomoWeaponStats.wclass). */
enum { WCLASS_SIDEARM, WCLASS_AUTO, WCLASS_PRECISION, WCLASS_SHOTGUN,
       WCLASS_HEAVY, WCLASS_MELEE, WCLASS_CUTE, WCLASS_COUNT };
extern const char *const doomo_wclass_names[WCLASS_COUNT];
extern const char *const doomo_ammo_names[AMMO_COUNT];
extern const char *const doomo_wstyle_names[2];

/* Part lists for menus: count / name of option `index` in a CLIST_W* list
 * (unlocks.h), "" if out of range. */
int  weld_part_count(int clist);
const char *weld_part_name(int clist, int index);
/* Read / write one part of a weapon by CLIST_W* list id. */
int  weld_part_get(const DoomoWeapon *w, int clist);
void weld_part_set(DoomoWeapon *w, int clist, int value);
/* A sensible weapon of a frame in a style (matching parts, paints). */
void weld_default(DoomoWeapon *w, int frame, int style);
/* Weapon level 0..7 from its kill xp. */
int  weld_level(const DoomoWeapon *w);

/* 0..100 ratings for the workshop stat bars. */
enum { WRATE_DAMAGE, WRATE_RATE, WRATE_RANGE, WRATE_ACCURACY, WRATE_AMMO,
       WRATE_RELOAD, WRATE_CONTROL, WRATE_MOBILITY, WRATE_COUNT };
extern const char *const doomo_wrate_names[WRATE_COUNT];
void weld_ratings(const DoomoWeaponStats *s, uint8_t out[WRATE_COUNT]);
/* Damage per second-ish figure (for comparisons / tests). */
int  weld_dps(const DoomoWeaponStats *s);

/* First-person sprite geometry: size of weld_view_image() frames and the
 * muzzle / grip points inside them (pixels). */
#define WELD_VIEW_W 96
#define WELD_VIEW_H 64
#define WELD_ICON_W 24
#define WELD_ICON_H 16
void weld_view_points(const DoomoWeapon *w, int *muzzle_x, int *muzzle_y,
                      int *grip_x, int *grip_y);
/* Drop cached sprites (after editing a weapon in place). */
void weld_gfx_invalidate(void);

extern const char *const doomo_wframe_names[WFRAME_COUNT];
extern const char *const doomo_wbarrel_names[WBARREL_COUNT];
extern const char *const doomo_wcore_names[WCORE_COUNT];
extern const char *const doomo_wmod_names[WMOD_COUNT];
extern const char *const doomo_wsight_names[WSIGHT_COUNT];
extern const char *const doomo_wstock_names[WSTOCK_COUNT];
extern const char *const doomo_wmag_names[WMAG_COUNT];
extern const char *const doomo_wunder_names[WUNDER_COUNT];
extern const char *const doomo_wcharm_names[WCHARM_COUNT];

void weld_stats(const DoomoWeapon *w, DoomoWeaponStats *out);
/* "Glittering Plush Repeater +3" / "Hellfire Meat Shredder Mk.II" */
void weld_name(const DoomoWeapon *w, char *buf, int cap);
/* Materials consumed to weld this combination (items.h ids/counts). */
int  weld_cost(const DoomoWeapon *w, uint8_t *items, uint8_t *counts, int max);
/* First-person view sprite for the weapon (frame 0 idle, 1 fire, 2 fire2,
 * 3 reload). Built from all parts + paint + style. */
const DoomoImage *weld_view_image(const DoomoWeapon *w, int frame);
/* Small icon for menus (24x16). */
const DoomoImage *weld_icon_image(const DoomoWeapon *w);

#endif
