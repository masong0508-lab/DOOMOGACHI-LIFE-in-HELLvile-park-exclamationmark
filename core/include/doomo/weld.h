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
} DoomoWeaponStats;

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
