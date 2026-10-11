/* resident.c - generating, validating and describing residents
 * (owner: foundation).
 *
 * resident_generate() builds a complete, coherent, cute-by-default
 * citizen from a seed using only a local DoomoRng, so the same seed gives
 * the same citizen on GBA, PS1 and native (resident_fingerprint parity).
 * Most Hillvile folk are fluffy critter-folk with a matching fur palette
 * and a harmonious pastel outfit scheme; Hellvile folk lean demonic and
 * goth but are still drawn cute.
 */
#include "doomo/resident.h"
#include "doomo/palette.h"
#include "doomo/rng.h"
#include "doomo/sim.h"

/* ---------------------------------------------------------------------- */
/* weighted picks                                                          */
typedef struct { uint8_t v, w; } Pick;

static int pick(DoomoRng *r, const Pick *p, int n)
{
    int i, total = 0, x;
    for (i = 0; i < n; i++) total += p[i].w;
    x = rng_range(r, total);
    for (i = 0; i < n; i++) {
        if (x < p[i].w) return p[i].v;
        x -= p[i].w;
    }
    return p[n - 1].v;
}
#define PICK(r, tbl) pick((r), (tbl), DOOMO_COUNTOF(tbl))
#define ONE_OF(r, arr) ((arr)[rng_range((r), DOOMO_COUNTOF(arr))])

/* ---------------------------------------------------------------------- */
/* species weights                                                          */
static const Pick sp_cute[] = {
    { SPECIES_CATFOLK, 12 }, { SPECIES_BUNNYKIN, 10 }, { SPECIES_PUPKIN, 10 },
    { SPECIES_FOXKIN, 9 }, { SPECIES_BEARKIN, 9 }, { SPECIES_MOUSEKIN, 8 },
    { SPECIES_SHEEPKIN, 7 }, { SPECIES_RACCOONKIN, 6 }, { SPECIES_OWLKIN, 6 },
    { SPECIES_PLUSHIE, 6 }, { SPECIES_HUMAN, 5 }, { SPECIES_CHERUB, 4 },
    { SPECIES_FROGLING, 4 }, { SPECIES_MOTHKIN, 3 }, { SPECIES_ROBOT, 3 },
    { SPECIES_GHOST, 2 }, { SPECIES_SLIME, 2 }, { SPECIES_BATLING, 1 },
    { SPECIES_IMP, 1 }, { SPECIES_GHOUL, 1 }
};
static const Pick sp_hell[] = {
    { SPECIES_IMP, 12 }, { SPECIES_GHOUL, 9 }, { SPECIES_SKELETON, 9 },
    { SPECIES_HELLHOUND, 9 }, { SPECIES_BATLING, 7 }, { SPECIES_CACOBLOB, 6 },
    { SPECIES_RACCOONKIN, 6 }, { SPECIES_SLIME, 5 }, { SPECIES_MUMMY, 5 },
    { SPECIES_GHOST, 5 }, { SPECIES_CATFOLK, 5 }, { SPECIES_FOXKIN, 5 },
    { SPECIES_MOTHKIN, 3 }, { SPECIES_ROBOT, 3 }, { SPECIES_HUMAN, 3 },
    { SPECIES_PUPKIN, 2 }, { SPECIES_MOUSEKIN, 2 }, { SPECIES_OWLKIN, 2 },
    { SPECIES_BEARKIN, 1 }, { SPECIES_PLUSHIE, 1 }
};

static int is_critter(int sp)
{
    return sp == SPECIES_BUNNYKIN || (sp >= SPECIES_CATFOLK && sp <= SPECIES_OWLKIN);
}

/* ---------------------------------------------------------------------- */
/* fur / skin palettes: { fur, second fur colour }                          */
typedef struct { uint8_t a, b; } Fur;
static const Fur fur_cat[] = {
    { PAINT_CARAMEL, PAINT_WHITE }, { PAINT_CHOCOLATE, PAINT_CARAMEL },
    { PAINT_CHARCOAL, PAINT_WHITE }, { PAINT_SILVER, PAINT_WHITE },
    { PAINT_HONEY, PAINT_WHITE }, { PAINT_TANGERINE, PAINT_WHITE },
    { PAINT_JET, PAINT_WHITE }, { PAINT_PEACH, PAINT_WHITE },
    { PAINT_LAVENDER, PAINT_WHITE }, { PAINT_BUBBLEGUM, PAINT_WHITE }
};
static const Fur fur_fox[] = {
    { PAINT_TANGERINE, PAINT_WHITE }, { PAINT_HONEY, PAINT_WHITE },
    { PAINT_RUST, PAINT_BONE }, { PAINT_SILVER, PAINT_WHITE },
    { PAINT_WHITE, PAINT_SILVER }, { PAINT_BUBBLEGUM, PAINT_WHITE },
    { PAINT_LAVENDER, PAINT_WHITE }, { PAINT_SKY, PAINT_WHITE }
};
static const Fur fur_bear[] = {
    { PAINT_CHOCOLATE, PAINT_CARAMEL }, { PAINT_CARAMEL, PAINT_BONE },
    { PAINT_HONEY, PAINT_BONE }, { PAINT_WHITE, PAINT_SILVER },
    { PAINT_JET, PAINT_WHITE }, { PAINT_PEACH, PAINT_WHITE },
    { PAINT_LAVENDER, PAINT_WHITE }, { PAINT_MINT, PAINT_WHITE }
};
static const Fur fur_pup[] = {
    { PAINT_HONEY, PAINT_WHITE }, { PAINT_CARAMEL, PAINT_WHITE },
    { PAINT_CHOCOLATE, PAINT_HONEY }, { PAINT_WHITE, PAINT_CARAMEL },
    { PAINT_SILVER, PAINT_WHITE }, { PAINT_JET, PAINT_CARAMEL },
    { PAINT_PEACH, PAINT_WHITE }, { PAINT_SKY, PAINT_WHITE }
};
static const Fur fur_mouse[] = {
    { PAINT_SILVER, PAINT_WHITE }, { PAINT_WHITE, PAINT_BUBBLEGUM },
    { PAINT_CARAMEL, PAINT_WHITE }, { PAINT_CHARCOAL, PAINT_SILVER },
    { PAINT_PEACH, PAINT_WHITE }, { PAINT_LAVENDER, PAINT_WHITE }
};
static const Fur fur_sheep[] = {
    { PAINT_WHITE, PAINT_BONE }, { PAINT_BONE, PAINT_CHOCOLATE },
    { PAINT_CHARCOAL, PAINT_WHITE }, { PAINT_LAVENDER, PAINT_WHITE },
    { PAINT_BUBBLEGUM, PAINT_WHITE }, { PAINT_LEMON, PAINT_WHITE }
};
static const Fur fur_raccoon[] = {
    { PAINT_SILVER, PAINT_CHARCOAL }, { PAINT_CHARCOAL, PAINT_WHITE },
    { PAINT_CARAMEL, PAINT_CHOCOLATE }, { PAINT_LAVENDER, PAINT_GRAPE }
};
static const Fur fur_owl[] = {
    { PAINT_CARAMEL, PAINT_BONE }, { PAINT_CHOCOLATE, PAINT_HONEY },
    { PAINT_WHITE, PAINT_SILVER }, { PAINT_SILVER, PAINT_WHITE },
    { PAINT_HONEY, PAINT_WHITE }, { PAINT_LAVENDER, PAINT_SKY }
};
static const Fur fur_bunny[] = {
    { PAINT_WHITE, PAINT_BUBBLEGUM }, { PAINT_CARAMEL, PAINT_WHITE },
    { PAINT_CHOCOLATE, PAINT_WHITE }, { PAINT_SILVER, PAINT_WHITE },
    { PAINT_PEACH, PAINT_WHITE }, { PAINT_BUBBLEGUM, PAINT_WHITE },
    { PAINT_LAVENDER, PAINT_WHITE }, { PAINT_HONEY, PAINT_WHITE }
};

static const uint8_t skin_imp[] = { PAINT_CHERRY, PAINT_ROSE, PAINT_MAGENTA,
    PAINT_TANGERINE, PAINT_LAVENDER, PAINT_BUBBLEGUM, PAINT_BLOOD,
    PAINT_HELLFIRE, PAINT_GRAPE };
static const uint8_t skin_ghoul[] = { PAINT_MINT, PAINT_ECTOPLASM, PAINT_LIME,
    PAINT_TEAL, PAINT_SILVER, PAINT_LAVENDER };
static const uint8_t skin_skel[] = { PAINT_BONE, PAINT_WHITE, PAINT_SILVER };
static const uint8_t skin_slime[] = { PAINT_SLIME, PAINT_MINT, PAINT_BUBBLEGUM,
    PAINT_CYAN, PAINT_LAVENDER, PAINT_LEMON, PAINT_MAGENTA };
static const uint8_t skin_hound[] = { PAINT_CHARCOAL, PAINT_JET, PAINT_RUST,
    PAINT_BLOOD, PAINT_CHOCOLATE };
static const uint8_t skin_caco[] = { PAINT_CHERRY, PAINT_MAGENTA, PAINT_ROSE,
    PAINT_GRAPE, PAINT_BLOOD, PAINT_TANGERINE };
static const uint8_t skin_bat[] = { PAINT_GRAPE, PAINT_CHARCOAL, PAINT_LAVENDER,
    PAINT_VAPE_PURPLE, PAINT_JET };
static const uint8_t skin_robot[] = { PAINT_SILVER, PAINT_COBALT, PAINT_SKY,
    PAINT_MINT, PAINT_LEMON, PAINT_WHITE, PAINT_CHARCOAL };
static const uint8_t skin_plush[] = { PAINT_BUBBLEGUM, PAINT_LAVENDER,
    PAINT_SKY, PAINT_MINT, PAINT_PEACH, PAINT_LEMON, PAINT_CARAMEL };
static const uint8_t skin_ghost[] = { PAINT_WHITE, PAINT_ECTOPLASM,
    PAINT_LAVENDER, PAINT_SKY, PAINT_MINT };
static const uint8_t skin_mummy[] = { PAINT_BONE, PAINT_WHITE, PAINT_CARAMEL };
static const uint8_t skin_moth[] = { PAINT_LAVENDER, PAINT_WHITE, PAINT_BONE,
    PAINT_PEACH, PAINT_SKY, PAINT_LEMON };
static const uint8_t skin_frog[] = { PAINT_LIME, PAINT_MINT, PAINT_CHARTREUSE,
    PAINT_TEAL, PAINT_FOREST, PAINT_SKY };

/* ---------------------------------------------------------------------- */
/* outfit colour schemes: main, accent, extra                               */
typedef struct { uint8_t a, b, c; } Scheme;
static const Scheme scheme_cute[] = {
    { PAINT_BUBBLEGUM, PAINT_MINT, PAINT_LEMON },
    { PAINT_LAVENDER, PAINT_PEACH, PAINT_SKY },
    { PAINT_SKY, PAINT_LEMON, PAINT_WHITE },
    { PAINT_MINT, PAINT_ROSE, PAINT_WHITE },
    { PAINT_PEACH, PAINT_TEAL, PAINT_HONEY },
    { PAINT_ROSE, PAINT_LAVENDER, PAINT_WHITE },
    { PAINT_LEMON, PAINT_SKY, PAINT_BUBBLEGUM },
    { PAINT_TANGERINE, PAINT_MINT, PAINT_CHOCOLATE },
    { PAINT_CARAMEL, PAINT_BUBBLEGUM, PAINT_WHITE },
    { PAINT_CYAN, PAINT_MAGENTA, PAINT_LEMON },
    { PAINT_LIME, PAINT_LAVENDER, PAINT_PEACH },
    { PAINT_CHERRY, PAINT_WHITE, PAINT_SKY },
    { PAINT_GRAPE, PAINT_LEMON, PAINT_MINT },
    { PAINT_TEAL, PAINT_PEACH, PAINT_WHITE },
    { PAINT_COBALT, PAINT_LEMON, PAINT_ROSE },
    { PAINT_HONEY, PAINT_FOREST, PAINT_WHITE }
};
static const Scheme scheme_hell[] = {
    { PAINT_JET, PAINT_CHERRY, PAINT_SILVER },
    { PAINT_CHARCOAL, PAINT_VAPE_PURPLE, PAINT_BONE },
    { PAINT_BLOOD, PAINT_JET, PAINT_HELLFIRE },
    { PAINT_GRAPE, PAINT_SLIME, PAINT_JET },
    { PAINT_NAVY, PAINT_MAGENTA, PAINT_SILVER },
    { PAINT_RUST, PAINT_CHARCOAL, PAINT_HONEY },
    { PAINT_JET, PAINT_ECTOPLASM, PAINT_GRAPE },
    { PAINT_CHARCOAL, PAINT_HELLFIRE, PAINT_BONE },
    { PAINT_VAPE_PURPLE, PAINT_JET, PAINT_LEMON },
    { PAINT_FOREST, PAINT_BLOOD, PAINT_BONE },
    { PAINT_JET, PAINT_BUBBLEGUM, PAINT_SILVER },
    { PAINT_MAGENTA, PAINT_JET, PAINT_CYAN }
};

static const uint8_t hair_natural[] = { PAINT_CHOCOLATE, PAINT_CARAMEL,
    PAINT_HONEY, PAINT_JET, PAINT_LEMON, PAINT_RUST, PAINT_SILVER,
    PAINT_WHITE, PAINT_CHARCOAL };
static const uint8_t hair_pastel[] = { PAINT_BUBBLEGUM, PAINT_LAVENDER,
    PAINT_SKY, PAINT_MINT, PAINT_PEACH, PAINT_LEMON, PAINT_ROSE };
static const uint8_t hair_hell[] = { PAINT_JET, PAINT_CHARCOAL, PAINT_BLOOD,
    PAINT_GRAPE, PAINT_VAPE_PURPLE, PAINT_MAGENTA, PAINT_HELLFIRE,
    PAINT_SLIME, PAINT_WHITE, PAINT_CHERRY };
static const uint8_t eye_cute_col[] = { PAINT_HONEY, PAINT_LEMON, PAINT_SKY,
    PAINT_MINT, PAINT_CHOCOLATE, PAINT_JET, PAINT_COBALT, PAINT_FOREST,
    PAINT_LAVENDER, PAINT_ROSE, PAINT_TEAL, PAINT_CARAMEL };
static const uint8_t eye_hell_col[] = { PAINT_CHERRY, PAINT_HELLFIRE,
    PAINT_LEMON, PAINT_SLIME, PAINT_VAPE_PURPLE, PAINT_MAGENTA,
    PAINT_ECTOPLASM, PAINT_BLOOD, PAINT_JET };
static const uint8_t horn_col_cute[] = { PAINT_BONE, PAINT_HONEY,
    PAINT_LAVENDER, PAINT_BUBBLEGUM, PAINT_WHITE, PAINT_LEMON };
static const uint8_t horn_col_hell[] = { PAINT_BONE, PAINT_JET, PAINT_CHERRY,
    PAINT_CHARCOAL, PAINT_BLOOD, PAINT_GRAPE };

/* ---------------------------------------------------------------------- */
/* part weights                                                             */
static const Pick eyes_cute[] = {
    { EYE_ROUND, 14 }, { EYE_ANIME, 12 }, { EYE_DOT, 10 }, { EYE_HAPPY_ARC, 8 },
    { EYE_SPARKLE, 8 }, { EYE_LASHES, 8 }, { EYE_WIDE, 6 }, { EYE_SLEEPY, 4 },
    { EYE_STAR, 3 }, { EYE_HEART, 3 }, { EYE_DROOPY, 4 }, { EYE_CAT, 3 }
};
static const Pick eyes_hell[] = {
    { EYE_ANGRY, 10 }, { EYE_SLEEPY, 8 }, { EYE_GOAT, 7 }, { EYE_DEMON_SLIT, 8 },
    { EYE_SQUINT, 7 }, { EYE_GLOW, 6 }, { EYE_ROUND, 6 }, { EYE_ANIME, 5 },
    { EYE_CAT, 5 }, { EYE_LASHES, 4 }, { EYE_DROOPY, 4 }, { EYE_HOLLOW, 2 },
    { EYE_VOID, 2 }, { EYE_X, 1 }, { EYE_SPIRAL, 1 }, { EYE_CROSS, 1 }
};
static const Pick brows_cute[] = {
    { BROW_NONE, 10 }, { BROW_THIN, 10 }, { BROW_DOTS, 6 }, { BROW_RAISED, 4 },
    { BROW_WORRIED, 3 }, { BROW_ARCHED, 3 }, { BROW_THICK, 2 }
};
static const Pick brows_hell[] = {
    { BROW_ANGRY, 10 }, { BROW_THICK, 6 }, { BROW_ARCHED, 6 }, { BROW_NONE, 5 },
    { BROW_SCARRED, 4 }, { BROW_UNIBROW, 3 }, { BROW_FLAT, 4 }, { BROW_BUSHY, 3 }
};
static const Pick mouths_cute[] = {
    { MOUTH_SMILE, 14 }, { MOUTH_OPEN, 6 }, { MOUTH_GRIN, 5 }, { MOUTH_O, 4 },
    { MOUTH_BIG_GRIN, 4 }, { MOUTH_POUT, 3 }, { MOUTH_TONGUE, 3 },
    { MOUTH_WAVY, 2 }, { MOUTH_LIPSTICK, 2 }
};
static const Pick mouths_hell[] = {
    { MOUTH_SMIRK, 10 }, { MOUTH_TEETH, 6 }, { MOUTH_SHARK, 6 }, { MOUTH_FANGY, 8 },
    { MOUTH_GRIN, 6 }, { MOUTH_FLAT, 6 }, { MOUTH_FROWN, 4 }, { MOUTH_TONGUE, 4 },
    { MOUTH_STITCHED, 3 }, { MOUTH_DROOL, 2 }, { MOUTH_LIPSTICK, 3 },
    { MOUTH_VOID, 1 }, { MOUTH_SMILE, 4 }
};
static const Pick hair_cute[] = {
    { HAIR_SHORT, 10 }, { HAIR_MESSY, 8 }, { HAIR_BOB, 8 }, { HAIR_CURLY, 6 },
    { HAIR_PIGTAILS, 5 }, { HAIR_TWINTAILS, 5 }, { HAIR_BUN, 5 },
    { HAIR_PONYTAIL, 6 }, { HAIR_ODANGO, 3 }, { HAIR_WAVY, 5 }, { HAIR_HIME, 3 },
    { HAIR_LONG, 6 }, { HAIR_BOWL, 3 }, { HAIR_AFRO, 3 }, { HAIR_BRAIDS, 3 },
    { HAIR_CROWN_BRAID, 2 }, { HAIR_POMPADOUR, 2 }, { HAIR_BUZZ, 2 }, { HAIR_LOCS, 3 }
};
static const Pick hair_hellw[] = {
    { HAIR_EMO_FRINGE, 10 }, { HAIR_MOHAWK, 7 }, { HAIR_SPIKY, 8 },
    { HAIR_SIDE_SHAVE, 7 }, { HAIR_LIBERTY_SPIKES, 4 }, { HAIR_MULLET, 5 },
    { HAIR_WILD, 6 }, { HAIR_SLICKED, 6 }, { HAIR_LONG, 6 }, { HAIR_MESSY, 5 },
    { HAIR_BUZZ, 3 }, { HAIR_HIME, 3 }, { HAIR_DRILLS, 2 }, { HAIR_FLAME, 2 },
    { HAIR_TENTACLES, 1 }, { HAIR_TWINTAILS, 3 }, { HAIR_LOCS, 3 }
};
static const Pick tops_cute[] = {
    { TOP_TSHIRT, 10 }, { TOP_HOODIE, 8 }, { TOP_SWEATER, 8 }, { TOP_DRESS, 7 },
    { TOP_OVERALLS, 6 }, { TOP_SWEATER_VEST, 4 }, { TOP_PAJAMAS, 3 },
    { TOP_KIMONO, 3 }, { TOP_CROP_TOP, 3 }, { TOP_TANK, 3 }, { TOP_NURSE, 1 },
    { TOP_LAB_COAT, 1 }, { TOP_MAID, 1 }, { TOP_PRIEST, 1 }, { TOP_SUIT, 2 }
};
static const Pick tops_hell[] = {
    { TOP_LEATHER_JACKET, 10 }, { TOP_BAND_TEE, 9 }, { TOP_HOODIE, 8 },
    { TOP_CLOAK, 6 }, { TOP_ROBE, 5 }, { TOP_SUIT, 6 }, { TOP_ARMOR, 3 },
    { TOP_TANK, 4 }, { TOP_CROP_TOP, 4 }, { TOP_DRESS, 4 }, { TOP_LAB_COAT, 2 },
    { TOP_PLATE_ARMOR, 1 }, { TOP_HAZMAT, 1 }, { TOP_TSHIRT, 3 }
};
static const Pick bottoms_cute[] = {
    { BOTTOM_SHORTS, 8 }, { BOTTOM_SKIRT, 8 }, { BOTTOM_PANTS, 8 },
    { BOTTOM_LEGGINGS, 4 }, { BOTTOM_LONG_SKIRT, 3 }, { BOTTOM_TUTU, 2 },
    { BOTTOM_SWEATS, 3 }, { BOTTOM_CARGO, 2 }
};
static const Pick bottoms_hell[] = {
    { BOTTOM_RIPPED_JEANS, 10 }, { BOTTOM_CARGO, 6 }, { BOTTOM_PANTS, 6 },
    { BOTTOM_LEGGINGS, 5 }, { BOTTOM_KILT, 3 }, { BOTTOM_SKIRT, 5 },
    { BOTTOM_LONG_SKIRT, 3 }, { BOTTOM_SWEATS, 2 }
};
static const Pick shoes_cute[] = {
    { SHOES_BARE, 6 }, { SHOES_SNEAKERS, 10 }, { SHOES_SLIPPERS, 5 },
    { SHOES_BOOTS, 5 }, { SHOES_SANDALS, 4 }, { SHOES_CLOGS, 2 },
    { SHOES_ROLLERSKATES, 1 }, { SHOES_HEELS, 1 }
};
static const Pick shoes_hell[] = {
    { SHOES_COMBAT, 10 }, { SHOES_BOOTS, 8 }, { SHOES_PLATFORM, 6 },
    { SHOES_SNEAKERS, 4 }, { SHOES_BARE, 3 }, { SHOES_HEELS, 2 }
};
static const Pick hats_cute[] = {
    { HAT_BERET, 6 }, { HAT_BEANIE, 6 }, { HAT_FLOWER_CROWN, 6 }, { HAT_CAP, 5 },
    { HAT_PARTY, 3 }, { HAT_BUCKET, 4 }, { HAT_HEADPHONES, 3 }, { HAT_TIARA, 2 },
    { HAT_CHEF, 1 }, { HAT_PROPELLER, 1 }, { HAT_BANDANA, 2 }
};
static const Pick hats_hell[] = {
    { HAT_SKULL_CAP, 6 }, { HAT_BEANIE, 6 }, { HAT_WITCH, 4 },
    { HAT_HORNED_HELMET, 3 }, { HAT_TOP_HAT, 4 }, { HAT_BANDANA, 4 },
    { HAT_CROWN, 2 }, { HAT_HEADPHONES, 4 }, { HAT_TRAFFIC_CONE, 1 },
    { HAT_NUN_VEIL, 1 }, { HAT_CAP, 3 }
};
static const Pick acc_cute[] = {
    { ACC_BOWTIE, 6 }, { ACC_SCARF, 6 }, { ACC_GLASSES, 6 }, { ACC_BACKPACK, 4 },
    { ACC_TEDDY, 3 }, { ACC_EARRINGS, 4 }, { ACC_CROSS_NECKLACE, 2 },
    { ACC_PACIFIER, 1 }, { ACC_MONOCLE, 1 }
};
static const Pick acc_hell[] = {
    { ACC_CHAINS, 7 }, { ACC_SKULL_NECKLACE, 7 }, { ACC_SUNGLASSES, 6 },
    { ACC_EYEPATCH, 4 }, { ACC_NOSE_RING, 5 }, { ACC_MASK, 3 },
    { ACC_GAS_MASK, 2 }, { ACC_CAPE, 3 }, { ACC_EARRINGS, 4 }, { ACC_SCARF, 2 }
};
static const Pick marks_cute[] = {
    { MARK_CHEEK_HEARTS, 5 }, { MARK_STAR_FRECKLES, 5 }, { MARK_SPOTS, 3 },
    { MARK_STRIPES, 2 }, { MARK_TEARS, 1 }
};
static const Pick marks_hell[] = {
    { MARK_SKULL_PAINT, 5 }, { MARK_TRIBAL, 4 }, { MARK_STITCHES, 4 },
    { MARK_CRACKS, 3 }, { MARK_EYELINER, 6 }, { MARK_PENTAGRAM, 2 },
    { MARK_THIRD_EYE, 1 }, { MARK_TEARS, 2 }, { MARK_SCALES, 2 }
};

/* ---------------------------------------------------------------------- */
static uint8_t paint_or(DoomoRng *r, uint8_t usual, int keep_pct)
{
    return rng_chance(r, keep_pct) ? usual : (uint8_t)rng_range(r, PAL_PAINT_COUNT);
}

static void gen_look(DoomoLook *l, DoomoRng *r, int hell)
{
    const Scheme *sc = hell ? &ONE_OF(r, scheme_hell) : &ONE_OF(r, scheme_cute);
    int sp = hell ? PICK(r, sp_hell) : PICK(r, sp_cute);
    const DoomoSpeciesInfo *si = &doomo_species_info[sp];
    uint8_t fur_b = PAINT_WHITE;
    int critter = is_critter(sp);

    memset(l, 0, sizeof(*l));
    l->species = (uint8_t)sp;

    /* ---- skin / fur ---- */
    {
        const Fur *f = NULL;
        switch (sp) {
        case SPECIES_CATFOLK:    f = &ONE_OF(r, fur_cat); break;
        case SPECIES_FOXKIN:     f = &ONE_OF(r, fur_fox); break;
        case SPECIES_BEARKIN:    f = &ONE_OF(r, fur_bear); break;
        case SPECIES_PUPKIN:     f = &ONE_OF(r, fur_pup); break;
        case SPECIES_MOUSEKIN:   f = &ONE_OF(r, fur_mouse); break;
        case SPECIES_SHEEPKIN:   f = &ONE_OF(r, fur_sheep); break;
        case SPECIES_RACCOONKIN: f = &ONE_OF(r, fur_raccoon); break;
        case SPECIES_OWLKIN:     f = &ONE_OF(r, fur_owl); break;
        case SPECIES_BUNNYKIN:   f = &ONE_OF(r, fur_bunny); break;
        default: break;
        }
        if (f) {
            l->skin = (uint8_t)(8 + f->a);
            fur_b = f->b;
        } else {
            uint8_t p;
            switch (sp) {
            case SPECIES_IMP:       p = ONE_OF(r, skin_imp); break;
            case SPECIES_GHOUL:     p = ONE_OF(r, skin_ghoul); break;
            case SPECIES_SKELETON:  p = ONE_OF(r, skin_skel); break;
            case SPECIES_SLIME:     p = ONE_OF(r, skin_slime); break;
            case SPECIES_HELLHOUND: p = ONE_OF(r, skin_hound); break;
            case SPECIES_CACOBLOB:  p = ONE_OF(r, skin_caco); break;
            case SPECIES_BATLING:   p = ONE_OF(r, skin_bat); break;
            case SPECIES_ROBOT:     p = ONE_OF(r, skin_robot); break;
            case SPECIES_PLUSHIE:   p = ONE_OF(r, skin_plush); break;
            case SPECIES_GHOST:     p = ONE_OF(r, skin_ghost); break;
            case SPECIES_MUMMY:     p = ONE_OF(r, skin_mummy); break;
            case SPECIES_MOTHKIN:   p = ONE_OF(r, skin_moth); break;
            case SPECIES_FROGLING:  p = ONE_OF(r, skin_frog); break;
            default:                p = 0xFF; break;
            }
            if (p == 0xFF) l->skin = (uint8_t)rng_range(r, PAL_SKIN_COUNT);
            else l->skin = (uint8_t)(8 + p);
            if (sp == SPECIES_CHERUB) l->skin = (uint8_t)rng_range(r, 6);
            fur_b = sc->c;
        }
    }

    /* ---- body & head ---- */
    if (hell) {
        static const Pick b[] = { { BODY_AVERAGE, 6 }, { BODY_LANKY, 6 },
            { BODY_HUNCHED, 4 }, { BODY_STOCKY, 4 }, { BODY_BUFF, 3 },
            { BODY_NOODLE, 3 }, { BODY_TOP_HEAVY, 3 }, { BODY_ROUND, 3 },
            { BODY_TINY, 3 }, { BODY_PEAR, 2 } };
        l->body = (uint8_t)PICK(r, b);
    } else {
        static const Pick b[] = { { BODY_AVERAGE, 8 }, { BODY_ROUND, 8 },
            { BODY_TINY, 7 }, { BODY_PEAR, 4 }, { BODY_STOCKY, 2 },
            { BODY_LANKY, 2 }, { BODY_TOP_HEAVY, 1 } };
        l->body = (uint8_t)PICK(r, b);
    }
    l->height = (uint8_t)rng_between(r, 3, 12);
    l->build = (uint8_t)rng_between(r, critter ? 6 : 3, 12);
    switch (sp) {
    case SPECIES_SKELETON: l->head_shape = HEAD_SKULL; break;
    case SPECIES_CACOBLOB: l->head_shape = HEAD_BLOB; break;
    case SPECIES_ROBOT: l->head_shape = rng_chance(r, 70) ? HEAD_SQUARE : HEAD_DIAMOND; break;
    case SPECIES_SLIME: l->head_shape = rng_chance(r, 60) ? HEAD_BLOB : HEAD_MOCHI; break;
    default: {
        static const Pick h[] = { { HEAD_ROUND, 12 }, { HEAD_MOCHI, 8 },
            { HEAD_OVAL, 5 }, { HEAD_BEAN, 4 }, { HEAD_HEART, 3 }, { HEAD_PEAR, 2 },
            { HEAD_STAR, 1 } };
        static const Pick hh[] = { { HEAD_ROUND, 6 }, { HEAD_OVAL, 6 },
            { HEAD_TRIANGLE, 4 }, { HEAD_SQUARE, 3 }, { HEAD_DIAMOND, 3 },
            { HEAD_BEAN, 3 }, { HEAD_MOCHI, 3 }, { HEAD_PEAR, 2 } };
        l->head_shape = (uint8_t)(hell ? PICK(r, hh) : PICK(r, h));
        break; }
    }
    l->head_size = (uint8_t)rng_between(r, 8, 14);

    /* ---- eyes, brows, nose, mouth ---- */
    if (sp == SPECIES_PLUSHIE && rng_chance(r, 70)) l->eye_style = EYE_BUTTON;
    else if (sp == SPECIES_MOTHKIN && rng_chance(r, 50)) l->eye_style = EYE_COMPOUND;
    else if (sp == SPECIES_CATFOLK && rng_chance(r, 40)) l->eye_style = EYE_CAT;
    else if (sp == SPECIES_SKELETON && rng_chance(r, 60)) l->eye_style = EYE_HOLLOW;
    else if (sp == SPECIES_GHOST && rng_chance(r, 40)) l->eye_style = EYE_VOID;
    else l->eye_style = (uint8_t)(hell ? PICK(r, eyes_hell) : PICK(r, eyes_cute));
    l->eye_color = hell ? ONE_OF(r, eye_hell_col) : ONE_OF(r, eye_cute_col);
    l->eye_size = (uint8_t)rng_between(r, 7, 13);
    l->eye_spacing = (uint8_t)rng_between(r, 5, 10);
    l->eye_height = (uint8_t)rng_between(r, 6, 10);
    l->eye_tilt = (uint8_t)(hell ? rng_between(r, 5, 11) : rng_between(r, 7, 9));
    if (sp == SPECIES_CACOBLOB) l->eye_count = 1;
    else if (rng_chance(r, hell ? 5 : 1)) l->eye_count = 3;
    else if (rng_chance(r, 2)) l->eye_count = 1;
    else l->eye_count = 2;
    l->brow_style = (uint8_t)(hell ? PICK(r, brows_hell) : PICK(r, brows_cute));
    l->brow_height = (uint8_t)rng_between(r, 5, 10);
    switch (sp) {
    case SPECIES_CATFOLK: case SPECIES_FOXKIN: case SPECIES_MOUSEKIN:
        l->nose_style = NOSE_CAT; break;
    case SPECIES_BEARKIN: case SPECIES_PUPKIN: case SPECIES_SHEEPKIN:
    case SPECIES_BUNNYKIN: case SPECIES_RACCOONKIN: case SPECIES_HELLHOUND:
        l->nose_style = NOSE_BUTTON; break;
    case SPECIES_OWLKIN: case SPECIES_SLIME: case SPECIES_GHOST:
    case SPECIES_CACOBLOB: case SPECIES_FROGLING: case SPECIES_ROBOT:
        l->nose_style = NOSE_NONE; break;
    case SPECIES_SKELETON: l->nose_style = NOSE_SKULL; break;
    case SPECIES_PLUSHIE: l->nose_style = rng_chance(r, 50) ? NOSE_BUTTON : NOSE_DOT; break;
    default: {
        static const Pick n[] = { { NOSE_DOT, 8 }, { NOSE_BUTTON, 8 },
            { NOSE_ROUND, 5 }, { NOSE_POINTY, 4 }, { NOSE_NONE, 3 }, { NOSE_WIDE, 2 },
            { NOSE_HOOK, 2 }, { NOSE_LONG, 1 }, { NOSE_PIG, 1 } };
        l->nose_style = (uint8_t)PICK(r, n);
        break; }
    }
    l->nose_size = (uint8_t)rng_between(r, 4, 10);
    if (sp == SPECIES_OWLKIN) l->mouth_style = MOUTH_BEAK;
    else if ((sp == SPECIES_CATFOLK || sp == SPECIES_FOXKIN || sp == SPECIES_BUNNYKIN ||
              sp == SPECIES_MOUSEKIN) && rng_chance(r, 55)) l->mouth_style = MOUTH_CAT;
    else if (sp == SPECIES_FROGLING && rng_chance(r, 50)) l->mouth_style = MOUTH_WAVY;
    else if (sp == SPECIES_PLUSHIE && rng_chance(r, 40)) l->mouth_style = MOUTH_STITCHED;
    else l->mouth_style = (uint8_t)(hell ? PICK(r, mouths_hell) : PICK(r, mouths_cute));
    l->mouth_size = (uint8_t)rng_between(r, 4, 10);
    l->fangs = (uint8_t)(hell ? rng_range(r, 4) : (rng_chance(r, 8) ? 1 : 0));

    /* ---- hair ---- */
    if (rng_chance(r, si->hair_chance)) {
        if (sp == SPECIES_SLIME && rng_chance(r, 50)) l->hair_style = HAIR_SLIME_DRIP;
        else if (sp == SPECIES_SHEEPKIN && rng_chance(r, 60))
            l->hair_style = rng_chance(r, 60) ? HAIR_CURLY : HAIR_AFRO;
        else l->hair_style = (uint8_t)(hell ? PICK(r, hair_hellw) : PICK(r, hair_cute));
    } else {
        l->hair_style = HAIR_BALD;
    }
    if (critter && rng_chance(r, 55))
        l->hair_color = rng_chance(r, 70) ? (uint8_t)(l->skin - 8) : fur_b;
    else if (hell)
        l->hair_color = ONE_OF(r, hair_hell);
    else
        l->hair_color = rng_chance(r, 55) ? ONE_OF(r, hair_natural) : ONE_OF(r, hair_pastel);
    l->hair_highlight = rng_chance(r, 25) ?
        (hell ? ONE_OF(r, hair_hell) : ONE_OF(r, hair_pastel)) : LOOK_NO_COLOR;
    if (l->hair_highlight == l->hair_color) l->hair_highlight = LOOK_NO_COLOR;
    l->facial_hair = (uint8_t)(rng_chance(r, critter ? 2 : (hell ? 12 : 8)) ?
                               rng_between(r, 1, FHAIR_COUNT - 1) : FHAIR_NONE);

    /* ---- ears, horns, tail, wings, halo ---- */
    switch (sp) {
    case SPECIES_CATFOLK:    l->ears = EARS_CAT; break;
    case SPECIES_FOXKIN:     l->ears = EARS_FOX; break;
    case SPECIES_BEARKIN:    l->ears = EARS_BEAR; break;
    case SPECIES_PUPKIN:     l->ears = rng_chance(r, 70) ? EARS_FLOPPY_PUP : EARS_DOG; break;
    case SPECIES_MOUSEKIN:   l->ears = EARS_MOUSE; break;
    case SPECIES_SHEEPKIN:   l->ears = EARS_SHEEP; break;
    case SPECIES_RACCOONKIN: l->ears = EARS_RACCOON; break;
    case SPECIES_OWLKIN:     l->ears = EARS_OWL_TUFTS; break;
    case SPECIES_BUNNYKIN:   l->ears = EARS_BUNNY; break;
    case SPECIES_IMP:        l->ears = EARS_POINTY; break;
    case SPECIES_GHOUL:      l->ears = rng_chance(r, 50) ? EARS_ELF : EARS_POINTY; break;
    case SPECIES_HELLHOUND:  l->ears = EARS_DOG; break;
    case SPECIES_BATLING:    l->ears = EARS_BAT; break;
    case SPECIES_FROGLING:   l->ears = rng_chance(r, 30) ? EARS_FIN : EARS_NONE; break;
    case SPECIES_PLUSHIE: {
        static const uint8_t pe[] = { EARS_BEAR, EARS_BUNNY, EARS_CAT, EARS_MOUSE };
        l->ears = ONE_OF(r, pe);
        break; }
    case SPECIES_SKELETON: case SPECIES_SLIME: case SPECIES_CACOBLOB:
    case SPECIES_ROBOT: case SPECIES_GHOST: case SPECIES_MOTHKIN:
        l->ears = EARS_NONE; break;
    default:
        l->ears = rng_chance(r, 5) ? EARS_ELF : EARS_NORMAL; break;
    }
    if (rng_chance(r, si->horn_chance)) {
        switch (sp) {
        case SPECIES_IMP: {
            static const uint8_t h[] = { HORNS_NUBS, HORNS_DEVIL, HORNS_RAM,
                                         HORNS_GOAT, HORNS_DEVIL_TALL, HORNS_NUBS };
            l->horns = ONE_OF(r, h); break; }
        case SPECIES_HELLHOUND: {
            static const uint8_t h[] = { HORNS_BULL, HORNS_DEVIL, HORNS_BROKEN };
            l->horns = ONE_OF(r, h); break; }
        case SPECIES_CACOBLOB: {
            static const uint8_t h[] = { HORNS_DEVIL, HORNS_MANY, HORNS_SPIRAL,
                                         HORNS_CROWN_SPIKES };
            l->horns = ONE_OF(r, h); break; }
        case SPECIES_ROBOT: case SPECIES_MOTHKIN:
            l->horns = HORNS_ANTENNAE; break;
        default: {
            static const uint8_t hc[] = { HORNS_UNICORN, HORNS_ANTLERS, HORNS_NUBS,
                                          HORNS_SPIRAL };
            static const uint8_t hh[] = { HORNS_DEVIL, HORNS_RAM, HORNS_ONI,
                                          HORNS_GOAT, HORNS_BROKEN };
            l->horns = hell ? ONE_OF(r, hh) : ONE_OF(r, hc);
            break; }
        }
    }
    l->horn_color = hell ? ONE_OF(r, horn_col_hell) : ONE_OF(r, horn_col_cute);
    l->horn_size = (uint8_t)rng_between(r, 4, 12);
    switch (sp) {
    case SPECIES_CATFOLK:    l->tail = TAIL_CAT; break;
    case SPECIES_FOXKIN:     l->tail = TAIL_FOX; break;
    case SPECIES_BEARKIN:    l->tail = TAIL_BEAR_NUB; break;
    case SPECIES_PUPKIN:     l->tail = TAIL_PUP_WAGGY; break;
    case SPECIES_MOUSEKIN:   l->tail = TAIL_MOUSE; break;
    case SPECIES_SHEEPKIN:   l->tail = TAIL_SHEEP_PUFF; break;
    case SPECIES_RACCOONKIN: l->tail = TAIL_RACCOON_RINGED; break;
    case SPECIES_OWLKIN:     l->tail = TAIL_FEATHER_FAN; break;
    case SPECIES_BUNNYKIN:   l->tail = TAIL_BUNNY; break;
    default:
        if (rng_chance(r, si->tail_chance)) {
            if (sp == SPECIES_IMP) l->tail = TAIL_DEVIL;
            else if (sp == SPECIES_HELLHOUND)
                l->tail = hell ? (rng_chance(r, 50) ? TAIL_FLAME : TAIL_SPIKED) : TAIL_PUP_WAGGY;
            else if (sp == SPECIES_SKELETON) l->tail = TAIL_SKELETAL;
            else if (sp == SPECIES_PLUSHIE) l->tail = rng_chance(r, 50) ? TAIL_BUNNY : TAIL_BEAR_NUB;
            else {
                static const uint8_t t[] = { TAIL_DEVIL, TAIL_LIZARD, TAIL_CAT,
                                             TAIL_TENTACLE };
                l->tail = ONE_OF(r, t);
            }
        }
        break;
    }
    if (rng_chance(r, si->wing_chance)) {
        switch (sp) {
        case SPECIES_CHERUB: case SPECIES_OWLKIN: l->wings = WINGS_ANGEL; break;
        case SPECIES_BATLING: l->wings = rng_chance(r, 60) ? WINGS_TINY_BAT : WINGS_BAT; break;
        case SPECIES_MOTHKIN: l->wings = WINGS_BUTTERFLY; break;
        case SPECIES_IMP: l->wings = rng_chance(r, 70) ? WINGS_TINY_BAT : WINGS_BAT; break;
        default: {
            static const uint8_t wc[] = { WINGS_ANGEL, WINGS_BUTTERFLY, WINGS_TINY_BAT };
            static const uint8_t wh[] = { WINGS_BAT, WINGS_BONE, WINGS_DRAGON,
                                          WINGS_FLY, WINGS_TINY_BAT };
            l->wings = hell ? ONE_OF(r, wh) : ONE_OF(r, wc);
            break; }
        }
    }
    if (sp == SPECIES_CHERUB) l->halo = rng_chance(r, 80) ? HALO_HALO : HALO_STARS;
    else if (hell && rng_chance(r, 4)) {
        static const uint8_t hh[] = { HALO_BROKEN, HALO_FLAME_RING, HALO_THORNS };
        l->halo = ONE_OF(r, hh);
    } else if (!hell && rng_chance(r, 2)) l->halo = HALO_STARS;

    /* ---- markings & cheeks ---- */
    if (critter && rng_chance(r, 75)) {
        if (sp == SPECIES_RACCOONKIN) l->marking = MARK_FUR_MASK;
        else if (sp == SPECIES_CATFOLK) {
            static const uint8_t m[] = { MARK_FUR_TABBY, MARK_FUR_CALICO,
                MARK_FUR_MUZZLE, MARK_FUR_SOCKS, MARK_FUR_BELLY };
            l->marking = ONE_OF(r, m);
        } else if (sp == SPECIES_FOXKIN) {
            static const uint8_t m[] = { MARK_FUR_TIPPED_EARS, MARK_FUR_MUZZLE,
                MARK_FUR_BELLY, MARK_FUR_SOCKS };
            l->marking = ONE_OF(r, m);
        } else {
            static const uint8_t m[] = { MARK_FUR_MUZZLE, MARK_FUR_BELLY,
                MARK_FUR_SOCKS, MARK_FUR_TIPPED_EARS };
            l->marking = ONE_OF(r, m);
        }
        l->marking_color = fur_b;
    } else if (sp == SPECIES_PLUSHIE && rng_chance(r, 60)) {
        l->marking = rng_chance(r, 50) ? MARK_STITCHES : MARK_PATCHES;
        l->marking_color = sc->b;
    } else if (rng_chance(r, hell ? 30 : 15)) {
        l->marking = (uint8_t)(hell ? PICK(r, marks_hell) : PICK(r, marks_cute));
        l->marking_color = hell ? ONE_OF(r, eye_hell_col) : sc->b;
    } else {
        l->marking_color = sc->b;
    }
    if (hell) {
        if (rng_chance(r, 30)) l->cheeks |= CHEEK_BLUSH;
        if (rng_chance(r, 15)) l->cheeks |= CHEEK_SCAR;
        if (rng_chance(r, 10)) l->cheeks |= CHEEK_BANDAGE;
        if (rng_chance(r, 10)) l->cheeks |= CHEEK_TEAR_TATTOO;
        if (rng_chance(r, 8))  l->cheeks |= CHEEK_BEAUTY_MARK;
    } else {
        if (rng_chance(r, 75)) l->cheeks |= CHEEK_BLUSH;
        if (rng_chance(r, 15)) l->cheeks |= CHEEK_FRECKLES;
        if (rng_chance(r, 15)) l->cheeks |= CHEEK_DIMPLES;
        if (rng_chance(r, 10)) l->cheeks |= CHEEK_SPARKLES;
        if (rng_chance(r, 5))  l->cheeks |= CHEEK_BEAUTY_MARK;
    }

    /* ---- outfit ---- */
    l->top = (uint8_t)(hell ? PICK(r, tops_hell) : PICK(r, tops_cute));
    l->top_color = paint_or(r, sc->a, 90);
    if (rng_chance(r, 30)) {
        static const uint8_t pc[] = { PATTERN_POLKA, PATTERN_STRIPES,
                                      PATTERN_HEARTS, PATTERN_PLAID };
        static const uint8_t ph[] = { PATTERN_SKULLS, PATTERN_FLAMES,
                                      PATTERN_STRIPES, PATTERN_CAMO, PATTERN_PLAID };
        l->pattern = hell ? ONE_OF(r, ph) : ONE_OF(r, pc);
    }
    l->pattern_color = sc->c;
    switch (l->top) {
    case TOP_DRESS: case TOP_ROBE: case TOP_KIMONO: case TOP_PAJAMAS:
    case TOP_OVERALLS: case TOP_HAZMAT: case TOP_SPACESUIT: case TOP_PRIEST:
        l->bottom = rng_chance(r, 60) ? BOTTOM_NONE :
                    (uint8_t)(hell ? PICK(r, bottoms_hell) : PICK(r, bottoms_cute));
        break;
    default:
        l->bottom = (uint8_t)(hell ? PICK(r, bottoms_hell) : PICK(r, bottoms_cute));
        break;
    }
    l->bottom_color = paint_or(r, sc->b, 85);
    if (sp == SPECIES_GHOST || sp == SPECIES_SLIME) l->shoes = SHOES_BARE;
    else if (sp == SPECIES_IMP && rng_chance(r, 30)) l->shoes = SHOES_HOOVES;
    else if (sp == SPECIES_HELLHOUND && rng_chance(r, 40)) l->shoes = SHOES_CLAWS;
    else if (critter && rng_chance(r, 35)) l->shoes = SHOES_BARE;
    else l->shoes = (uint8_t)(hell ? PICK(r, shoes_hell) : PICK(r, shoes_cute));
    l->shoes_color = paint_or(r, rng_chance(r, 50) ? sc->b : sc->c, 85);
    if (rng_chance(r, hell ? 25 : 30))
        l->hat = (uint8_t)(hell ? PICK(r, hats_hell) : PICK(r, hats_cute));
    l->hat_color = paint_or(r, rng_chance(r, 50) ? sc->a : sc->c, 85);
    if (sp == SPECIES_MUMMY) l->accessory = ACC_BANDAGES;
    else if (rng_chance(r, hell ? 35 : 30))
        l->accessory = (uint8_t)(hell ? PICK(r, acc_hell) : PICK(r, acc_cute));
    l->accessory_color = paint_or(r, rng_chance(r, 50) ? sc->b : sc->c, 85);
    if (rng_chance(r, hell ? 10 : 8)) {
        static const uint8_t ac[] = { AURA_SPARKLES, AURA_HEARTS, AURA_BUBBLES };
        static const uint8_t ah[] = { AURA_FLAMES, AURA_SMOKE, AURA_FLIES, AURA_GLITCH };
        l->aura = hell ? ONE_OF(r, ah) : ONE_OF(r, ac);
    }
}

/* ---------------------------------------------------------------------- */
static void gen_rest(DoomoResident *res, DoomoRng *r, int hell)
{
    const DoomoSpeciesInfo *si = &doomo_species_info[res->look.species];
    int i, corr;
    const char *first;

    /* name */
    if (hell && rng_chance(r, 80)) first = doomo_first_names[rng_between(r, 160, 255)];
    else if (!hell && rng_chance(r, 96)) first = doomo_first_names[rng_range(r, 160)];
    else first = doomo_first_names[rng_range(r, FIRST_NAME_COUNT)];
    memset(res->name, 0, sizeof(res->name));
    for (i = 0; i < DOOMO_NAME_LEN - 1 && first[i]; i++) res->name[i] = first[i];

    /* voice: little critters squeak, big ones rumble */
    for (i = 0; i < VOICE_COUNT; i++) res->voice[i] = (uint8_t)rng_range(r, 32);
    switch (res->look.species) {
    case SPECIES_MOUSEKIN: case SPECIES_BUNNYKIN: case SPECIES_CHERUB:
    case SPECIES_BATLING: case SPECIES_PLUSHIE:
        res->voice[VOICE_PITCH] = (uint8_t)rng_between(r, 18, 31); break;
    case SPECIES_BEARKIN: case SPECIES_HELLHOUND: case SPECIES_SKELETON:
    case SPECIES_MUMMY:
        res->voice[VOICE_PITCH] = (uint8_t)rng_between(r, 0, 14); break;
    default: break;
    }

    /* personality */
    res->persona[PERS_MOVEMENT] = (uint8_t)rng_range(r, 16);
    if (hell) {
        res->persona[PERS_SPEECH] = (uint8_t)rng_between(r, 6, 15);
        res->persona[PERS_EXPRESSION] = (uint8_t)rng_range(r, 16);
        res->persona[PERS_ATTITUDE] = (uint8_t)rng_between(r, 0, 12);
        res->persona[PERS_QUIRK] = (uint8_t)rng_between(r, 4, 15);
    } else {
        res->persona[PERS_SPEECH] = (uint8_t)rng_between(r, 0, 10);
        res->persona[PERS_EXPRESSION] = (uint8_t)rng_between(r, 4, 15);
        res->persona[PERS_ATTITUDE] = (uint8_t)rng_between(r, 5, 15);
        res->persona[PERS_QUIRK] = (uint8_t)rng_between(r, 0, 10);
    }

    /* stats */
    res->stat[STAT_HUNGER] = (uint8_t)rng_between(r, 55, 90);
    res->stat[STAT_HAPPY] = (uint8_t)(hell ? rng_between(r, 35, 80) : rng_between(r, 55, 95));
    res->stat[STAT_HYGIENE] = (uint8_t)(hell ? rng_between(r, 40, 90) : rng_between(r, 60, 100));
    res->stat[STAT_ENERGY] = (uint8_t)rng_between(r, 50, 100);
    res->stat[STAT_HEALTH] = (uint8_t)rng_between(r, 85, 100);
    res->stat[STAT_SANITY] = (uint8_t)(hell ? rng_between(r, 45, 90) : rng_between(r, 70, 100));
    corr = si->base_corruption + rng_between(r, -10, 10) + (hell ? 15 : 0);
    res->stat[STAT_CORRUPTION] = (uint8_t)DOOMO_CLAMP(corr, 0, 95);
    res->stat[STAT_SOCIAL] = (uint8_t)rng_between(r, 40, 90);

    res->status = RES_ALIVE;
    res->flags = hell ? RF_HELLBORN : 0;
    {
        static const Pick pr[] = { { PRONOUN_SHE, 38 }, { PRONOUN_HE, 38 },
            { PRONOUN_THEY, 20 }, { PRONOUN_IT, 2 }, { PRONOUN_XE, 2 } };
        res->pronoun = (uint8_t)PICK(r, pr);
        if (res->look.species == SPECIES_ROBOT && rng_chance(r, 30)) res->pronoun = PRONOUN_IT;
    }
    {
        static const Pick ro[] = { { ROMANCE_ANY, 70 }, { ROMANCE_SHE, 8 },
            { ROMANCE_HE, 8 }, { ROMANCE_THEY, 8 }, { ROMANCE_NONE, 6 } };
        res->romance = (uint8_t)PICK(r, ro);
    }
    res->bday_month = (uint8_t)rng_range(r, 12);
    res->bday_day = (uint8_t)rng_between(r, 1, 28);

    /* likes */
    if (hell && rng_chance(r, 60)) {
        static const uint8_t hf[] = { 2, 3, 4, 6, 8, 11, 36, 37, 38, 39, 40, 41,
                                      42, 43, 44, 47 };
        res->like_food = ONE_OF(r, hf);
    } else {
        res->like_food = (uint8_t)rng_range(r, hell ? FOOD_COUNT : 36);
    }
    do {
        res->hate_food = (uint8_t)rng_range(r, FOOD_COUNT);
    } while (res->hate_food == res->like_food);
    res->like_hobby = (uint8_t)(hell ? rng_between(r, 18, 31) : rng_range(r, 24));
    res->like_music = (uint8_t)(hell ? rng_between(r, 9, 15) : rng_range(r, 12));
    res->like_color = rng_chance(r, 60) ? res->look.top_color
                                        : (uint8_t)rng_range(r, PAL_PAINT_COUNT);
    res->fear = (uint8_t)(hell ? rng_between(r, 12, 23) : rng_range(r, 17));
    if (hell) res->catchphrase = (uint8_t)(rng_chance(r, 80) ? rng_between(r, 36, 63)
                                                              : rng_range(r, 36));
    else res->catchphrase = (uint8_t)(rng_chance(r, 88) ? rng_range(r, 36)
                                                        : rng_between(r, 36, 43));
    res->exorcism = 0;
    res->level = 0;
    res->mood = res->stat[STAT_HAPPY] >= 75 ? MOOD_HAPPY : MOOD_OK;
    res->activity = ACT_HOME;
    res->death_cause = DEATH_NONE;
}

void resident_generate(DoomoResident *r, uint32_t seed)
{
    DoomoRng rng;
    rng_seed(&rng, seed);
    memset(r, 0, sizeof(*r));
    gen_look(&r->look, &rng, 0);
    gen_rest(r, &rng, 0);
}

void resident_generate_hellborn(DoomoResident *r, uint32_t seed)
{
    DoomoRng rng;
    rng_seed(&rng, seed ^ 0x48E11B0Du);
    memset(r, 0, sizeof(*r));
    gen_look(&r->look, &rng, 1);
    gen_rest(r, &rng, 1);
}

void resident_randomize_look(DoomoResident *r, uint32_t seed)
{
    DoomoRng rng;
    rng_seed(&rng, seed ^ 0x100C0000u);
    gen_look(&r->look, &rng, (r->flags & RF_HELLBORN) != 0);
}

/* ---------------------------------------------------------------------- */
/* validation                                                               */
static int fix_max(uint8_t *v, int max)
{
    if (*v > max) { *v = (uint8_t)max; return 1; }
    return 0;
}
static int fix_enum(uint8_t *v, int count, int def)
{
    if (*v >= count) { *v = (uint8_t)def; return 1; }
    return 0;
}
static int fix_range(uint8_t *v, int lo, int hi, int def)
{
    if (*v < lo || *v > hi) { *v = (uint8_t)def; return 1; }
    return 0;
}

int resident_sanitize(DoomoResident *r)
{
    DoomoLook *l = &r->look;
    int n = 0, i, end;

    n += fix_enum(&l->species, SPECIES_COUNT, SPECIES_HUMAN);
    n += fix_enum(&l->body, BODY_COUNT, BODY_AVERAGE);
    n += fix_max(&l->height, 15);
    n += fix_max(&l->build, 15);
    n += fix_max(&l->skin, 8 + PAL_PAINT_COUNT - 1);
    n += fix_enum(&l->head_shape, HEAD_COUNT, HEAD_ROUND);
    n += fix_max(&l->head_size, 15);
    n += fix_enum(&l->eye_style, EYE_COUNT, EYE_ROUND);
    n += fix_max(&l->eye_color, PAL_PAINT_COUNT - 1);
    n += fix_max(&l->eye_size, 15);
    n += fix_max(&l->eye_spacing, 15);
    n += fix_max(&l->eye_height, 15);
    n += fix_max(&l->eye_tilt, 15);
    n += fix_range(&l->eye_count, 1, 3, 2);
    n += fix_enum(&l->brow_style, BROW_COUNT, BROW_NONE);
    n += fix_max(&l->brow_height, 15);
    n += fix_enum(&l->nose_style, NOSE_COUNT, NOSE_DOT);
    n += fix_max(&l->nose_size, 15);
    n += fix_enum(&l->mouth_style, MOUTH_COUNT, MOUTH_SMILE);
    n += fix_max(&l->mouth_size, 15);
    n += fix_max(&l->fangs, 3);
    n += fix_enum(&l->hair_style, HAIR_COUNT, HAIR_SHORT);
    n += fix_max(&l->hair_color, PAL_PAINT_COUNT - 1);
    if (l->hair_highlight != LOOK_NO_COLOR && l->hair_highlight >= PAL_PAINT_COUNT) {
        l->hair_highlight = LOOK_NO_COLOR;
        n++;
    }
    n += fix_enum(&l->facial_hair, FHAIR_COUNT, FHAIR_NONE);
    n += fix_enum(&l->ears, EARS_COUNT, EARS_NORMAL);
    n += fix_enum(&l->horns, HORNS_COUNT, HORNS_NONE);
    n += fix_max(&l->horn_color, PAL_PAINT_COUNT - 1);
    n += fix_max(&l->horn_size, 15);
    n += fix_enum(&l->tail, TAIL_COUNT, TAIL_NONE);
    n += fix_enum(&l->wings, WINGS_COUNT, WINGS_NONE);
    n += fix_enum(&l->halo, HALO_COUNT, HALO_NONE);
    n += fix_enum(&l->marking, MARK_COUNT, MARK_NONE);
    n += fix_max(&l->marking_color, PAL_PAINT_COUNT - 1);
    /* cheeks: every bit pattern is valid */
    n += fix_enum(&l->top, TOP_COUNT, TOP_TSHIRT);
    n += fix_max(&l->top_color, PAL_PAINT_COUNT - 1);
    n += fix_enum(&l->bottom, BOTTOM_COUNT, BOTTOM_PANTS);
    n += fix_max(&l->bottom_color, PAL_PAINT_COUNT - 1);
    n += fix_enum(&l->shoes, SHOES_COUNT, SHOES_SNEAKERS);
    n += fix_max(&l->shoes_color, PAL_PAINT_COUNT - 1);
    n += fix_enum(&l->hat, HAT_COUNT, HAT_NONE);
    n += fix_max(&l->hat_color, PAL_PAINT_COUNT - 1);
    n += fix_enum(&l->accessory, ACC_COUNT, ACC_NONE);
    n += fix_max(&l->accessory_color, PAL_PAINT_COUNT - 1);
    n += fix_enum(&l->aura, AURA_COUNT, AURA_NONE);
    n += fix_enum(&l->pattern, PATTERN_COUNT, PATTERN_SOLID);
    n += fix_max(&l->pattern_color, PAL_PAINT_COUNT - 1);

    /* name: printable ASCII, NUL-terminated, zero padded */
    end = DOOMO_NAME_LEN - 1;
    for (i = 0; i < DOOMO_NAME_LEN - 1; i++) {
        int c = (unsigned char)r->name[i];
        if (c < 0x20 || c > 0x7E) { end = i; break; }   /* NUL or junk */
    }
    for (i = end; i < DOOMO_NAME_LEN; i++) {
        if (r->name[i] != 0) { r->name[i] = 0; n++; }
    }
    if (end == 0 && r->status != RES_EMPTY) {
        memcpy(r->name, "Nameless", 9);
        n++;
    }

    for (i = 0; i < VOICE_COUNT; i++) n += fix_max(&r->voice[i], 31);
    for (i = 0; i < PERS_COUNT; i++) n += fix_max(&r->persona[i], 15);
    for (i = 0; i < STAT_COUNT; i++) n += fix_max(&r->stat[i], 100);
    n += fix_enum(&r->status, RES_GHOST + 1, RES_ALIVE);
    n += fix_enum(&r->pronoun, PRONOUN_COUNT, PRONOUN_THEY);
    n += fix_enum(&r->romance, ROMANCE_COUNT, ROMANCE_ANY);
    n += fix_enum(&r->bday_month, 12, 0);
    n += fix_range(&r->bday_day, 1, 28, 1);
    n += fix_enum(&r->like_food, FOOD_COUNT, 0);
    n += fix_enum(&r->like_hobby, HOBBY_COUNT, 0);
    n += fix_enum(&r->like_music, MUSIC_COUNT, 0);
    n += fix_max(&r->like_color, PAL_PAINT_COUNT - 1);
    n += fix_enum(&r->hate_food, FOOD_COUNT, 1);
    n += fix_enum(&r->fear, FEAR_COUNT, 0);
    n += fix_enum(&r->catchphrase, CATCHPHRASE_COUNT, 0);
    n += fix_max(&r->level, 99);
    n += fix_enum(&r->mood, MOOD_COUNT, MOOD_OK);
    n += fix_enum(&r->activity, ACT_COUNT, ACT_HOME);
    n += fix_enum(&r->death_cause, DEATH_COUNT, DEATH_NONE);
    return n;
}

void resident_default(DoomoResident *r)
{
    DoomoLook *l = &r->look;
    int i;
    memset(r, 0, sizeof(*r));
    l->species = SPECIES_CATFOLK;
    l->body = BODY_AVERAGE;
    l->height = 7; l->build = 8;
    l->skin = 8 + PAINT_CARAMEL;
    l->head_shape = HEAD_ROUND; l->head_size = 11;
    l->eye_style = EYE_ROUND; l->eye_color = PAINT_HONEY;
    l->eye_size = 10; l->eye_spacing = 7; l->eye_height = 8; l->eye_tilt = 8;
    l->eye_count = 2;
    l->brow_style = BROW_THIN; l->brow_height = 7;
    l->nose_style = NOSE_CAT; l->nose_size = 6;
    l->mouth_style = MOUTH_CAT; l->mouth_size = 6;
    l->hair_style = HAIR_MESSY; l->hair_color = PAINT_CARAMEL;
    l->hair_highlight = LOOK_NO_COLOR;
    l->ears = EARS_CAT; l->horn_color = PAINT_BONE; l->horn_size = 7;
    l->tail = TAIL_CAT;
    l->marking = MARK_FUR_MUZZLE; l->marking_color = PAINT_WHITE;
    l->cheeks = CHEEK_BLUSH;
    l->top = TOP_HOODIE; l->top_color = PAINT_BUBBLEGUM;
    l->bottom = BOTTOM_SHORTS; l->bottom_color = PAINT_SKY;
    l->shoes = SHOES_SNEAKERS; l->shoes_color = PAINT_WHITE;
    l->hat_color = PAINT_LEMON; l->accessory_color = PAINT_MINT;
    l->pattern_color = PAINT_WHITE;
    memcpy(r->name, "Mochi", 6);
    for (i = 0; i < VOICE_COUNT; i++) r->voice[i] = 16;
    for (i = 0; i < PERS_COUNT; i++) r->persona[i] = 8;
    r->persona[PERS_QUIRK] = 4;
    r->stat[STAT_HUNGER] = 80; r->stat[STAT_HAPPY] = 80;
    r->stat[STAT_HYGIENE] = 90; r->stat[STAT_ENERGY] = 90;
    r->stat[STAT_HEALTH] = 100; r->stat[STAT_SANITY] = 100;
    r->stat[STAT_CORRUPTION] = 0; r->stat[STAT_SOCIAL] = 70;
    r->status = RES_ALIVE;
    r->pronoun = PRONOUN_THEY;
    r->romance = ROMANCE_ANY;
    r->bday_month = 3; r->bday_day = 14;
    r->like_food = 0; r->hate_food = 46; r->like_hobby = 0; r->like_music = 0;
    r->like_color = PAINT_BUBBLEGUM; r->fear = 0; r->catchphrase = 0;
    r->mood = MOOD_HAPPY; r->activity = ACT_HOME;
}

int persona_type(const DoomoResident *r)
{
    return ((r->persona[PERS_MOVEMENT] >= 8) << 3) |
           ((r->persona[PERS_EXPRESSION] >= 8) << 2) |
           ((r->persona[PERS_SPEECH] >= 8) << 1) |
           (r->persona[PERS_ATTITUDE] >= 8);
}

uint8_t look_paint(uint8_t paint, int shade)
{
    if (paint == LOOK_NO_COLOR) return 0;
    if (paint >= PAL_PAINT_COUNT) paint = (uint8_t)(paint % PAL_PAINT_COUNT);
    shade = DOOMO_CLAMP(shade, 0, 2);
    return PAL_PAINT(paint, shade);
}

uint8_t look_skin(const DoomoLook *l, int shade)
{
    shade = DOOMO_CLAMP(shade, 0, 2);
    if (l->skin < PAL_SKIN_COUNT) return PAL_SKIN(l->skin, shade);
    if (l->skin < 8 + PAL_PAINT_COUNT) return PAL_PAINT(l->skin - 8, shade);
    return PAL_SKIN(SKIN_FAIR, shade);
}

uint32_t resident_fingerprint(const DoomoResident *r)
{
    const uint8_t *p = (const uint8_t *)r;
    uint32_t h = 2166136261u;
    size_t i;
    for (i = 0; i < sizeof(*r); i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}
