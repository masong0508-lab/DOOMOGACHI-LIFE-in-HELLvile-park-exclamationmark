/* sprites.h - procedural sprites for demons, pickups, projectiles, props
 * and effects (owner: art). Generated at build time by artgen into
 * core/gen/art_data.c like the textures (const, ROM on GBA).
 * Cute vs brutal variants for every death/gore effect: GORE_CUTE turns
 * gibs into confetti, hearts and plush stuffing; GORE_BRUTAL is full Doom.
 */
#ifndef DOOMO_SPRITES_H
#define DOOMO_SPRITES_H

#include "types.h"

enum {
    SPR_NONE = 0,
    /* demons (frames: 0-1 walk, 2 attack, 3 pain, 4-7 death) */
    SPR_IMPLING, SPR_VAPE_WRAITH, SPR_PUFF_DEMON, SPR_HELLHOUND_PUP,
    SPR_BONE_JESTER, SPR_SLIME_BELCHER, SPR_GARGOYLE, SPR_CYBER_PIGGY,
    SPR_BRUTE, SPR_CLOUD_SPIDER, SPR_LORD_VAPULA,
    /* projectiles & effects */
    SPR_FIREBALL, SPR_VAPE_BOLT, SPR_BUBBLE, SPR_GLITTER_SHOT, SPR_PLASMA,
    SPR_ROCKET, SPR_HOLY_SPLASH, SPR_EXPLOSION, SPR_PUFF, SPR_BLOOD_SPRAY,
    SPR_GIBS, SPR_CONFETTI, SPR_HEARTS, SPR_STUFFING, SPR_SPARKLE,
    /* pickups */
    SPR_HEALTH_SMALL, SPR_HEALTH_BIG, SPR_ARMOR, SPR_AMMO_BULLETS,
    SPR_AMMO_SHELLS, SPR_AMMO_CELLS, SPR_AMMO_ROCKETS, SPR_KEY_RED,
    SPR_KEY_BLUE, SPR_SCRAP, SPR_SOUL_ORB, SPR_CANDY,
    /* props */
    SPR_TREE, SPR_FLOWER_BED, SPR_LAMP, SPR_BENCH, SPR_FOUNTAIN, SPR_BARREL,
    SPR_SKULL_PILE, SPR_CORPSE_CUTE, SPR_CORPSE_BRUTAL, SPR_TORCH,
    SPR_SIGN, SPR_MAILBOX, SPR_VAPE_PORTAL, SPR_EGG, SPR_PET,
    SPR_COUNT
};

#define SPR_MAX_FRAMES 8
/* frame is clamped to the sprite's frame count. */
const DoomoImage *sprite_image(int spr_id, int frame);
int  sprite_frames(int spr_id);
void sprites_init(void);

#endif
