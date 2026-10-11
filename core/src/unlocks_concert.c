/* unlocks_concert.c - the Concert Hall + Dreams unlockables (ids 896..943,
 * owner: concert; 944..959 hold the dream journal's raw bits, see
 * concert_int.h). Rock-star and sleepyhead creator parts plus the stage
 * themes. Automatic rules read PROG_CONCERTS; need == 0 entries are
 * granted from concert_scene.c / dream.c. */
#include "concert_int.h"

#define P(id, list, idx, name, how, prog, need) { id, UL_CAT_PART, list, idx, name, how, prog, need }
#define STAGE(id, idx, name, how, prog, need) \
    { id, UL_CAT_MISC, UL_LIST_CONCERT, idx, name, how, prog, need }

const DoomoUnlock doomo_unlocks_concert[] = {
    /* ---- concerts ---- */
    P(UL_PART_BAND_TEE, CLIST_TOP, TOP_BAND_TEE, "Band Tee",
      "Put on your first concert", PROG_CONCERTS, 1),
    P(UL_PART_HEADPHONES, CLIST_HAT, HAT_HEADPHONES, "Headphones",
      "Put on 3 concerts", PROG_CONCERTS, 3),
    P(UL_PART_PLATFORM, CLIST_SHOES, SHOES_PLATFORM, "Platform Shoes",
      "Put on 6 concerts", PROG_CONCERTS, 6),
    P(UL_PART_LEATHER, CLIST_TOP, TOP_LEATHER_JACKET, "Leather Jacket",
      "Put on 10 concerts", PROG_CONCERTS, 10),
    P(UL_PART_POMPADOUR, CLIST_HAIR, HAIR_POMPADOUR, "Pompadour",
      "Get the crowd to scream for an encore", 0, 0),
    P(UL_PART_MULLET, CLIST_HAIR, HAIR_MULLET, "Mullet",
      "Headbang through a Punk, Metal or Industrial show", 0, 0),
    P(UL_PART_SPARKLE_AURA, CLIST_AURA, AURA_SPARKLES, "Sparkle Aura",
      "Earn a 5-star review", 0, 0),
    P(UL_PART_SPARKLE_EYES, CLIST_EYE, EYE_SPARKLE, "Sparkle Eyes",
      "Release a hit single", 0, 0),
    P(UL_PART_STAR_FRECKLES, CLIST_MARK, MARK_STAR_FRECKLES, "Star Freckles",
      "Conduct a full 25-voice choir", 0, 0),
    P(UL_PART_RIPPED_JEANS, CLIST_BOTTOM, BOTTOM_RIPPED_JEANS, "Ripped Jeans",
      "Start a band", 0, 0),
    P(UL_PART_STAR_HALO, CLIST_HALO, HALO_STARS, "Star Halo",
      "Sing a Hymn with a choir", 0, 0),
    P(UL_PART_EMO_FRINGE, CLIST_HAIR, HAIR_EMO_FRINGE, "Emo Fringe",
      "Put on a Goth Rock show", 0, 0),
    /* ---- stage themes ---- */
    STAGE(UL_STAGE_DISCO, STAGE_DISCO_INFERNO, "Stage: Disco Inferno",
          "Put on 2 concerts", PROG_CONCERTS, 2),
    STAGE(UL_STAGE_GRAVEYARD, STAGE_GRAVEYARD, "Stage: Graveyard Bash",
          "Let a Hellvile citizen take the stage", 0, 0),
    STAGE(UL_STAGE_HELLFIRE, STAGE_HELLFIRE, "Stage: Hellfire Pit",
          "Put on 5 concerts", PROG_CONCERTS, 5),
    STAGE(UL_STAGE_DREAMLAND, STAGE_DREAMLAND, "Stage: Dreamland",
          "Peek into a citizen's dream", 0, 0),
    /* ---- dreams ---- */
    P(UL_PART_DROOPY_EYES, CLIST_EYE, EYE_DROOPY, "Droopy Eyes",
      "Peek into a dream", 0, 0),
    P(UL_PART_WAVY_MOUTH, CLIST_MOUTH, MOUTH_WAVY, "Wavy Mouth",
      "Witness a nightmare", 0, 0),
    P(UL_PART_MESSY_HAIR, CLIST_HAIR, HAIR_MESSY, "Bedhead",
      "Wake a citizen from a nightmare", 0, 0),
    P(UL_PART_SLEEPY_EYES, CLIST_HAT, HAT_BEANIE, "Cozy Beanie",
      "See 8 different dreams", 0, 0),
    P(UL_PART_WAVY_HAIR, CLIST_HAIR, HAIR_WAVY, "Dreamy Waves",
      "Fill the dream journal (all 16 dreams)", 0, 0),
    { 0, 0, 0, 0, NULL, NULL, 0, 0 }
};

DOOMO_STATIC_ASSERT(UL_CONCERT_LAST <= UL_DREAM_JOURNAL, concert_ids_fit);

int concert_stage_unlocked(int stage)
{
    static const uint16_t ids[STAGE_COUNT] = { 0, UL_STAGE_DISCO, UL_STAGE_GRAVEYARD,
                                               UL_STAGE_HELLFIRE, UL_STAGE_DREAMLAND };
    if (stage <= 0) return 1;
    if (stage >= STAGE_COUNT) return 0;
    return unlock_has(ids[stage]);
}
