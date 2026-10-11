/* unlocks_studio.c - the Music Studio's unlockables (ids 240..255, owner:
 * studio) and its activities. Granted by studio_check_unlocks() from the
 * studio stats kept in the 'SONG' save chunk (songs saved, shows, Inspire
 * me songs, lyric lines, genres, choir size, encores).
 */
#include "studio_int.h"

DOOMO_STATIC_ASSERT(UL_STUDIO_LAST <= 256, studio_unlocks_in_range);

#define S(id, list, idx, name, how) { id, UL_CAT_MISC, list, idx, name, how, 0, 0 }

const DoomoUnlock doomo_unlocks_studio[] = {
    S(UL_STUDIO_WAVE_TINY, STUL_WAVE, WAVE_PULSE12, "Sound: Tiny Pulse", "Save a song in the Music Studio"),
    S(UL_STUDIO_WAVE_SAW, STUL_WAVE, WAVE_SAW, "Sound: Buzzy Saw", "Perform a song on the studio stage"),
    S(UL_STUDIO_WAVE_STATIC, STUL_WAVE, WAVE_NOISE, "Sound: Static", "Write 20 lyric lines"),
    S(UL_STUDIO_KIT_TOY, STUL_KIT, SONG_KIT_TOY, "Drums: Toy Box", "Save 2 songs in the Music Studio"),
    S(UL_STUDIO_KIT_BONES, STUL_KIT, SONG_KIT_BONES, "Drums: Bone Rattle", "Get inspired once (Inspire me!)"),
    S(UL_STUDIO_KIT_808, STUL_KIT, SONG_KIT_808, "Drums: Boom 808", "Perform 2 shows"),
    S(UL_STUDIO_KIT_FORGE, STUL_KIT, SONG_KIT_FORGE, "Drums: Hell Forge", "Save songs in 4 genres"),
    S(UL_STUDIO_TPL_SHANTY, STUL_TEMPLATE, 9, "Tune: Yo Ho Hellvile", "Save 3 songs"),
    S(UL_STUDIO_TPL_SYNTH, STUL_TEMPLATE, 10, "Tune: Neon Nightmare", "Get an encore on stage"),
    S(UL_STUDIO_TPL_JAZZ, STUL_TEMPLATE, 11, "Tune: Cat Cafe Jazz", "Get inspired 3 times"),
    S(UL_STUDIO_TPL_DISCO, STUL_TEMPLATE, 12, "Tune: Disco Inferno", "Perform 3 shows"),
    S(UL_STUDIO_TPL_FORGE, STUL_TEMPLATE, 13, "Tune: Forge of Clank", "Perform with a full 25 choir"),
    S(UL_STUDIO_LAYOUT_PYRAMID, STUL_LAYOUT, CHOIR_PYRAMID, "Choir Pyramid", "Perform with 10+ singers"),
    { 0, 0, 0, 0, NULL, NULL, 0, 0 }
};

static const DoomoActivity studio_acts[] = {
    { "Music Studio", "Write songs, lyrics & choir pieces", ACTLOC_CONCERT_HALL,
      ACTF_FREEPLAY | ACTF_STORY, ICON_NOTE, 1, ACT_ALWAYS, &SCENE_STUDIO, -1 },
    { "Music Studio", "Compose for the town choir", ACTLOC_TOWN_HALL,
      ACTF_FREEPLAY | ACTF_STORY, ICON_NOTE, 1, ACT_ALWAYS, &SCENE_STUDIO, -1 },
    { NULL, NULL, 0, 0, 0, 0, 0, NULL, 0 }
};
const DoomoActivity *const doomo_activities_studio = studio_acts;
