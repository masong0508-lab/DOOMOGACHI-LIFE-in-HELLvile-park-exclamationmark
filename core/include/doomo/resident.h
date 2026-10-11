/* resident.h - one inhabitant of Hellvile Park.
 *
 * DoomoResident is exactly 96 bytes, all uint8_t / char so it has no
 * padding or endianness issues and is written to saves verbatim.
 * Slider ranges: 0..15 unless noted. Colour fields: values 0..31 select a
 * paint colour (PAL_PAINT); for `skin` values 0..7 are natural skin tones
 * (PAL_SKIN) and 8..39 are paint colours (value-8).
 */
#ifndef DOOMO_RESIDENT_H
#define DOOMO_RESIDENT_H

#include "types.h"
#include "catalog.h"

#define LOOK_NO_COLOR 0xFF  /* e.g. hair_highlight = none */

typedef struct DoomoLook {
    uint8_t species;        /* SPECIES_*                        */
    uint8_t body;           /* BODY_*                           */
    uint8_t height;         /* 0..15                            */
    uint8_t build;          /* 0..15 skinny..chonky             */
    uint8_t skin;           /* 0..7 natural, 8..39 paint        */
    uint8_t head_shape;     /* HEAD_*                           */
    uint8_t head_size;      /* 0..15                            */
    uint8_t eye_style;      /* EYE_*                            */
    uint8_t eye_color;      /* paint                            */
    uint8_t eye_size;       /* 0..15                            */
    uint8_t eye_spacing;    /* 0..15                            */
    uint8_t eye_height;     /* 0..15                            */
    uint8_t eye_tilt;       /* 0..15, 8 = level                 */
    uint8_t eye_count;      /* 1..3 (yes, three)                */
    uint8_t brow_style;     /* BROW_*                           */
    uint8_t brow_height;    /* 0..15                            */
    uint8_t nose_style;     /* NOSE_*                           */
    uint8_t nose_size;      /* 0..15                            */
    uint8_t mouth_style;    /* MOUTH_*                          */
    uint8_t mouth_size;     /* 0..15                            */
    uint8_t fangs;          /* 0..3                             */
    uint8_t hair_style;     /* HAIR_*                           */
    uint8_t hair_color;     /* paint                            */
    uint8_t hair_highlight; /* paint or LOOK_NO_COLOR           */
    uint8_t facial_hair;    /* FHAIR_*                          */
    uint8_t ears;           /* EARS_*                           */
    uint8_t horns;          /* HORNS_*                          */
    uint8_t horn_color;     /* paint                            */
    uint8_t horn_size;      /* 0..15                            */
    uint8_t tail;           /* TAIL_*                           */
    uint8_t wings;          /* WINGS_*                          */
    uint8_t halo;           /* HALO_*                           */
    uint8_t marking;        /* MARK_*                           */
    uint8_t marking_color;  /* paint                            */
    uint8_t cheeks;         /* CHEEK_* bit flags                */
    uint8_t top;            /* TOP_*                            */
    uint8_t top_color;      /* paint                            */
    uint8_t bottom;         /* BOTTOM_*                         */
    uint8_t bottom_color;   /* paint                            */
    uint8_t shoes;          /* SHOES_*                          */
    uint8_t shoes_color;    /* paint                            */
    uint8_t hat;            /* HAT_*                            */
    uint8_t hat_color;      /* paint                            */
    uint8_t accessory;      /* ACC_*                            */
    uint8_t accessory_color;/* paint                            */
    uint8_t aura;           /* AURA_*                           */
    uint8_t pattern;        /* PATTERN_* (applied to top)       */
    uint8_t pattern_color;  /* paint                            */
} DoomoLook;                /* 48 bytes */

/* Resident status. */
enum { RES_EMPTY = 0,       /* unused slot                         */
       RES_ALIVE,
       RES_POSSESSED,       /* a demon moved in; needs exorcism    */
       RES_SOUL_LOST,       /* 3+ failed exorcisms: may be laid to rest */
       RES_DEAD,            /* in the graveyard                    */
       RES_GHOST };         /* came back as a ghost (graveyard rite) */

/* Resident flags. */
enum { RF_CUSTOM = 1,       /* made/edited by the player            */
       RF_PLAYER = 2,       /* the story-mode protagonist           */
       RF_FAVORITE = 4,
       RF_STORY_NPC = 8,    /* fixed story character (keep alive)   */
       RF_MONSTER_KILL = 16,/* killed via Monster Mode (shame)      */
       RF_HAS_PET = 32,
       RF_TRAVELER = 64,
       RF_HELLBORN = 128 };  /* a citizen of Hellvile (the "bad" side) by
                                birth, even if redeemed and moved up      */

/* Needs/meters, each 0..100. */
enum { STAT_HUNGER,      /* 100 = stuffed, 0 = starving            */
       STAT_HAPPY,
       STAT_HYGIENE,
       STAT_ENERGY,
       STAT_HEALTH,
       STAT_SANITY,
       STAT_CORRUPTION,  /* 100 = possessed threshold               */
       STAT_SOCIAL,
       STAT_COUNT };

typedef struct DoomoResident {
    DoomoLook look;                 /* 48 */
    char      name[DOOMO_NAME_LEN]; /* 60 */
    uint8_t   voice[VOICE_COUNT];   /* 65  each 0..31 */
    uint8_t   persona[PERS_COUNT];  /* 70  each 0..15 */
    uint8_t   stat[STAT_COUNT];     /* 78  each 0..100 */
    uint8_t   status;               /* 79  RES_* */
    uint8_t   flags;                /* 80  RF_* */
    uint8_t   pronoun;              /* 81  PRONOUN_* */
    uint8_t   romance;              /* 82  ROMANCE_* */
    uint8_t   bday_month;           /* 83  0..11 */
    uint8_t   bday_day;             /* 84  1..28 */
    uint8_t   like_food;            /* 85  0..FOOD_COUNT-1 */
    uint8_t   like_hobby;           /* 86 */
    uint8_t   like_music;           /* 87 */
    uint8_t   like_color;           /* 88  paint */
    uint8_t   hate_food;            /* 89 */
    uint8_t   fear;                 /* 90 */
    uint8_t   catchphrase;          /* 91 */
    uint8_t   exorcism;             /* 92  hi nibble attempts, lo nibble failures */
    uint8_t   level;                /* 93  friendship-with-player level 0..99 */
    uint8_t   mood;                 /* 94  MOOD_* (sim.h) */
    uint8_t   activity;             /* 95  ACT_*  (sim.h) */
    uint8_t   death_cause;          /* 96  DEATH_* (sim.h) */
} DoomoResident;

DOOMO_STATIC_ASSERT(sizeof(DoomoLook) == 48, look_is_48_bytes);
DOOMO_STATIC_ASSERT(sizeof(DoomoResident) == 96, resident_is_96_bytes);

/* Hellvile Park sits at the heart of HaHaland (the good place) and is split
 * in two: Hillvile, the good side, and Hellvile, the bad side (built on the
 * sealed hell gate). District of a slot id: 0 = Hillvile, 1 = Hellvile. */
enum { DISTRICT_HILLVILE = 0, DISTRICT_HELLVILE = 1 };
#define RES_DISTRICT(id) ((id) >= DOOMO_DISTRICT_SIZE)

/* Deterministically generate a complete, valid, cute-by-default random
 * resident from a seed (same result on every platform). Status ALIVE,
 * flags 0, stats healthy-ish, corruption from species info. */
void resident_generate(DoomoResident *r, uint32_t seed);
/* Same, biased toward a Hellvile citizen (demonic species/outfits,
 * edgier personalities, RF_HELLBORN set). */
void resident_generate_hellborn(DoomoResident *r, uint32_t seed);
/* Randomise only the look (keeps name/personality). */
void resident_randomize_look(DoomoResident *r, uint32_t seed);
/* Clamp every field into its valid range (used after loading saves or
 * editing in the creator). Returns number of fields that were fixed. */
int  resident_sanitize(DoomoResident *r);
/* A sensible blank template for the creator. */
void resident_default(DoomoResident *r);

/* 0..15 personality type derived from the sliders. */
int  persona_type(const DoomoResident *r);
/* Map a look colour value to a palette index (shade 0..2). */
uint8_t look_paint(uint8_t paint, int shade);
uint8_t look_skin(const DoomoLook *l, int shade);

/* Packed exorcism counters. */
#define RES_EXO_ATTEMPTS(r) ((r)->exorcism >> 4)
#define RES_EXO_FAILS(r)    ((r)->exorcism & 15)
#define DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL 3

/* FNV-1a over a resident, for parity tests (JS-free: GBA/PS1/native must
 * all produce the same value for resident_generate(seed)). */
uint32_t resident_fingerprint(const DoomoResident *r);

#endif
