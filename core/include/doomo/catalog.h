/* catalog.h - every customisation option in the game.
 *
 * The enum values are stored in saves: append new entries just before the
 * *_COUNT terminator, never reorder. Each list has a matching name table
 * (catalog.c) used by the creator UI: doomo_<list>_names[COUNT].
 * The portrait renderer (portrait.c) must draw every entry of every list.
 */
#ifndef DOOMO_CATALOG_H
#define DOOMO_CATALOG_H

#include "types.h"

/* ---- species ----------------------------------------------------------- */
enum {
    SPECIES_HUMAN, SPECIES_IMP, SPECIES_CHERUB, SPECIES_GHOUL,
    SPECIES_SKELETON, SPECIES_SLIME, SPECIES_HELLHOUND, SPECIES_CACOBLOB,
    SPECIES_BATLING, SPECIES_ROBOT, SPECIES_PLUSHIE, SPECIES_GHOST,
    SPECIES_MUMMY, SPECIES_MOTHKIN, SPECIES_FROGLING, SPECIES_BUNNYKIN,
    /* fluffy critter-folk (the vaguely furry heart of the cast) */
    SPECIES_CATFOLK, SPECIES_FOXKIN, SPECIES_BEARKIN, SPECIES_PUPKIN,
    SPECIES_MOUSEKIN, SPECIES_SHEEPKIN, SPECIES_RACCOONKIN, SPECIES_OWLKIN,
    SPECIES_COUNT
};

/* ---- body & head ------------------------------------------------------- */
enum { BODY_AVERAGE, BODY_ROUND, BODY_LANKY, BODY_STOCKY, BODY_TINY,
       BODY_HUNCHED, BODY_BUFF, BODY_NOODLE, BODY_PEAR, BODY_TOP_HEAVY,
       BODY_COUNT };

enum { HEAD_ROUND, HEAD_OVAL, HEAD_SQUARE, HEAD_HEART, HEAD_TRIANGLE,
       HEAD_PEAR, HEAD_BEAN, HEAD_SKULL, HEAD_STAR, HEAD_BLOB, HEAD_DIAMOND,
       HEAD_MOCHI, HEAD_COUNT };

/* ---- face -------------------------------------------------------------- */
enum { EYE_DOT, EYE_ROUND, EYE_ANIME, EYE_SLEEPY, EYE_ANGRY, EYE_HAPPY_ARC,
       EYE_CAT, EYE_GOAT, EYE_SPIRAL, EYE_STAR, EYE_HEART, EYE_X,
       EYE_BUTTON, EYE_VOID, EYE_LASHES, EYE_DROOPY, EYE_WIDE, EYE_SQUINT,
       EYE_SPARKLE, EYE_DEMON_SLIT, EYE_CROSS, EYE_HOLLOW, EYE_GLOW,
       EYE_COMPOUND, EYE_COUNT };

enum { BROW_NONE, BROW_THIN, BROW_THICK, BROW_ANGRY, BROW_WORRIED,
       BROW_UNIBROW, BROW_ARCHED, BROW_BUSHY, BROW_DOTS, BROW_SCARRED,
       BROW_FLAT, BROW_RAISED, BROW_COUNT };

enum { NOSE_NONE, NOSE_DOT, NOSE_BUTTON, NOSE_POINTY, NOSE_ROUND, NOSE_PIG,
       NOSE_SKULL, NOSE_HOOK, NOSE_WIDE, NOSE_LONG, NOSE_CAT, NOSE_CLOWN,
       NOSE_COUNT };

enum { MOUTH_SMILE, MOUTH_GRIN, MOUTH_FLAT, MOUTH_FROWN, MOUTH_OPEN,
       MOUTH_CAT, MOUTH_TEETH, MOUTH_SHARK, MOUTH_TONGUE, MOUTH_SMIRK,
       MOUTH_POUT, MOUTH_O, MOUTH_WAVY, MOUTH_STITCHED, MOUTH_BEAK,
       MOUTH_FANGY, MOUTH_DROOL, MOUTH_LIPSTICK, MOUTH_BIG_GRIN, MOUTH_VOID,
       MOUTH_COUNT };

enum { EARS_NORMAL, EARS_NONE, EARS_POINTY, EARS_ELF, EARS_CAT, EARS_DOG,
       EARS_BAT, EARS_FIN, EARS_BUNNY,
       EARS_FOX, EARS_BEAR, EARS_MOUSE, EARS_SHEEP, EARS_FLOPPY_PUP,
       EARS_OWL_TUFTS, EARS_RACCOON, EARS_COUNT };

/* bit flags in DoomoLook.cheeks */
enum { CHEEK_BLUSH = 1, CHEEK_FRECKLES = 2, CHEEK_SCAR = 4,
       CHEEK_BANDAGE = 8, CHEEK_BEAUTY_MARK = 16, CHEEK_TEAR_TATTOO = 32,
       CHEEK_DIMPLES = 64, CHEEK_SPARKLES = 128 };

/* ---- hair -------------------------------------------------------------- */
enum { HAIR_BALD, HAIR_BUZZ, HAIR_SHORT, HAIR_SPIKY, HAIR_MOHAWK, HAIR_BOB,
       HAIR_LONG, HAIR_PONYTAIL, HAIR_TWINTAILS, HAIR_BUN, HAIR_AFRO,
       HAIR_CURLY, HAIR_MULLET, HAIR_EMO_FRINGE, HAIR_POMPADOUR,
       HAIR_BRAIDS, HAIR_SIDE_SHAVE, HAIR_WILD, HAIR_LOCS, HAIR_PIGTAILS,
       HAIR_HIME, HAIR_WAVY, HAIR_BOWL, HAIR_LIBERTY_SPIKES, HAIR_SLICKED,
       HAIR_MESSY, HAIR_ODANGO, HAIR_DRILLS, HAIR_FLAME, HAIR_TENTACLES,
       HAIR_SLIME_DRIP, HAIR_CROWN_BRAID, HAIR_COUNT };

enum { FHAIR_NONE, FHAIR_STUBBLE, FHAIR_MUSTACHE, FHAIR_GOATEE,
       FHAIR_FULL_BEARD, FHAIR_HANDLEBAR, FHAIR_CHOPS, FHAIR_WIZARD,
       FHAIR_COUNT };

/* ---- supernatural bits -------------------------------------------------- */
enum { HORNS_NONE, HORNS_NUBS, HORNS_RAM, HORNS_BULL, HORNS_DEVIL,
       HORNS_UNICORN, HORNS_ANTLERS, HORNS_GOAT, HORNS_ONI, HORNS_SPIRAL,
       HORNS_BROKEN, HORNS_CROWN_SPIKES, HORNS_ANTENNAE, HORNS_DEVIL_TALL,
       HORNS_MANY, HORNS_HALO_HORNS, HORNS_COUNT };

enum { TAIL_NONE, TAIL_DEVIL, TAIL_CAT, TAIL_FOX, TAIL_LIZARD, TAIL_BUNNY,
       TAIL_SPIKED, TAIL_TENTACLE, TAIL_SKELETAL, TAIL_FLAME,
       TAIL_BEAR_NUB, TAIL_PUP_WAGGY, TAIL_MOUSE, TAIL_SHEEP_PUFF,
       TAIL_RACCOON_RINGED, TAIL_FEATHER_FAN, TAIL_COUNT };

enum { WINGS_NONE, WINGS_BAT, WINGS_ANGEL, WINGS_BUTTERFLY, WINGS_BONE,
       WINGS_FLY, WINGS_DRAGON, WINGS_TINY_BAT, WINGS_MECH, WINGS_FLAME,
       WINGS_COUNT };

enum { HALO_NONE, HALO_HALO, HALO_BROKEN, HALO_FLAME_RING, HALO_STARS,
       HALO_THORNS, HALO_COUNT };

enum { MARK_NONE, MARK_STRIPES, MARK_SPOTS, MARK_TRIBAL, MARK_SKULL_PAINT,
       MARK_CHEEK_HEARTS, MARK_STITCHES, MARK_CRACKS, MARK_STAR_FRECKLES,
       MARK_CLOWN, MARK_EYELINER, MARK_THIRD_EYE, MARK_PENTAGRAM, MARK_TEARS,
       MARK_PATCHES, MARK_SCALES,
       /* fur patterns for critter-folk (marking_color = the 2nd fur colour) */
       MARK_FUR_MUZZLE, MARK_FUR_BELLY, MARK_FUR_SOCKS, MARK_FUR_MASK,
       MARK_FUR_TABBY, MARK_FUR_CALICO, MARK_FUR_TIPPED_EARS,
       MARK_COUNT };

/* ---- outfit ------------------------------------------------------------ */
enum { TOP_NONE, TOP_TSHIRT, TOP_HOODIE, TOP_TANK, TOP_SWEATER, TOP_SUIT,
       TOP_DRESS, TOP_ROBE, TOP_ARMOR, TOP_LEATHER_JACKET, TOP_OVERALLS,
       TOP_KIMONO, TOP_LAB_COAT, TOP_CROP_TOP, TOP_SWEATER_VEST, TOP_CLOAK,
       TOP_BAND_TEE, TOP_PRIEST, TOP_NURSE, TOP_MAID, TOP_SPACESUIT,
       TOP_PLATE_ARMOR, TOP_HAZMAT, TOP_PAJAMAS, TOP_COUNT };

enum { BOTTOM_NONE, BOTTOM_PANTS, BOTTOM_SHORTS, BOTTOM_SKIRT,
       BOTTOM_LONG_SKIRT, BOTTOM_RIPPED_JEANS, BOTTOM_CARGO,
       BOTTOM_LEGGINGS, BOTTOM_KILT, BOTTOM_TUTU, BOTTOM_SWEATS,
       BOTTOM_ROBE, BOTTOM_COUNT };

enum { SHOES_BARE, SHOES_SNEAKERS, SHOES_BOOTS, SHOES_SANDALS, SHOES_HEELS,
       SHOES_CLOGS, SHOES_COMBAT, SHOES_SLIPPERS, SHOES_HOOVES,
       SHOES_ROLLERSKATES, SHOES_PLATFORM, SHOES_CLAWS, SHOES_COUNT };

enum { HAT_NONE, HAT_CAP, HAT_BEANIE, HAT_TOP_HAT, HAT_WITCH, HAT_CROWN,
       HAT_PROPELLER, HAT_COWBOY, HAT_BERET, HAT_MARINE_HELMET,
       HAT_HORNED_HELMET, HAT_BUNNY_EARS, HAT_CAT_EARS, HAT_HEADPHONES,
       HAT_BANDANA, HAT_CHEF, HAT_PARTY, HAT_NUN_VEIL, HAT_FEZ,
       HAT_FLOWER_CROWN, HAT_TIARA, HAT_SKULL_CAP, HAT_TRAFFIC_CONE,
       HAT_BUCKET, HAT_COUNT };

enum { ACC_NONE, ACC_GLASSES, ACC_SUNGLASSES, ACC_MONOCLE, ACC_EYEPATCH,
       ACC_MASK, ACC_BOWTIE, ACC_CROSS_NECKLACE, ACC_SKULL_NECKLACE,
       ACC_SCARF, ACC_BANDAGES, ACC_EARRINGS, ACC_NOSE_RING, ACC_PACIFIER,
       ACC_GAS_MASK, ACC_CAPE, ACC_BACKPACK, ACC_ROSARY, ACC_CHAINS,
       ACC_TEDDY, ACC_COUNT };

enum { AURA_NONE, AURA_SPARKLES, AURA_FLAMES, AURA_FLIES, AURA_HEARTS,
       AURA_SMOKE, AURA_BUBBLES, AURA_GLITCH, AURA_COUNT };

enum { PATTERN_SOLID, PATTERN_STRIPES, PATTERN_POLKA, PATTERN_PLAID,
       PATTERN_FLAMES, PATTERN_SKULLS, PATTERN_HEARTS, PATTERN_CAMO,
       PATTERN_COUNT };

/* ---- identity & personality -------------------------------------------- */
enum { PRONOUN_THEY, PRONOUN_SHE, PRONOUN_HE, PRONOUN_IT, PRONOUN_XE,
       PRONOUN_COUNT };
/* Who this resident can fall for (by pronoun group); ANY is default. */
enum { ROMANCE_ANY, ROMANCE_SHE, ROMANCE_HE, ROMANCE_THEY, ROMANCE_NONE,
       ROMANCE_COUNT };

/* Five personality sliders, each 0..15 (Tomodachi-style). */
enum { PERS_MOVEMENT,     /* slow .. quick        */
       PERS_SPEECH,       /* polite .. blunt      */
       PERS_EXPRESSION,   /* flat .. expressive   */
       PERS_ATTITUDE,     /* serious .. easygoing */
       PERS_QUIRK,        /* normal .. unhinged   */
       PERS_COUNT };

/* Voice parameters, each 0..31. */
enum { VOICE_PITCH, VOICE_SPEED, VOICE_QUALITY, VOICE_INTONATION,
       VOICE_ACCENT, VOICE_COUNT };

/* 16 derived personality types (persona_type()). */
#define PERSONA_TYPE_COUNT 16

/* ---- things residents like ---------------------------------------------- */
#define FOOD_COUNT       48
#define HOBBY_COUNT      32
#define MUSIC_COUNT      16
#define FEAR_COUNT       24
#define CATCHPHRASE_COUNT 64
#define FIRST_NAME_COUNT 256
#define LAST_NAME_COUNT  128

/* ---- name tables (catalog.c) ------------------------------------------- */
extern const char *const doomo_species_names[SPECIES_COUNT];
extern const char *const doomo_body_names[BODY_COUNT];
extern const char *const doomo_head_names[HEAD_COUNT];
extern const char *const doomo_eye_names[EYE_COUNT];
extern const char *const doomo_brow_names[BROW_COUNT];
extern const char *const doomo_nose_names[NOSE_COUNT];
extern const char *const doomo_mouth_names[MOUTH_COUNT];
extern const char *const doomo_ear_names[EARS_COUNT];
extern const char *const doomo_cheek_names[8];
extern const char *const doomo_hair_names[HAIR_COUNT];
extern const char *const doomo_fhair_names[FHAIR_COUNT];
extern const char *const doomo_horn_names[HORNS_COUNT];
extern const char *const doomo_tail_names[TAIL_COUNT];
extern const char *const doomo_wing_names[WINGS_COUNT];
extern const char *const doomo_halo_names[HALO_COUNT];
extern const char *const doomo_mark_names[MARK_COUNT];
extern const char *const doomo_top_names[TOP_COUNT];
extern const char *const doomo_bottom_names[BOTTOM_COUNT];
extern const char *const doomo_shoes_names[SHOES_COUNT];
extern const char *const doomo_hat_names[HAT_COUNT];
extern const char *const doomo_acc_names[ACC_COUNT];
extern const char *const doomo_aura_names[AURA_COUNT];
extern const char *const doomo_pattern_names[PATTERN_COUNT];
extern const char *const doomo_pronoun_names[PRONOUN_COUNT];   /* "they/them" */
extern const char *const doomo_romance_names[ROMANCE_COUNT];
extern const char *const doomo_persona_axis_names[PERS_COUNT];
extern const char *const doomo_persona_type_names[PERSONA_TYPE_COUNT];
extern const char *const doomo_voice_names[VOICE_COUNT];
extern const char *const doomo_food_names[FOOD_COUNT];
extern const char *const doomo_hobby_names[HOBBY_COUNT];
extern const char *const doomo_music_names[MUSIC_COUNT];
extern const char *const doomo_fear_names[FEAR_COUNT];
extern const char *const doomo_catchphrases[CATCHPHRASE_COUNT];
extern const char *const doomo_first_names[FIRST_NAME_COUNT];
extern const char *const doomo_last_names[LAST_NAME_COUNT];
extern const char *const doomo_month_names[12];

/* Pronoun helpers: subject/object/possessive, e.g. "they","them","their". */
const char *pronoun_subj(int p);
const char *pronoun_obj(int p);
const char *pronoun_poss(int p);

/* Species defaults the generator and creator lean on (e.g. skeletons get
 * no hair by default, slimes get PAINT_SLIME skin, ghosts float). */
typedef struct DoomoSpeciesInfo {
    uint8_t default_skin;       /* look.skin value                    */
    uint8_t hair_chance;        /* % generated residents have hair    */
    uint8_t horn_chance, tail_chance, wing_chance;
    uint8_t base_corruption;    /* starting corruption 0..100         */
    uint8_t holy_affinity;      /* resistance to possession 0..100    */
    uint8_t flags;              /* SPF_* */
} DoomoSpeciesInfo;
enum { SPF_FLOATS = 1, SPF_NO_SKIN_SHADE = 2, SPF_UNDEAD = 4, SPF_SQUISHY = 8,
       SPF_MECHANICAL = 16, SPF_HOLY = 32 };
extern const DoomoSpeciesInfo doomo_species_info[SPECIES_COUNT];

#endif
