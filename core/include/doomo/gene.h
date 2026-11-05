/* gene.h - the 384-bit citizen gene (customisation v2, P0.M0a).
 * docs/design/customization/SPEC.md sections 1.2 and 1.3.
 *
 * The gene packs every look and identity field of a citizen LSB-first into
 * 48 bytes. This file is the codec and the v1 bridge only: nothing in the
 * game stores or reads a gene yet (DoomoResident is still the 96-byte v1
 * record), so adding it changes no behaviour, save byte or golden. The
 * repack that makes the gene the stored form is the rest of M0a.
 *
 * Reads and writes are BYTE-WISE (CF-2): a record can sit at any offset, and
 * a 32-bit load from a byte array is wrong on the ARM7TDMI and traps on the
 * R3000.
 */
#ifndef DOOMO_GENE_H
#define DOOMO_GENE_H

#include "types.h"
#include "catalog.h"
#include "resident.h"

#define DOOMO_GENE_BYTES 48
#define DOOMO_GENE_BITS  (DOOMO_GENE_BYTES * 8)

/* One id per field, in bit order. The legacy look fields keep the order of
 * DoomoLook (minus `species`, the hot byte), so GF_BODY + i is look byte 1 + i. */
enum {
    GF_BODY, GF_HEIGHT, GF_BUILD, GF_SKIN, GF_HEAD_SHAPE, GF_HEAD_SIZE,
    GF_EYE_STYLE, GF_EYE_COLOR, GF_EYE_SIZE, GF_EYE_SPACING, GF_EYE_HEIGHT, GF_EYE_TILT,
    GF_EYE_COUNT, GF_BROW_STYLE, GF_BROW_HEIGHT, GF_NOSE_STYLE, GF_NOSE_SIZE,
    GF_MOUTH_STYLE, GF_MOUTH_SIZE, GF_FANGS, GF_HAIR_STYLE, GF_HAIR_COLOR,
    GF_HAIR_HIGHLIGHT, GF_FACIAL_HAIR, GF_EARS, GF_HORNS, GF_HORN_COLOR, GF_HORN_SIZE,
    GF_TAIL, GF_WINGS, GF_HALO, GF_MARKING, GF_MARKING_COLOR, GF_CHEEKS, GF_TOP,
    GF_TOP_COLOR, GF_BOTTOM, GF_BOTTOM_COLOR, GF_SHOES, GF_SHOES_COLOR, GF_HAT,
    GF_HAT_COLOR, GF_ACCESSORY, GF_ACCESSORY_COLOR, GF_AURA, GF_PATTERN, GF_PATTERN_COLOR,
    GF_VOICE0,                                   /* VOICE_COUNT (5) consecutive ids */
    GF_PERSONA0 = GF_VOICE0 + 5,                 /* PERS_COUNT  (5) consecutive ids */
    GF_PRONOUN = GF_PERSONA0 + 5,
    GF_ROMANCE, GF_BDAY_MONTH, GF_BDAY_DAY, GF_LIKE_FOOD, GF_LIKE_HOBBY, GF_LIKE_MUSIC,
    GF_LIKE_COLOR, GF_HATE_FOOD, GF_FEAR, GF_CATCHPHRASE,
    /* new core fields (P0 appends them; the painters come later) */
    GF_LOOK_SEED, GF_AUTO_STYLE, GF_AGE, GF_PUPIL, GF_SCLERA, GF_FUR_PATTERN,
    GF_FUR_COLOR3, GF_GLASSES, GF_OUTER, GF_LEGWEAR, GF_BLUSH_STYLE, GF_EAR_FOLD,
    GF_HOOD_UP, GF_HANDEDNESS,
    GF_SPARE,                                    /* 3 bits, must stay 0 */
    GF_COUNT
};
DOOMO_STATIC_ASSERT(GF_COUNT == 83, gene_has_83_fields);
DOOMO_STATIC_ASSERT(VOICE_COUNT == 5 && PERS_COUNT == 5, gene_voice_persona_counts);
DOOMO_STATIC_ASSERT(GF_PATTERN_COLOR + 1 == GF_VOICE0, gene_look_then_voice);

typedef struct DoomoGeneField { uint16_t pos, max; uint8_t bits; } DoomoGeneField;
extern const DoomoGeneField doomo_gene_field[GF_COUNT];

enum { AUTO_CLASSIC, AUTO_VARIED, AUTO_AUTHORED };

/* AGE: the gene's `age` field IS the life stage (resident.h LIFE_*), with the
 * same numbers, so the gene and the record can never disagree and an old
 * record (stage 0) is an adult exactly as it is today. This settles the
 * spec's AGE_AUTO / AGE_KID enum (SPEC 1.2), which predates the explicit
 * stages: there is no AUTO (the roster and the sim now set a stage on every
 * citizen) and no single KID (TODDLER and CHILD are separate stages). The
 * 3-bit field holds 0..7; LIFE_COUNT is 6, so 6 and 7 are invalid and read as
 * ADULT (like resident_sanitize). */
#define AGE_ADULT   LIFE_ADULT
#define AGE_BABY    LIFE_BABY
#define AGE_TODDLER LIFE_TODDLER
#define AGE_CHILD   LIFE_CHILD
#define AGE_TEEN    LIFE_TEEN
#define AGE_ELDER   LIFE_ELDER
#define AGE_COUNT   LIFE_COUNT
DOOMO_STATIC_ASSERT(AGE_COUNT <= 8, age_fits_its_3_bits);

uint32_t gene_get(const uint8_t *g, int gf);
void     gene_set(uint8_t *g, int gf, uint32_t v);        /* clamps to the field's max */

/* v1 bridge. gene_pack_v1 writes every field a v1 DoomoResident holds that
 * the gene has: the look (not `species`), voice, persona and the identity
 * members; hair_highlight's "none" (0xFF) is stored as 32 (CF-3); `romance`
 * is the preference half only, and `age` is the life stage (AGE_* above). The
 * other new core fields are written as 0. Returns 1 when every value fit its
 * width, 0 when something was clamped.
 * gene_unpack_v1 is the inverse: it overwrites the same members of `r` and
 * leaves everything else (species, name, stats, status, flags, exorcism,
 * level, mood, activity, death_cause) alone. */
int  gene_pack_v1(const DoomoResident *r, uint8_t g[DOOMO_GENE_BYTES]);
void gene_unpack_v1(const uint8_t g[DOOMO_GENE_BYTES], DoomoResident *r);

#endif
