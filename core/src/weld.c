/* weld.c - weapon welding: stats, names, material costs (owner: action).
 *
 * A welded weapon is frame + barrel + core + mod + sight + stock + mag +
 * underbarrel + charm, two paints, a CUTE / BRUTAL style and a quality
 * 0..100 from the welding minigame. weld_stats() turns any combination
 * into DoomoWeaponStats:
 *
 *   1. the frame gives the archetype (class, base damage, pellets, spread,
 *      fire rate, magazine, behaviour: hitscan / projectile / lob / float /
 *      stream / rail / melee, ...)
 *   2. the ammo core decides WHAT comes out (it can turn a pistol into a
 *      plasma pistol, a minigun into a bubble hose, a sniper into a rail)
 *   3. barrel, mod, sight, stock, mag, underbarrel, charm, style, quality
 *      and the weapon's level apply multipliers (x16 fixed point)
 *   4. everything is clamped into sane ranges (no zero refire, no zero
 *      damage, no division by zero anywhere downstream)
 *
 * Integer-only and deterministic; no tables in RAM.
 */
#include "doomo/weld.h"
#include "doomo/items.h"
#include "doomo/audio.h"
#include "doomo/sprites.h"
#include "doomo/palette.h"
#include "doomo/unlocks.h"
#include "doomo/rng.h"
#include "doomo/fixmath.h"

/* ---- part names ---------------------------------------------------------- */
const char *const doomo_wframe_names[WFRAME_COUNT] = {
    "Pistol", "Revolver", "Dual Pistols", "Machine Pistol", "SMG",
    "Assault Rifle", "Battle Rifle", "Bullpup", "LMG", "Minigun", "DMR",
    "Sniper Rifle", "Railgun", "Crossbow", "Bow", "Pump Shotgun",
    "Super Shotgun", "Auto Shotgun", "Lever Shotgun", "Grenade Launcher",
    "Rocket Launcher", "Flamethrower", "Plasma Rifle", "Nailgun", "BFG",
    "Chainsaw", "Katana", "Sledgehammer", "Fists", "Bubble Wand",
    "Plush Launcher", "Confetti Cannon", "Lollipop Hammer", "Heart Bow",
    "Squeaky Mallet", "Glitter Sprayer", "Water Pistol", "Sticker Gun",
    "Yarn Crossbow"
};
const char *const doomo_wbarrel_names[WBARREL_COUNT] = {
    "Standard", "Short", "Long", "Heavy", "Double", "Triple", "Spiral",
    "Wide", "Sawed-off", "Suppressed", "Heart", "Star"
};
const char *const doomo_wcore_names[WCORE_COUNT] = {
    "Bullet", "AP Bullet", "Explosive Round", "Shell", "Slug", "Flechette",
    "Plasma", "Rocket", "Grenade", "Nail", "Rail Slug", "Fuel", "Bubble",
    "Glitter", "Holy Water", "Hellfire", "Ice", "Love", "Sawblade", "Slime",
    "Candy", "Plush"
};
const char *const doomo_wmod_names[WMOD_COUNT] = {
    "None", "Ext. Mag", "Spikes", "Bayonet", "Stickers", "Cooler",
    "Overclock", "Ricochet", "Homing", "Burst", "Full Auto", "Charge Shot",
    "Lifesteal", "Squeaker"
};
const char *const doomo_wsight_names[WSIGHT_COUNT] = {
    "Iron Sights", "Red Dot", "Holo", "4x Scope", "8x Scope", "Thermal",
    "Heart Dot", "Laser"
};
const char *const doomo_wstock_names[WSTOCK_COUNT] = {
    "None", "Fixed", "Folding", "Heavy", "Skeleton", "Plush"
};
const char *const doomo_wmag_names[WMAG_COUNT] = {
    "Standard", "Extended", "Drum", "Quick", "Belt", "Candy Tube"
};
const char *const doomo_wunder_names[WUNDER_COUNT] = {
    "None", "Grip", "Grenade", "Shotgun", "Flashlight", "Chainsaw", "Bubble"
};
const char *const doomo_wcharm_names[WCHARM_COUNT] = {
    "None", "Teddy", "Skull", "Dice", "Heart", "Eyeball", "Bell", "Rosary"
};
const char *const doomo_wclass_names[WCLASS_COUNT] = {
    "Sidearm", "Automatic", "Precision", "Shotgun", "Heavy", "Melee", "Cute"
};
const char *const doomo_ammo_names[AMMO_COUNT] = {
    "Bullets", "Shells", "Cells", "Rockets", "Fuel", "Nails", "Candy"
};
const char *const doomo_wstyle_names[2] = { "Cute", "Brutal" };
const char *const doomo_wrate_names[WRATE_COUNT] = {
    "Damage", "Fire rate", "Range", "Accuracy", "Ammo", "Reload", "Control",
    "Mobility"
};

/* ---- frame archetypes ------------------------------------------------------ */
typedef struct FrameBase {
    uint8_t wclass, beh, pk;
    uint8_t dmg, pellets, spread, refire;
    uint8_t ammo, use, speed, range, mag, reload, recoil, zoom, autof, knock;
    uint8_t fx, sfx, splash, spin, charge, weight, crit;
    uint8_t core;              /* the natural ammo core */
} FrameBase;

#define NOAMMO 0xFF
#define C_SID WCLASS_SIDEARM
#define C_AUT WCLASS_AUTO
#define C_PRE WCLASS_PRECISION
#define C_SHO WCLASS_SHOTGUN
#define C_HEA WCLASS_HEAVY
#define C_MEL WCLASS_MELEE
#define C_CUT WCLASS_CUTE

static const FrameBase frames[WFRAME_COUNT] = {
/*                 cls   beh            pk          dmg pel spr ref ammo          use spd rng mag rel rec zm au kn fx                     sfx           spl spn chg wgt crit core */
/* PISTOL      */ {C_SID, WB_HITSCAN,   PK_NONE,     14, 1,  6,  9, AMMO_BULLETS, 1,  0, 40, 12, 30,  6, 0, 0, 3, 0,                    SFX_PISTOL,    0, 0, 0, 10, 32, WCORE_BULLET},
/* REVOLVER    */ {C_SID, WB_HITSCAN,   PK_NONE,     34, 1,  3, 18, AMMO_BULLETS, 1,  0, 48,  6, 45, 18, 0, 0, 8, 0,                    SFX_PISTOL,    0, 0, 0, 18, 40, WCORE_BULLET},
/* DUAL        */ {C_SID, WB_HITSCAN,   PK_NONE,     12, 1,  9,  5, AMMO_BULLETS, 1,  0, 36, 24, 45,  5, 0, 0, 3, 0,                    SFX_PISTOL,    0, 0, 0, 20, 28, WCORE_BULLET},
/* MACHINE_P   */ {C_SID, WB_HITSCAN,   PK_NONE,      9, 1, 12,  3, AMMO_BULLETS, 1,  0, 32, 20, 30,  4, 0, 1, 2, 0,                    SFX_CHAINGUN,  0, 0, 0, 14, 24, WCORE_BULLET},
/* SMG         */ {C_AUT, WB_HITSCAN,   PK_NONE,     10, 1,  9,  3, AMMO_BULLETS, 1,  0, 36, 30, 36,  3, 0, 1, 2, 0,                    SFX_CHAINGUN,  0, 0, 0, 25, 24, WCORE_BULLET},
/* AR          */ {C_AUT, WB_HITSCAN,   PK_NONE,     15, 1,  6,  4, AMMO_BULLETS, 1,  0, 56, 30, 42,  5, 0, 1, 3, 0,                    SFX_CHAINGUN,  0, 0, 0, 40, 32, WCORE_BULLET},
/* BATTLE      */ {C_AUT, WB_HITSCAN,   PK_NONE,     27, 1,  4,  8, AMMO_BULLETS, 1,  0, 60, 20, 48, 10, 0, 0, 5, 0,                    SFX_PISTOL,    0, 0, 0, 50, 32, WCORE_BULLET},
/* BULLPUP     */ {C_AUT, WB_HITSCAN,   PK_NONE,     14, 1,  5,  4, AMMO_BULLETS, 1,  0, 52, 32, 38,  4, 0, 1, 3, 0,                    SFX_CHAINGUN,  0, 0, 0, 32, 32, WCORE_BULLET},
/* LMG         */ {C_HEA, WB_HITSCAN,   PK_NONE,     16, 1, 11,  3, AMMO_BULLETS, 1,  0, 52,100, 90,  6, 0, 1, 4, 0,                    SFX_CHAINGUN,  0, 0, 0, 75, 24, WCORE_BULLET},
/* MINIGUN     */ {C_HEA, WB_HITSCAN,   PK_NONE,     12, 1, 13,  2, AMMO_BULLETS, 1,  0, 48,200,120,  3, 0, 1, 4, 0,                    SFX_CHAINGUN,  0,20, 0, 95, 24, WCORE_BULLET},
/* DMR         */ {C_PRE, WB_HITSCAN,   PK_NONE,     42, 1,  2, 14, AMMO_BULLETS, 1,  0, 64, 10, 50, 14, 1, 0, 6, 0,                    SFX_PISTOL,    0, 0, 0, 45, 48, WCORE_BULLET},
/* SNIPER      */ {C_PRE, WB_HITSCAN,   PK_NONE,     95, 1,  0, 36, AMMO_BULLETS, 1,  0, 64,  5, 70, 30, 2, 0,10, WFX_PIERCE,           SFX_SHOTGUN,   0, 0, 0, 60, 64, WCORE_BULLET},
/* RAILGUN     */ {C_PRE, WB_RAIL,      PK_NONE,    120, 1,  0, 45, AMMO_CELLS,   5,  0, 64,  4, 75, 26, 1, 0,12, WFX_PIERCE,           SFX_PLASMA,    0, 0, 0, 70, 48, WCORE_RAIL_SLUG},
/* CROSSBOW    */ {C_PRE, WB_PROJECTILE,PK_ARROW,    60, 1,  1, 26, AMMO_NAILS,   1, 40, 64,  0,  0, 10, 0, 0, 8, WFX_PIERCE,           SFX_BLIP,      0, 0, 0, 35, 48, WCORE_NAIL},
/* BOW         */ {C_PRE, WB_PROJECTILE,PK_ARROW,    40, 1,  1, 18, AMMO_NAILS,   1, 34, 56,  0,  0,  6, 0, 0, 6, 0,                    SFX_BLIP,      0, 0,24, 20, 48, WCORE_NAIL},
/* PUMP        */ {C_SHO, WB_HITSCAN,   PK_NONE,      9, 8, 48, 24, AMMO_SHELLS,  1,  0, 24,  8, 60, 22, 0, 0,12, 0,                    SFX_SHOTGUN,   0, 0, 0, 40, 24, WCORE_SHELL},
/* SUPER       */ {C_SHO, WB_HITSCAN,   PK_NONE,      9,12, 80, 36, AMMO_SHELLS,  2,  0, 20,  2, 50, 34, 0, 0,20, 0,                    SFX_SHOTGUN,   0, 0, 0, 45, 24, WCORE_SHELL},
/* AUTO_SG     */ {C_SHO, WB_HITSCAN,   PK_NONE,      7, 7, 56,  9, AMMO_SHELLS,  1,  0, 22, 12, 70, 16, 0, 1,10, 0,                    SFX_SHOTGUN,   0, 0, 0, 55, 20, WCORE_SHELL},
/* LEVER       */ {C_SHO, WB_HITSCAN,   PK_NONE,     10, 9, 40, 18, AMMO_SHELLS,  1,  0, 26,  7, 60, 20, 0, 0,12, 0,                    SFX_SHOTGUN,   0, 0, 0, 38, 24, WCORE_SHELL},
/* GRENADE_L   */ {C_HEA, WB_LOB,       PK_GRENADE,  70, 1,  4, 22, AMMO_ROCKETS, 1, 20, 40,  6, 60, 18, 0, 0,14, WFX_SPLASH|WFX_BOUNCE, SFX_ROCKET,   48, 0, 0, 55, 16, WCORE_GRENADE},
/* ROCKET_L    */ {C_HEA, WB_PROJECTILE,PK_ROCKET,   95, 1,  2, 26, AMMO_ROCKETS, 1, 26, 64,  4, 60, 24, 0, 0,20, WFX_SPLASH,           SFX_ROCKET,   56, 0, 0, 65, 16, WCORE_ROCKET},
/* FLAME       */ {C_HEA, WB_STREAM,    PK_FLAME,     5, 1, 10,  1, AMMO_FUEL,    1, 14,  6,100, 70,  1, 0, 1, 1, WFX_BURN,             SFX_ROCKET,    0, 0, 0, 60, 16, WCORE_FUEL},
/* PLASMA      */ {C_AUT, WB_PROJECTILE,PK_PLASMA,   22, 1,  4,  4, AMMO_CELLS,   1, 30, 56, 40, 50,  5, 0, 1, 3, 0,                    SFX_PLASMA,    0, 0, 0, 45, 24, WCORE_PLASMA},
/* NAILGUN     */ {C_AUT, WB_PROJECTILE,PK_NAIL,     12, 1,  6,  3, AMMO_NAILS,   1, 36, 48, 50, 50,  4, 0, 1, 2, 0,                    SFX_CHAINGUN,  0, 0, 0, 35, 24, WCORE_NAIL},
/* BFG         */ {C_HEA, WB_PROJECTILE,PK_BFG,     250, 1,  0, 60, AMMO_CELLS,  40, 16, 64,  0,  0, 40, 0, 0,30, WFX_SPLASH,           SFX_PLASMA,   80, 0,20, 90, 16, WCORE_PLASMA},
/* CHAINSAW    */ {C_MEL, WB_MELEE,     PK_NONE,      8, 1,  0,  2, NOAMMO,       0,  0,  1,  0,  0,  1, 0, 1, 2, 0,                    SFX_CHAINSAW,  0, 0, 0, 40, 16, WCORE_SAWBLADE},
/* KATANA      */ {C_MEL, WB_MELEE,     PK_NONE,     70, 1, 30, 14, NOAMMO,       0,  0,  1,  0,  0,  4, 0, 0, 6, 0,                    SFX_SPLAT,     0, 0, 0, 15, 32, WCORE_BULLET},
/* SLEDGE      */ {C_MEL, WB_MELEE,     PK_NONE,    140, 1, 20, 30, NOAMMO,       0,  0,  1,  0,  0, 12, 0, 0,30, WFX_SPLASH,           SFX_EXPLODE,  12, 0, 0, 60, 16, WCORE_BULLET},
/* FISTS       */ {C_MEL, WB_MELEE,     PK_NONE,     22, 1,  0, 10, NOAMMO,       0,  0,  1,  0,  0,  3, 0, 0,10, 0,                    SFX_SPLAT,     0, 0, 0,  0, 24, WCORE_BULLET},
/* BUBBLE_WAND */ {C_CUT, WB_FLOAT,     PK_BUBBLE,   22, 1,  6,  8, AMMO_CANDY,   1,  7, 30,  0,  0,  1, 0, 1, 2, WFX_FREEZE|WFX_SPLASH, SFX_BUBBLE,   12, 0, 0, 10, 16, WCORE_BUBBLE},
/* PLUSH_L     */ {C_CUT, WB_LOB,       PK_PLUSH,    45, 1,  3, 16, AMMO_CANDY,   2, 20, 40,  6, 40, 10, 0, 0,14, WFX_BOUNCE,           SFX_SQUEAK,    0, 0, 0, 30, 24, WCORE_PLUSH},
/* CONFETTI    */ {C_CUT, WB_HITSCAN,   PK_NONE,      7,12, 64, 20, AMMO_CANDY,   2,  0, 18,  8, 50, 16, 0, 0,16, WFX_CHARM,            SFX_CONFETTI,  0, 0, 0, 35, 16, WCORE_CANDY},
/* LOLLIPOP    */ {C_CUT, WB_MELEE,     PK_NONE,     90, 1, 20, 20, NOAMMO,       0,  0,  1,  0,  0,  8, 0, 0,26, WFX_CHARM,            SFX_SQUEAK,    0, 0, 0, 35, 24, WCORE_CANDY},
/* HEART_BOW   */ {C_CUT, WB_PROJECTILE,PK_HEART,    30, 1,  1, 16, AMMO_CANDY,   1, 30, 56,  0,  0,  4, 0, 0, 4, WFX_CHARM,            SFX_LOVE,      0, 0,20, 15, 40, WCORE_LOVE},
/* SQUEAKY     */ {C_CUT, WB_MELEE,     PK_NONE,     45, 1, 20, 10, NOAMMO,       0,  0,  1,  0,  0,  4, 0, 0,34, 0,                    SFX_SQUEAK,    0, 0, 0, 20, 24, WCORE_CANDY},
/* GLITTER     */ {C_CUT, WB_STREAM,    PK_GLITTER,   4, 1, 12,  1, AMMO_CANDY,   1, 16,  7,120, 60,  1, 0, 1, 1, WFX_FREEZE,           SFX_SPARKLE,   0, 0, 0, 25, 16, WCORE_GLITTER},
/* WATER       */ {C_CUT, WB_PROJECTILE,PK_WATER,     8, 1,  4,  3, AMMO_CANDY,   1, 26, 30, 40, 40,  2, 0, 1, 2, 0,                    SFX_SPLASH,    0, 0, 0, 12, 24, WCORE_HOLY_WATER},
/* STICKER     */ {C_CUT, WB_PROJECTILE,PK_STICKER,  30, 1,  4, 10, AMMO_CANDY,   1, 26, 40, 12, 40,  5, 0, 0, 4, WFX_SPLASH,           SFX_SQUEAK,   12, 0, 0, 18, 24, WCORE_CANDY},
/* YARN        */ {C_CUT, WB_PROJECTILE,PK_YARN,     40, 1,  2, 22, AMMO_CANDY,   1, 30, 56,  0,  0,  8, 0, 0, 8, WFX_FREEZE,           SFX_BUBBLE,    0, 0, 0, 28, 32, WCORE_PLUSH},
};

/* ---- small fixed-point helpers -------------------------------------------- */
static int mul16(int v, int m) { return (v * m + 8) >> 4; }   /* v * m / 16 */
static int is_projectile_beh(int b) { return b == WB_PROJECTILE || b == WB_LOB || b == WB_FLOAT; }

static uint8_t sprite_of_pk(int pk)
{
    switch (pk) {
    case PK_FIREBALL: return SPR_FIREBALL;
    case PK_VAPE_BOLT: case PK_ORB: return SPR_VAPE_BOLT;
    case PK_PLASMA: case PK_BFG: case PK_ICE: case PK_SLIME: return SPR_PLASMA;
    case PK_ROCKET: case PK_GRENADE: return SPR_ROCKET;
    case PK_BUBBLE: case PK_WATER: case PK_YARN: return SPR_BUBBLE;
    case PK_GLITTER: case PK_STICKER: return SPR_GLITTER_SHOT;
    case PK_HEART: case PK_CONFETTI: return SPR_HEARTS;
    case PK_PLUSH: case PK_CANDY: return SPR_CANDY;
    case PK_HOLY: return SPR_HOLY_SPLASH;
    case PK_FLAME: return SPR_FIREBALL;
    default: return pk ? SPR_SPARKLE : 0;
    }
}

/* the core: what comes out of the gun */
typedef struct Work {
    int dmg, pellets, spread, refire, ammo, use, speed, range, mag, reload;
    int recoil, zoom, autof, knock, fx, sfx, beh, pk, splash, spin, charge;
    int weight, crit, burst, under_every, lifesteal, quiet, holy;
} Work;

static void set_proj(Work *k, int beh, int pk, int speed)
{
    if (k->beh == WB_MELEE) return;
    k->beh = beh;
    k->pk = pk;
    k->speed = speed;
}

static void apply_core(Work *k, const FrameBase *f, int core)
{
    int melee = k->beh == WB_MELEE;
    int stream = f->beh == WB_STREAM;
    if (core == f->core) return;                       /* the natural core */
    if (melee) {
        /* melee weapons only take the element of the core */
        switch (core) {
        case WCORE_HELLFIRE: case WCORE_FUEL: k->fx |= WFX_BURN; k->dmg = mul16(k->dmg, 19); break;
        case WCORE_ICE: k->fx |= WFX_FREEZE; break;
        case WCORE_HOLY_WATER: k->fx |= WFX_HOLY; k->holy += 12; break;
        case WCORE_LOVE: k->fx |= WFX_CHARM | WFX_HOLY; k->holy += 6; break;
        case WCORE_SAWBLADE: k->dmg = mul16(k->dmg, 20); break;
        case WCORE_EXPLOSIVE_ROUND: case WCORE_ROCKET: case WCORE_GRENADE:
            k->fx |= WFX_SPLASH; k->splash += 16; k->dmg = mul16(k->dmg, 18); break;
        case WCORE_PLASMA: case WCORE_RAIL_SLUG: k->dmg = mul16(k->dmg, 19); break;
        case WCORE_GLITTER: case WCORE_BUBBLE: k->fx |= WFX_FREEZE; break;
        case WCORE_SLIME: k->fx |= WFX_BURN; break;
        case WCORE_CANDY: case WCORE_PLUSH: k->knock += 8; break;
        case WCORE_AP_BULLET: case WCORE_FLECHETTE: k->fx |= WFX_PIERCE; break;
        default: break;
        }
        return;
    }
    switch (core) {
    case WCORE_BULLET: case WCORE_AP_BULLET: case WCORE_EXPLOSIVE_ROUND:
        if (k->beh != WB_HITSCAN) {
            k->beh = WB_HITSCAN; k->pk = PK_NONE; k->speed = 0;
            k->splash = 0; k->fx &= ~(WFX_SPLASH | WFX_BOUNCE);
            if (stream) { k->dmg = k->dmg * 2 + 4; k->range = 32; k->refire = 2; }
        }
        k->ammo = AMMO_BULLETS;
        if (core == WCORE_AP_BULLET) { k->dmg = mul16(k->dmg, 18); k->fx |= WFX_PIERCE; }
        if (core == WCORE_EXPLOSIVE_ROUND) {
            k->dmg = mul16(k->dmg, 18); k->fx |= WFX_SPLASH; k->splash += 16;
        }
        break;
    case WCORE_SHELL: case WCORE_SLUG: case WCORE_FLECHETTE:
        if (k->beh != WB_HITSCAN) {
            k->beh = WB_HITSCAN; k->pk = PK_NONE; k->speed = 0;
            k->splash = 0; k->fx &= ~(WFX_SPLASH | WFX_BOUNCE);
            if (stream) { k->dmg = k->dmg + 4; k->range = 20; k->refire = 6; }
        }
        k->ammo = AMMO_SHELLS;
        if (core == WCORE_SHELL && f->wclass != WCLASS_SHOTGUN) {
            k->pellets *= 4;
            k->dmg = k->dmg * 2 / 5;
            k->spread = k->spread * 3 + 24;
            k->range = k->range * 3 / 5;
            k->knock += 4;
        } else if (core == WCORE_SLUG) {
            if (f->wclass == WCLASS_SHOTGUN) {
                k->dmg = k->dmg * k->pellets * 3 / 4;
                k->pellets = 1;
                k->spread = 4;
                k->range *= 2;
            } else {
                k->dmg = mul16(k->dmg, 20);
                k->refire = mul16(k->refire, 19) + 1;
            }
            k->knock += 6;
        } else if (core == WCORE_FLECHETTE) {
            k->pellets *= 2;
            k->dmg = k->dmg * 3 / 5;
            k->spread = mul16(k->spread, 21) + 4;
            k->fx |= WFX_PIERCE;
        }
        break;
    case WCORE_PLASMA:
        if (stream) k->pk = PK_PLASMA; else set_proj(k, WB_PROJECTILE, PK_PLASMA, 30);
        k->ammo = AMMO_CELLS;
        k->dmg = mul16(k->dmg, 17) + 2;
        break;
    case WCORE_RAIL_SLUG:
        k->beh = WB_RAIL; k->pk = PK_NONE; k->speed = 0;
        k->ammo = AMMO_CELLS;
        k->dmg = mul16(k->dmg, 24) + 10;
        k->refire = DOOMO_MAX(mul16(k->refire, 26), 18);
        k->use = DOOMO_MAX(k->use, 1) * 2;
        k->pellets = DOOMO_MIN(k->pellets, 2);
        k->fx |= WFX_PIERCE;
        k->fx &= ~(WFX_SPLASH | WFX_BOUNCE);
        k->splash = 0;
        k->recoil += 8;
        k->sfx = SFX_PLASMA;
        break;
    case WCORE_ROCKET:
        set_proj(k, WB_PROJECTILE, PK_ROCKET, 26);
        k->ammo = AMMO_ROCKETS;
        k->pellets = DOOMO_MIN(k->pellets, 3);
        k->dmg = DOOMO_MAX(mul16(k->dmg, 22), 40);
        k->refire = DOOMO_MAX(mul16(k->refire, 24), 12);
        k->fx |= WFX_SPLASH;
        k->splash = DOOMO_MAX(k->splash, 48);
        k->sfx = SFX_ROCKET;
        break;
    case WCORE_GRENADE:
        set_proj(k, WB_LOB, PK_GRENADE, 20);
        k->ammo = AMMO_ROCKETS;
        k->pellets = DOOMO_MIN(k->pellets, 3);
        k->dmg = DOOMO_MAX(mul16(k->dmg, 20), 35);
        k->refire = DOOMO_MAX(mul16(k->refire, 22), 12);
        k->fx |= WFX_SPLASH | WFX_BOUNCE;
        k->splash = DOOMO_MAX(k->splash, 44);
        k->sfx = SFX_ROCKET;
        break;
    case WCORE_NAIL:
        if (!stream) set_proj(k, WB_PROJECTILE, PK_NAIL, 36);
        else { k->pk = PK_NAIL; k->dmg += 3; }
        k->ammo = AMMO_NAILS;
        break;
    case WCORE_FUEL:
        k->beh = WB_STREAM; k->pk = PK_FLAME; k->speed = 14;
        k->ammo = AMMO_FUEL;
        k->fx |= WFX_BURN;
        k->fx &= ~(WFX_SPLASH | WFX_BOUNCE);
        k->splash = 0;
        k->dmg = DOOMO_MAX(k->dmg * k->pellets / 3, 3);
        k->pellets = 1;
        k->refire = DOOMO_MAX(k->refire / 3, 1);
        k->range = DOOMO_MIN(k->range, 7);
        k->autof = 1;
        k->sfx = SFX_ROCKET;
        break;
    case WCORE_BUBBLE:
        if (stream) k->pk = PK_BUBBLE; else set_proj(k, WB_FLOAT, PK_BUBBLE, 7);
        k->ammo = AMMO_CANDY;
        k->fx |= WFX_FREEZE;
        k->dmg = mul16(k->dmg, 13) + 2;
        k->sfx = SFX_BUBBLE;
        break;
    case WCORE_GLITTER:
        if (!stream) set_proj(k, WB_PROJECTILE, PK_GLITTER, 24);
        else k->pk = PK_GLITTER;
        k->ammo = AMMO_CANDY;
        k->fx |= WFX_FREEZE;
        k->dmg = mul16(k->dmg, 14) + 1;
        k->sfx = SFX_SPARKLE;
        break;
    case WCORE_HOLY_WATER:
        if (is_projectile_beh(k->beh) || stream) k->pk = stream ? PK_WATER : PK_HOLY;
        k->ammo = AMMO_CANDY;
        k->fx |= WFX_HOLY;
        k->holy += 16;
        k->sfx = SFX_SPLASH;
        break;
    case WCORE_HELLFIRE:
        if (is_projectile_beh(k->beh)) k->pk = PK_FIREBALL;
        if (stream) k->pk = PK_FLAME;
        k->fx |= WFX_BURN;
        k->dmg = mul16(k->dmg, 19) + 1;
        break;
    case WCORE_ICE:
        if (is_projectile_beh(k->beh) || stream) k->pk = PK_ICE;
        k->fx |= WFX_FREEZE;
        k->dmg = mul16(k->dmg, 15);
        break;
    case WCORE_LOVE:
        if (is_projectile_beh(k->beh) || stream) k->pk = PK_HEART;
        k->ammo = AMMO_CANDY;
        k->fx |= WFX_CHARM | WFX_HOLY;
        k->holy += 4;
        k->dmg = mul16(k->dmg, 13) + 1;
        k->sfx = SFX_LOVE;
        break;
    case WCORE_SAWBLADE:
        set_proj(k, WB_PROJECTILE, PK_SAW, 22);
        k->ammo = AMMO_NAILS;
        k->fx |= WFX_BOUNCE | WFX_PIERCE;
        k->dmg = mul16(k->dmg, 21) + 4;
        k->refire = DOOMO_MAX(mul16(k->refire, 21), 6);
        k->pellets = DOOMO_MIN(k->pellets, 3);
        break;
    case WCORE_SLIME:
        set_proj(k, WB_LOB, PK_SLIME, 18);
        k->ammo = AMMO_CANDY;
        k->fx |= WFX_SPLASH | WFX_BURN;
        k->splash = DOOMO_MAX(k->splash, 32);
        k->pellets = DOOMO_MIN(k->pellets, 4);
        k->refire = DOOMO_MAX(k->refire, 5);
        k->sfx = SFX_SPLAT;
        break;
    case WCORE_CANDY:
        if (!stream) set_proj(k, WB_PROJECTILE, PK_CANDY, 24);
        else k->pk = PK_CANDY;
        k->ammo = AMMO_CANDY;
        k->dmg = mul16(k->dmg, 15);
        k->knock += 4;
        break;
    case WCORE_PLUSH:
        set_proj(k, WB_LOB, PK_PLUSH, 18);
        k->ammo = AMMO_CANDY;
        k->fx |= WFX_BOUNCE;
        k->fx &= ~WFX_SPLASH;
        k->splash = 0;
        k->dmg = mul16(k->dmg, 18) + 2;
        k->knock = k->knock * 2 + 6;
        k->refire = DOOMO_MAX(mul16(k->refire, 22), 8);
        k->pellets = DOOMO_MIN(k->pellets, 3);
        k->sfx = SFX_SQUEAK;
        break;
    default:
        break;
    }
}

void weld_stats(const DoomoWeapon *w, DoomoWeaponStats *out)
{
    static const DoomoWeapon fallback = { WFRAME_FISTS, 0, 0, 0, 0, 0, 0, 0, 0,
                                          WSTYLE_CUTE, PAINT_ROSE, PAINT_WHITE, 50,
                                          WF_VALID, 0, 0 };
    const FrameBase *f;
    Work k;
    int q, lvl;
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!w) w = &fallback;
    f = &frames[w->frame < WFRAME_COUNT ? w->frame : WFRAME_FISTS];
    k.dmg = f->dmg; k.pellets = f->pellets; k.spread = f->spread; k.refire = f->refire;
    k.ammo = f->ammo; k.use = f->use; k.speed = f->speed; k.range = f->range;
    k.mag = f->mag; k.reload = f->reload; k.recoil = f->recoil; k.zoom = f->zoom;
    k.autof = f->autof; k.knock = f->knock; k.fx = f->fx; k.sfx = f->sfx;
    k.beh = f->beh; k.pk = f->pk; k.splash = f->splash; k.spin = f->spin;
    k.charge = f->charge; k.weight = f->weight; k.crit = f->crit; k.burst = 1;
    k.under_every = 0; k.lifesteal = 0; k.quiet = (w->frame == WFRAME_BOW); k.holy = 16;

    /* 2. ammo core */
    apply_core(&k, f, w->core < WCORE_COUNT ? w->core : f->core);

    /* 3a. barrel */
    if (k.beh != WB_MELEE) {
        switch (w->barrel) {
        case WBARREL_SHORT: k.spread = mul16(k.spread, 22) + 1; k.range = mul16(k.range, 12);
            k.refire = mul16(k.refire, 15); k.weight -= 5; break;
        case WBARREL_LONG: k.spread = mul16(k.spread, 10); k.range = mul16(k.range, 21);
            k.dmg = mul16(k.dmg, 18); k.weight += 8; k.speed = mul16(k.speed, 19); break;
        case WBARREL_HEAVY: k.dmg = mul16(k.dmg, 19); k.refire = mul16(k.refire, 19) + 1;
            k.recoil = mul16(k.recoil, 21) + 1; k.knock += 4; k.weight += 10; break;
        case WBARREL_DOUBLE: k.pellets *= 2; k.use = k.use * 2; k.spread = mul16(k.spread, 18) + 3;
            k.recoil = mul16(k.recoil, 22); k.weight += 8; break;
        case WBARREL_TRIPLE: k.pellets *= 3; k.use = k.use * 3; k.spread = mul16(k.spread, 20) + 5;
            k.dmg = mul16(k.dmg, 14); k.recoil = mul16(k.recoil, 24); k.weight += 14; break;
        case WBARREL_SPIRAL: k.spread = k.spread / 2; k.speed = mul16(k.speed, 21);
            k.range = mul16(k.range, 18); break;
        case WBARREL_WIDE: k.pellets = k.pellets * 3 / 2 + 2; k.spread = mul16(k.spread, 29) + 10;
            k.dmg = mul16(k.dmg, 11); k.range = mul16(k.range, 12); break;
        case WBARREL_SAWED: k.spread = k.spread * 2 + 6; k.dmg = mul16(k.dmg, 20);
            k.range = k.range / 2; k.weight -= 6; k.knock += 4; break;
        case WBARREL_SUPPRESSED: k.dmg = mul16(k.dmg, 14); k.recoil = mul16(k.recoil, 11);
            k.quiet = 1; break;
        case WBARREL_HEART: k.fx |= WFX_CHARM; k.spread = mul16(k.spread, 18); break;
        case WBARREL_STAR:
            k.pellets *= 3; k.dmg = k.dmg / 2 + 1; k.spread = mul16(k.spread, 18) + 8; break;
        default: break;
        }
    } else {
        /* a barrel welded on a melee weapon adds reach / heft */
        if (w->barrel == WBARREL_HEAVY) { k.dmg = mul16(k.dmg, 20); k.refire = mul16(k.refire, 19) + 1; }
        else if (w->barrel == WBARREL_LONG) k.spread += 10;
        else if (w->barrel == WBARREL_SAWED || w->barrel == WBARREL_SHORT) k.refire = mul16(k.refire, 13);
        else if (w->barrel == WBARREL_HEART) k.fx |= WFX_CHARM;
        else if (w->barrel == WBARREL_STAR) k.crit += 8;
    }

    /* 3b. mod */
    switch (w->mod) {
    case WMOD_EXT_MAG: k.mag = mul16(k.mag, 24); break;
    case WMOD_SPIKES: k.knock += 6; if (k.beh == WB_MELEE) k.dmg = mul16(k.dmg, 20); break;
    case WMOD_BAYONET: k.knock += 4; if (k.beh == WB_MELEE) k.dmg = mul16(k.dmg, 18); break;
    case WMOD_STICKERS: k.dmg = mul16(k.dmg, 17); k.crit += 4; break;
    case WMOD_COOLER: k.refire = mul16(k.refire, 14); k.reload = mul16(k.reload, 14); break;
    case WMOD_OVERCLOCK: k.refire = mul16(k.refire, 11); k.recoil = mul16(k.recoil, 21) + 1;
        k.spread = mul16(k.spread, 19) + 1; break;
    case WMOD_RICOCHET: k.fx |= WFX_BOUNCE; break;
    case WMOD_HOMING: k.fx |= WFX_HOMING; break;
    case WMOD_BURST: if (k.beh != WB_MELEE) { k.burst = 3; k.refire = mul16(k.refire, 36) + 4; k.autof = 0; } break;
    case WMOD_FULL_AUTO: k.autof = 1; k.refire = mul16(k.refire, 14); break;
    case WMOD_CHARGE_SHOT: if (k.charge == 0) k.charge = 20; break;
    case WMOD_LIFESTEAL: k.lifesteal += 10; break;
    case WMOD_SQUEAKER: k.sfx = SFX_SQUEAK; k.knock += 4; break;
    default: break;
    }

    /* 3c. sight */
    if (k.beh != WB_MELEE) {
        switch (w->sight) {
        case WSIGHT_RED_DOT: k.spread = mul16(k.spread, 14); if (!k.zoom) k.zoom = 1; break;
        case WSIGHT_HOLO: k.spread = mul16(k.spread, 13); if (!k.zoom) k.zoom = 1; break;
        case WSIGHT_SCOPE_4X: k.spread = mul16(k.spread, 10); k.zoom = DOOMO_MAX(k.zoom, 2);
            k.weight += 4; k.crit += 8; break;
        case WSIGHT_SCOPE_8X: k.spread = mul16(k.spread, 7); k.zoom = 3; k.weight += 8;
            k.crit += 16; k.range = DOOMO_MAX(k.range, 56); break;
        case WSIGHT_THERMAL: k.zoom = DOOMO_MAX(k.zoom, 2); k.weight += 6; k.crit += 4; break;
        case WSIGHT_HEART_DOT: k.spread = mul16(k.spread, 14); if (!k.zoom) k.zoom = 1; k.crit += 8; break;
        case WSIGHT_LASER: k.spread = mul16(k.spread, 12); break;
        default: break;
        }
    }

    /* 3d. stock */
    switch (w->stock) {
    case WSTOCK_NONE: k.recoil = mul16(k.recoil, 19) + 1; k.weight -= 4; break;
    case WSTOCK_FIXED: k.recoil = mul16(k.recoil, 13); break;
    case WSTOCK_FOLDING: k.recoil = mul16(k.recoil, 14); k.weight -= 2; break;
    case WSTOCK_HEAVY: k.recoil = mul16(k.recoil, 10); k.weight += 10; k.spread = mul16(k.spread, 14); break;
    case WSTOCK_SKELETON: k.recoil = mul16(k.recoil, 14); k.weight -= 6; break;
    case WSTOCK_PLUSH: k.recoil = mul16(k.recoil, 11); k.knock += 2; break;
    default: break;
    }

    /* 3e. magazine */
    if (k.mag > 0) {
        switch (w->mag) {
        case WMAG_EXTENDED: k.mag = mul16(k.mag, 24); k.reload = mul16(k.reload, 18); break;
        case WMAG_DRUM: k.mag = mul16(k.mag, 40); k.reload = mul16(k.reload, 24); k.weight += 8; break;
        case WMAG_QUICK: k.reload = mul16(k.reload, 10); break;
        case WMAG_BELT: k.mag = k.mag * 4; k.reload = k.reload * 2; k.weight += 14; break;
        case WMAG_CANDY_TUBE: k.mag = mul16(k.mag, 19); break;
        default: break;
        }
    }
    if (w->mag == WMAG_CANDY_TUBE && k.ammo != NOAMMO) k.ammo = AMMO_CANDY;

    /* 3f. underbarrel */
    switch (w->under) {
    case WUNDER_GRIP: k.spread = mul16(k.spread, 13); k.recoil = mul16(k.recoil, 13); break;
    case WUNDER_GRENADE: k.under_every = 6; k.weight += 6; break;
    case WUNDER_SHOTGUN: k.under_every = 4; k.weight += 6; break;
    case WUNDER_FLASHLIGHT: k.crit += 2; break;
    case WUNDER_CHAINSAW: k.under_every = 1; k.weight += 8; break;   /* contact saw */
    case WUNDER_BUBBLE: k.under_every = 3; break;
    default: break;
    }

    /* 3g. charm */
    switch (w->charm) {
    case WCHARM_TEDDY: k.knock += 2; break;
    case WCHARM_SKULL: k.dmg = mul16(k.dmg, 18); break;
    case WCHARM_DICE: k.crit += 12; break;
    case WCHARM_HEART: k.lifesteal += 5; break;
    case WCHARM_EYEBALL: k.spread = mul16(k.spread, 14); break;
    case WCHARM_BELL: k.reload = mul16(k.reload, 13); break;
    case WCHARM_ROSARY: k.fx |= WFX_HOLY; k.holy += 8; break;
    default: break;
    }

    /* 3h. style, quality, level */
    if (w->style == WSTYLE_BRUTAL) { k.dmg = mul16(k.dmg, 17) + 0; k.recoil = mul16(k.recoil, 18); }
    else { k.knock += 2; k.recoil = mul16(k.recoil, 15); }
    q = w->quality > 100 ? 100 : w->quality;
    k.dmg = (k.dmg * (300 + q * 2) + 200) / 400;                 /* x0.75 .. x1.25 */
    k.spread = k.spread * (500 - q) / 400;                       /* x1.25 .. x1.0 */
    k.reload = k.reload * (480 - q) / 400;
    lvl = weld_level(w);
    k.dmg = k.dmg * (50 + lvl) / 50;                             /* +2% per level */

    /* 4. clamp everything into sane ranges */
    out->damage = (int16_t)DOOMO_CLAMP(k.dmg, 1, 999);
    out->pellets = (uint8_t)DOOMO_CLAMP(k.pellets, 1, 12);
    out->spread = (uint8_t)DOOMO_CLAMP(k.spread, 0, 120);
    out->refire = (uint8_t)DOOMO_CLAMP(k.refire, 1, 90);
    out->ammo_type = (uint8_t)(k.ammo == NOAMMO ? NOAMMO : DOOMO_CLAMP(k.ammo, 0, AMMO_COUNT - 1));
    out->ammo_use = (uint8_t)(k.ammo == NOAMMO ? 0 : DOOMO_CLAMP(k.use, 1, 40));
    out->behaviour = (uint8_t)k.beh;
    out->pkind = (uint8_t)((k.beh == WB_HITSCAN || k.beh == WB_RAIL || k.beh == WB_MELEE) ? PK_NONE : k.pk);
    if ((k.beh == WB_PROJECTILE || k.beh == WB_LOB || k.beh == WB_FLOAT || k.beh == WB_STREAM) &&
        out->pkind == PK_NONE)
        out->pkind = PK_CANDY;
    out->projectile = sprite_of_pk(out->pkind);
    out->speed = (int16_t)(out->pkind ? DOOMO_CLAMP(k.speed, 6, 60) : 0);
    out->range = (uint8_t)(k.beh == WB_MELEE ? 1 : DOOMO_CLAMP(k.range, 3, 64));
    out->mag_size = (uint8_t)(k.ammo == NOAMMO ? 0 : DOOMO_CLAMP(k.mag, 0, 250));
    out->reload = (uint8_t)(out->mag_size ? DOOMO_CLAMP(k.reload, 8, 200) : 0);
    out->recoil = (uint8_t)DOOMO_CLAMP(k.recoil, 0, 60);
    out->zoom = (uint8_t)DOOMO_CLAMP(k.zoom, 0, 3);
    out->auto_fire = (uint8_t)(k.autof ? 1 : 0);
    out->knockback = (uint8_t)DOOMO_CLAMP(k.knock, 0, 60);
    out->fx = (uint8_t)k.fx;
    if (k.splash > 0) out->fx |= WFX_SPLASH;
    out->sfx = (uint8_t)k.sfx;
    out->wclass = f->wclass;
    out->splash = (uint8_t)DOOMO_CLAMP(k.splash, 0, 120);
    if ((out->fx & WFX_SPLASH) && out->splash == 0) out->splash = 16;
    out->spin_up = (uint8_t)DOOMO_CLAMP(k.spin, 0, 60);
    out->charge = (uint8_t)DOOMO_CLAMP(k.charge, 0, 60);
    out->burst = (uint8_t)DOOMO_CLAMP(k.burst, 1, 5);
    out->crit = (uint8_t)DOOMO_CLAMP(k.crit, 16, 96);
    out->weight = (uint8_t)DOOMO_CLAMP(k.weight, 0, 100);
    out->under_every = (uint8_t)DOOMO_CLAMP(k.under_every, 0, 12);
    out->lifesteal = (uint8_t)DOOMO_CLAMP(k.lifesteal, 0, 50);
    out->quiet = (uint8_t)(k.quiet ? 1 : 0);
    out->gore = (uint8_t)(w->style == WSTYLE_BRUTAL ? WSTYLE_BRUTAL : WSTYLE_CUTE);
    out->holy = (uint8_t)DOOMO_CLAMP(k.holy, 16, 64);
}

int weld_level(const DoomoWeapon *w)
{
    int l = w ? w->xp / 32 : 0;
    return l > 7 ? 7 : l;
}

int weld_dps(const DoomoWeaponStats *s)
{
    int per = s->damage * s->pellets * s->burst;
    if (s->splash) per += s->damage / 2;
    return per * 30 / (s->refire ? s->refire : 1);
}

static int isqrt_i(int v) { return v > 0 ? (int)fx_isqrt((uint32_t)v) : 0; }

void weld_ratings(const DoomoWeaponStats *s, uint8_t out[WRATE_COUNT])
{
    int total = s->damage * s->pellets * s->burst + (s->splash ? s->damage / 2 : 0);
    int v;
    v = isqrt_i(total * 10000 / 300);                  out[WRATE_DAMAGE] = (uint8_t)DOOMO_CLAMP(v, 1, 100);
    v = 300 / (s->refire + 2);                         out[WRATE_RATE] = (uint8_t)DOOMO_CLAMP(v, 1, 100);
    v = s->behaviour == WB_MELEE ? 4 : s->range * 100 / 64; out[WRATE_RANGE] = (uint8_t)DOOMO_CLAMP(v, 1, 100);
    v = 100 - s->spread * 100 / 96;                    out[WRATE_ACCURACY] = (uint8_t)DOOMO_CLAMP(v, 1, 100);
    if (s->ammo_type == 0xFF || s->mag_size == 0) v = 100;
    else v = isqrt_i(s->mag_size * 10000 / 120);
    out[WRATE_AMMO] = (uint8_t)DOOMO_CLAMP(v, 1, 100);
    v = s->mag_size ? 100 - s->reload * 100 / 150 : 100; out[WRATE_RELOAD] = (uint8_t)DOOMO_CLAMP(v, 1, 100);
    v = 100 - s->recoil * 100 / 45;                    out[WRATE_CONTROL] = (uint8_t)DOOMO_CLAMP(v, 1, 100);
    v = 100 - s->weight;                               out[WRATE_MOBILITY] = (uint8_t)DOOMO_CLAMP(v, 1, 100);
}

/* ---- parts by list --------------------------------------------------------- */
int weld_part_count(int clist)
{
    switch (clist) {
    case CLIST_WFRAME: return WFRAME_COUNT;
    case CLIST_WBARREL: return WBARREL_COUNT;
    case CLIST_WCORE: return WCORE_COUNT;
    case CLIST_WMOD: return WMOD_COUNT;
    case CLIST_WSIGHT: return WSIGHT_COUNT;
    case CLIST_WSTOCK: return WSTOCK_COUNT;
    case CLIST_WMAG: return WMAG_COUNT;
    case CLIST_WUNDER: return WUNDER_COUNT;
    case CLIST_WCHARM: return WCHARM_COUNT;
    default: return 0;
    }
}

const char *weld_part_name(int clist, int i)
{
    if (i < 0 || i >= weld_part_count(clist)) return "";
    switch (clist) {
    case CLIST_WFRAME: return doomo_wframe_names[i];
    case CLIST_WBARREL: return doomo_wbarrel_names[i];
    case CLIST_WCORE: return doomo_wcore_names[i];
    case CLIST_WMOD: return doomo_wmod_names[i];
    case CLIST_WSIGHT: return doomo_wsight_names[i];
    case CLIST_WSTOCK: return doomo_wstock_names[i];
    case CLIST_WMAG: return doomo_wmag_names[i];
    case CLIST_WUNDER: return doomo_wunder_names[i];
    case CLIST_WCHARM: return doomo_wcharm_names[i];
    default: return "";
    }
}

int weld_part_get(const DoomoWeapon *w, int clist)
{
    switch (clist) {
    case CLIST_WFRAME: return w->frame;
    case CLIST_WBARREL: return w->barrel;
    case CLIST_WCORE: return w->core;
    case CLIST_WMOD: return w->mod;
    case CLIST_WSIGHT: return w->sight;
    case CLIST_WSTOCK: return w->stock;
    case CLIST_WMAG: return w->mag;
    case CLIST_WUNDER: return w->under;
    case CLIST_WCHARM: return w->charm;
    default: return 0;
    }
}

void weld_part_set(DoomoWeapon *w, int clist, int v)
{
    int n = weld_part_count(clist);
    if (n <= 0) return;
    v = DOOMO_CLAMP(v, 0, n - 1);
    switch (clist) {
    case CLIST_WFRAME: w->frame = (uint8_t)v; break;
    case CLIST_WBARREL: w->barrel = (uint8_t)v; break;
    case CLIST_WCORE: w->core = (uint8_t)v; break;
    case CLIST_WMOD: w->mod = (uint8_t)v; break;
    case CLIST_WSIGHT: w->sight = (uint8_t)v; break;
    case CLIST_WSTOCK: w->stock = (uint8_t)v; break;
    case CLIST_WMAG: w->mag = (uint8_t)v; break;
    case CLIST_WUNDER: w->under = (uint8_t)v; break;
    case CLIST_WCHARM: w->charm = (uint8_t)v; break;
    default: break;
    }
}

void weld_default(DoomoWeapon *w, int frame, int style)
{
    const FrameBase *f;
    memset(w, 0, sizeof(*w));
    if (frame < 0 || frame >= WFRAME_COUNT) frame = WFRAME_PISTOL;
    f = &frames[frame];
    w->frame = (uint8_t)frame;
    w->core = f->core;
    w->barrel = WBARREL_STANDARD;
    w->mod = WMOD_NONE;
    w->sight = f->wclass == WCLASS_PRECISION ? WSIGHT_SCOPE_4X : WSIGHT_IRON;
    w->stock = (f->wclass == WCLASS_SIDEARM || f->wclass == WCLASS_MELEE || f->wclass == WCLASS_CUTE)
                   ? WSTOCK_NONE : WSTOCK_FIXED;
    w->mag = WMAG_STANDARD;
    w->under = WUNDER_NONE;
    w->style = (uint8_t)(style == WSTYLE_BRUTAL ? WSTYLE_BRUTAL : WSTYLE_CUTE);
    if (w->style == WSTYLE_CUTE) {
        w->paint = f->wclass == WCLASS_CUTE ? PAINT_BUBBLEGUM : PAINT_LAVENDER;
        w->accent = f->wclass == WCLASS_CUTE ? PAINT_SKY : PAINT_LEMON;
        w->charm = f->wclass == WCLASS_CUTE ? WCHARM_HEART : WCHARM_TEDDY;
    } else {
        w->paint = PAINT_CHARCOAL;
        w->accent = PAINT_BLOOD;
        w->charm = WCHARM_SKULL;
    }
    w->quality = 60;
    w->flags = WF_VALID;
}

/* ---- names ----------------------------------------------------------------- */
/* Five cute + five brutal nouns per frame. */
static const char *const frame_nouns[WFRAME_COUNT][10] = {
    { "Popper", "Pew-Pew", "Peashooter", "Pocket Pal", "Plinker",
      "Sidearm", "Hand Cannon", "Iron", "Bonebreaker", "Persuader" },
    { "Six-Shooter", "Spinny", "Cowpoke", "Wheelie", "Tumbler",
      "Hogleg", "Judge", "Widowmaker", "Peacekeeper", "Magnum" },
    { "Twin Poppers", "Duo", "Besties", "Pals", "Double Trouble",
      "Akimbos", "Twin Fangs", "Dual Ruin", "Gemini", "Twin Reapers" },
    { "Buzzy", "Rattler", "Brrrt", "Chatterbox", "Hummer",
      "Stinger", "Spitfire", "Hornet", "Ripper", "Viper" },
    { "Sprinkler", "Tickler", "Chitter", "Purr Gun", "Zippy",
      "Shredder", "Bullet Hose", "Grinder", "Mauler", "Chopper" },
    { "Party Rifle", "Sparkler", "Starburst", "Cuddle Carbine", "Dazzler",
      "Warbringer", "Reaper", "Carbine", "Ravager", "War Rifle" },
    { "Big Hugger", "Thumper", "Bonk Rifle", "Sunbeam", "Marshmallow",
      "Battleaxe", "Crusher", "Executioner", "Warlord", "Rampart" },
    { "Snugglepup", "Puppy", "Kitten", "Hopper", "Bunny",
      "Butcher", "Wolfbite", "Jackal", "Razorback", "Hellhound" },
    { "Cuddle Storm", "Rainmaker", "Bubble Bath", "Fluffstorm", "Hailstorm",
      "Meat Shredder", "Suppressor", "Bulldozer", "Harvester", "Gutripper" },
    { "Spinner", "Whirligig", "Tornado", "Spin Cycle", "Twirlybird",
      "Meat Grinder", "Buzzsaw", "Annihilator", "Death Spin", "Gatling" },
    { "Stargazer", "Twinkle", "Peek-a-Boo", "Eyelash", "Lookie-Loo",
      "Marksman", "Ranger", "Deadeye", "Headhunter", "Longarm" },
    { "Snoozer", "Far Kiss", "Telescope", "Long Hug", "Birdwatcher",
      "Widow", "Silencer", "Skullsplitter", "Longshot", "Last Word" },
    { "Rainbow Rail", "Zap Zap", "Laser Pony", "Sparkle Beam", "Lightning Bug",
      "Railspike", "Lancer", "Impaler", "Obliterator", "Skewer" },
    { "Cupid", "Pokey", "Twang", "Dart Pal", "Pin Pal",
      "Ballista", "Quarrel", "Bolt Thrower", "Piercer", "Arbalest" },
    { "Wishbow", "Rainbow Bow", "Twangle", "Archie", "Featherfling",
      "Longbow", "Warbow", "Huntress", "Recurve", "Bonebow" },
    { "Boomstick", "Pop-Pop", "Party Pump", "Honker", "Boopstick",
      "Street Sweeper", "Thunder", "Slaughter", "Scattergun", "Boomstick" },
    { "Double Hug", "Mega Boop", "Big Smooch", "Boop Boop", "Twin Honk",
      "Super Shotgun", "Doom Barrels", "Gorehound", "Meathook", "Hellmouth" },
    { "Party Popper", "Hailey", "Pinata", "Fizzer", "Sprayer",
      "Auto Sweeper", "Riot", "Abattoir", "Torrent", "Shredstorm" },
    { "Yeehaw", "Clicky", "Saddlebag", "Lasso", "Rodeo",
      "Lever Gun", "Outlaw", "Gunslinger", "Drifter", "Sheriff" },
    { "Bouncy Castle", "Pop Tube", "Boing Boom", "Bubble Bomb", "Ka-Boing",
      "Thumper", "Bloop", "Boomer", "Frag Tube", "Pineapple" },
    { "Rocket Pony", "Moonshot", "Fireworks", "Sky Hug", "Blast-Off",
      "Bazooka", "Devastator", "Rocketeer", "Bunker Buster", "Hellbringer" },
    { "Toasty", "S'mores Maker", "Hot Cocoa", "Campfire", "Marshmallow",
      "Scorcher", "Inferno", "Pyre", "Immolator", "Hellbreath" },
    { "Glowbug", "Starshine", "Nebula", "Lumi", "Twinkler",
      "Plasma Rifle", "Fusion", "Ionizer", "Pulsar", "Melter" },
    { "Pin Cushion", "Tacky", "Sewing Kit", "Needle Pal", "Pokey",
      "Nailer", "Stakegun", "Crucifier", "Pinhead", "Spiker" },
    { "Big Friendly Gun", "Mega Hug", "Hug Bomb", "Love Bomb", "Sun Pup",
      "BFG", "Big Freakin' Gun", "Doombringer", "World Ender", "Oblivion" },
    { "Lawn Pal", "Buzz Buddy", "Purring Saw", "Snip Snip", "Tree Hugger",
      "Chainsaw", "Ripsaw", "Meatcleaver", "Gutter", "Hacker" },
    { "Moon Blade", "Sakura", "Petal Edge", "Kitty Claw", "Sparkle Sword",
      "Katana", "Muramasa", "Soulreaver", "Edge", "Oni Blade" },
    { "Bonk Stick", "Mallet", "Whack-a-Mole", "Big Boop", "Thwomp",
      "Sledgehammer", "Skullcrusher", "Maul", "Doomhammer", "Pulverizer" },
    { "Paws", "Mittens", "Hug Fists", "Boop Paws", "Kitty Punch",
      "Knuckles", "Fists", "Bonecrackers", "Pounders", "Brass" },
    { "Bubble Wand", "Wish Wand", "Sudsy", "Bubblina", "Fizz Wand",
      "Bubble Blaster", "Drowner", "Foamer", "Choker", "Lungbuster" },
    { "Plush Launcher", "Teddy Tosser", "Hug Cannon", "Stuffie Lobber", "Bear Mortar",
      "Doll Mortar", "Voodoo Lobber", "Effigy", "Puppet Gun", "Ragdoll" },
    { "Confetti Cannon", "Party Time", "Celebrator", "Hooray", "Fiesta",
      "Shrapnel Cannon", "Gore Popper", "Wake Maker", "Grim Fiesta", "Wreckfetti" },
    { "Lollipop Hammer", "Sugar Bonk", "Candy Mallet", "Swirly", "Sweet Tooth",
      "Jawbreaker", "Toothrot", "Sugarcrush", "Cavity", "Sour Smash" },
    { "Heart Bow", "Cupid's Kiss", "Lovebow", "Smooch", "Valentine",
      "Heartseeker", "Heartbreaker", "Bleeding Heart", "Love Spike", "Cardiac" },
    { "Squeaky Mallet", "Squeaker", "Honk Hammer", "Bonky", "Peep",
      "Shrieker", "Screamer", "Wailer", "Banshee", "Howler" },
    { "Glitter Sprayer", "Shimmer", "Sparkle Mist", "Glam", "Fairy Dust",
      "Blinder", "Glass Spray", "Razor Mist", "Eye Grater", "Glitterbomb" },
    { "Water Pistol", "Squirt", "Splashy", "Super Soaker", "Drizzle",
      "Drowner", "Holy Hose", "Baptizer", "Torrent", "Riptide" },
    { "Sticker Gun", "Stickerbomb", "Stampy", "Decal Blaster", "Sticky",
      "Tagger", "Brander", "Doom Stamp", "Sigil Gun", "Marker" },
    { "Yarn Crossbow", "Purl Shooter", "Knitty", "Ball Launcher", "Cozy Bolt",
      "Webber", "Snare", "Tangler", "Binder", "Cocoon" }
};

/* Three cute + three brutal adjectives per core. */
static const char *const core_adj[WCORE_COUNT][6] = {
    { "Plucky", "Trusty", "Pocket", "Leaden", "Brass", "Gunmetal" },
    { "Pointy", "Pokey", "Piercy", "Piercing", "Hollow", "Tungsten" },
    { "Poppy", "Fizzy", "Crackly", "Explosive", "Blasting", "Frag" },
    { "Scatter", "Sprinkly", "Buckshot", "Buckshot", "Scatter", "Shellfire" },
    { "Chunky", "Stout", "Plump", "Slugging", "Heavy", "Brick" },
    { "Needly", "Prickly", "Spiny", "Flechette", "Barbed", "Shredding" },
    { "Glowy", "Starry", "Shiny", "Plasma", "Fusion", "Searing" },
    { "Zoomy", "Rocket", "Whooshy", "Rocket", "Warhead", "Missile" },
    { "Bouncy", "Boingy", "Poppin'", "Frag", "Pineapple", "Concussion" },
    { "Tacky", "Pinny", "Stitchy", "Nailed", "Spiked", "Rusty" },
    { "Zappy", "Sparky", "Beamy", "Rail", "Magnetic", "Gauss" },
    { "Toasty", "Cozy", "Warm", "Burning", "Scorching", "Flaming" },
    { "Bubbly", "Sudsy", "Foamy", "Frothing", "Drowning", "Foaming" },
    { "Glittering", "Sparkly", "Shimmering", "Gleaming", "Blinding", "Shining" },
    { "Blessed", "Holy", "Angelic", "Sanctified", "Exorcist", "Consecrated" },
    { "Spicy", "Fiery", "Sizzly", "Hellfire", "Infernal", "Brimstone" },
    { "Frosty", "Chilly", "Snowy", "Glacial", "Frozen", "Cryo" },
    { "Lovely", "Huggy", "Sweetheart", "Lovesick", "Obsessive", "Passionate" },
    { "Spinny", "Whirly", "Twirly", "Sawtooth", "Ripping", "Rending" },
    { "Gooey", "Slimy", "Squishy", "Toxic", "Acidic", "Corrosive" },
    { "Sugary", "Sweet", "Candied", "Rotten", "Sour", "Sickly" },
    { "Fluffy", "Cuddly", "Snuggly", "Stuffed", "Stitched", "Cursed" }
};

static const char *const barrel_word[WBARREL_COUNT][2] = {
    { NULL, NULL }, { "Stubby", "Short" }, { "Long", "Long" }, { "Chunky", "Heavy" },
    { "Twin", "Double" }, { "Triple", "Triple" }, { "Swirly", "Rifled" },
    { "Wide", "Gaping" }, { "Sawn", "Sawed-Off" }, { "Hushy", "Silent" },
    { "Heart", "Heart" }, { "Star", "Star" }
};
static const char *const mod_word[WMOD_COUNT][2] = {
    { NULL, NULL }, { "Hungry", "Extended" }, { "Pointy", "Spiked" },
    { "Stabby", "Bayonet" }, { "Stickered", "Tagged" }, { "Chill", "Cooled" },
    { "Turbo", "Overclocked" }, { "Bouncy", "Ricochet" }, { "Seeking", "Homing" },
    { "Triple-Tap", "Burst" }, { "Rapid", "Auto" }, { "Charged", "Charged" },
    { "Vampy", "Vampiric" }, { "Squeaky", "Shrieking" }
};
static const char *const sight_word[WSIGHT_COUNT][2] = {
    { NULL, NULL }, { "Dotty", "Red Dot" }, { "Holo", "Holo" }, { "Scoped", "Scoped" },
    { "Far-Seeing", "Long-Range" }, { "Warm-Eye", "Thermal" }, { "Lovesight", "Heartsight" },
    { "Laser", "Laser" }
};
static const char *const mag_word[WMAG_COUNT][2] = {
    { NULL, NULL }, { "Hungry", "Extended" }, { "Drum", "Drum" }, { "Quick", "Quickload" },
    { "Belt-Fed", "Belt-Fed" }, { "Candy-Fed", "Candy-Fed" }
};
static const char *const under_word[WUNDER_COUNT][2] = {
    { NULL, NULL }, { "Grippy", "Gripped" }, { "Combo", "Combo" }, { "Combo", "Combo" },
    { "Glowy", "Lit" }, { "Sawtooth", "Sawtooth" }, { "Bubbly", "Bubbling" }
};
static const char *const charm_word[WCHARM_COUNT][2] = {
    { NULL, NULL }, { "Teddy's", "Teddy's" }, { "Spooky", "Skull" }, { "Lucky", "Gambler's" },
    { "Sweetheart", "Heartbound" }, { "Googly", "Watching" }, { "Jingly", "Tolling" },
    { "Blessed", "Pious" }
};

static int slen(const char *s) { int n = 0; while (s && s[n]) n++; return n; }

static void cat(char *buf, int cap, int *n, const char *s)
{
    while (s && *s && *n < cap - 1) buf[(*n)++] = *s++;
    buf[*n] = 0;
}

void weld_name(const DoomoWeapon *w, char *buf, int cap)
{
    static const char *const sfx_cute[3] = { "Jr.", "Plus", "Deluxe" };
    static const char *const sfx_brutal[3] = { "Scrap", "Mk.II", "Mk.III" };
    const char *adj, *noun, *mid = NULL, *suffix = NULL;
    const char *cand[6];
    char lvl[4];
    uint32_t h;
    int b, n = 0, nc = 0, len, q, level;
    if (!buf || cap <= 0) return;
    buf[0] = 0;
    if (!w) return;
    b = w->style == WSTYLE_BRUTAL ? 1 : 0;
    h = doomo_hash32((uint32_t)w->frame | ((uint32_t)w->barrel << 6) | ((uint32_t)w->core << 10) |
                     ((uint32_t)w->mod << 15) | ((uint32_t)w->sight << 19) |
                     ((uint32_t)w->stock << 22) | ((uint32_t)w->mag << 25) |
                     ((uint32_t)w->under << 28));
    h = doomo_hash32(h ^ ((uint32_t)w->charm << 4) ^ ((uint32_t)w->paint << 9) ^
                     ((uint32_t)w->accent << 15) ^ ((uint32_t)b << 21));
    noun = frame_nouns[w->frame < WFRAME_COUNT ? w->frame : WFRAME_FISTS][b * 5 + (int)(h % 5)];
    adj = core_adj[w->core < WCORE_COUNT ? w->core : 0][b * 3 + (int)((h >> 3) % 3)];
    /* cute weapons sometimes wear their paint as their adjective */
    if (!b && ((h >> 6) & 7) == 0 && w->paint < PAL_PAINT_COUNT && slen(doomo_paint_names[w->paint]) <= 9)
        adj = doomo_paint_names[w->paint];
    if (w->barrel < WBARREL_COUNT && barrel_word[w->barrel][b]) cand[nc++] = barrel_word[w->barrel][b];
    if (w->mod < WMOD_COUNT && mod_word[w->mod][b]) cand[nc++] = mod_word[w->mod][b];
    if (w->sight < WSIGHT_COUNT && sight_word[w->sight][b]) cand[nc++] = sight_word[w->sight][b];
    if (w->mag < WMAG_COUNT && mag_word[w->mag][b]) cand[nc++] = mag_word[w->mag][b];
    if (w->under < WUNDER_COUNT && under_word[w->under][b]) cand[nc++] = under_word[w->under][b];
    if (w->charm < WCHARM_COUNT && charm_word[w->charm][b]) cand[nc++] = charm_word[w->charm][b];
    if (nc) mid = cand[(h >> 9) % (uint32_t)nc];
    if (mid && (mid == adj || strcmp(mid, adj) == 0)) mid = NULL;
    q = w->quality;
    if (q < 25) suffix = b ? sfx_brutal[0] : sfx_cute[0];
    else if (q >= 95) suffix = b ? sfx_brutal[2] : sfx_cute[2];
    else if (q >= 80) suffix = b ? sfx_brutal[1] : sfx_cute[1];
    level = weld_level(w);
    lvl[0] = 0;
    if (level > 0) { lvl[0] = '+'; lvl[1] = (char)('0' + level); lvl[2] = 0; }
    /* fit into 31 characters: drop the suffix, then the middle word */
    for (;;) {
        len = slen(adj) + 1 + slen(noun);
        if (mid) len += slen(mid) + 1;
        if (suffix) len += slen(suffix) + 1;
        if (lvl[0]) len += 3;
        if (len <= 31 || (!suffix && !mid)) break;
        if (suffix) suffix = NULL; else mid = NULL;
    }
    if (len > 31) adj = NULL;
    if (adj) { cat(buf, cap, &n, adj); cat(buf, cap, &n, " "); }
    if (mid) { cat(buf, cap, &n, mid); cat(buf, cap, &n, " "); }
    cat(buf, cap, &n, noun);
    if (suffix) { cat(buf, cap, &n, " "); cat(buf, cap, &n, suffix); }
    if (lvl[0]) { cat(buf, cap, &n, " "); cat(buf, cap, &n, lvl); }
}

/* ---- material cost ---------------------------------------------------------- */
static int add_item(uint8_t *items, uint8_t *counts, int n, int max, int item, int count)
{
    int i;
    if (count <= 0) return n;
    for (i = 0; i < n; i++)
        if (items[i] == item) { counts[i] = (uint8_t)DOOMO_MIN(counts[i] + count, 99); return n; }
    if (n >= max) return n;
    items[n] = (uint8_t)item;
    counts[n] = (uint8_t)DOOMO_MIN(count, 99);
    return n + 1;
}

int weld_cost(const DoomoWeapon *w, uint8_t *items, uint8_t *counts, int max)
{
    const FrameBase *f;
    int n = 0, scrap, gears = 0, steel = 0, horn = 0, glitter = 0, plush = 0, rods = 1, bp = 0;
    if (!w || !items || !counts || max <= 0) return 0;
    f = &frames[w->frame < WFRAME_COUNT ? w->frame : WFRAME_FISTS];
    switch (f->wclass) {
    case WCLASS_SIDEARM: scrap = 2; gears = 1; break;
    case WCLASS_AUTO: scrap = 3; gears = 2; break;
    case WCLASS_PRECISION: scrap = 3; gears = 2; break;
    case WCLASS_SHOTGUN: scrap = 4; gears = 1; break;
    case WCLASS_HEAVY: scrap = 5; gears = 3; steel = 1; rods = 2; break;
    case WCLASS_MELEE: scrap = 2; break;
    default: scrap = 1; glitter = 1; plush = 1; break;
    }
    if (w->frame == WFRAME_BFG) { steel += 2; horn += 2; bp = 1; }
    if (w->frame == WFRAME_RAILGUN || w->frame == WFRAME_MINIGUN) { steel += 1; bp = 1; }
    if (w->frame == WFRAME_PLUSH_LAUNCHER) plush += 2;
    if (w->barrel == WBARREL_DOUBLE || w->barrel == WBARREL_HEAVY) scrap += 1;
    if (w->barrel == WBARREL_TRIPLE) { scrap += 2; gears += 1; }
    if (w->barrel == WBARREL_HEART || w->barrel == WBARREL_STAR) glitter += 1;
    switch (w->core) {
    case WCORE_HELLFIRE: horn += 1; break;
    case WCORE_PLASMA: case WCORE_RAIL_SLUG: steel += 1; gears += 1; break;
    case WCORE_ROCKET: case WCORE_GRENADE: case WCORE_EXPLOSIVE_ROUND: gears += 1; break;
    case WCORE_GLITTER: case WCORE_CANDY: case WCORE_LOVE: glitter += 1; break;
    case WCORE_PLUSH: plush += 1; break;
    case WCORE_HOLY_WATER: glitter += 1; break;
    case WCORE_SAWBLADE: scrap += 2; break;
    default: break;
    }
    if (w->mod != WMOD_NONE) gears += 1;
    if (w->mod == WMOD_LIFESTEAL || w->mod == WMOD_OVERCLOCK) horn += 1;
    if (w->sight == WSIGHT_SCOPE_8X || w->sight == WSIGHT_THERMAL) gears += 1;
    if (w->stock == WSTOCK_PLUSH) plush += 1;
    if (w->stock == WSTOCK_HEAVY) scrap += 1;
    if (w->mag == WMAG_DRUM || w->mag == WMAG_BELT) scrap += 1;
    if (w->under != WUNDER_NONE && w->under != WUNDER_GRIP) gears += 1;
    if (w->charm == WCHARM_TEDDY) plush += 1;
    if (w->charm == WCHARM_SKULL) horn += 1;
    if (w->style == WSTYLE_BRUTAL) steel += 1; else glitter += 1;
    n = add_item(items, counts, n, max, ITEM_WELD_ROD, rods);
    n = add_item(items, counts, n, max, ITEM_SCRAP, scrap);
    n = add_item(items, counts, n, max, ITEM_GEARS, gears);
    n = add_item(items, counts, n, max, ITEM_HELLSTEEL, steel);
    n = add_item(items, counts, n, max, ITEM_DEMON_HORN, horn);
    n = add_item(items, counts, n, max, ITEM_GLITTER, glitter);
    n = add_item(items, counts, n, max, ITEM_PLUSH_STUFFING, plush);
    n = add_item(items, counts, n, max, ITEM_BLUEPRINT, bp);
    return n;
}
