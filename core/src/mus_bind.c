/* mus_bind.c - which cue a classic SONG_* plays (owner: MU1;
 * docs/design/music/SPEC.md sec.9.3, CF-5, CF-26 .. CF-29).
 *
 * M1: every song plays its classic cue (MC_CLASSIC_FIRST + song - 1),
 * imported by tools/mmlc.py from the song itself, so the game sounds as
 * it did. From M4 the (song, scene, map, arg) key resolves to an adaptive
 * cue first (CF-29) and falls back to the classic one. Resident data and
 * functions only (CF-5): never story_*, ow_*, fp_island_*, pet_* or lv_*. */
#include "mus_int.h"

int mus_bind_cue(int song, int scene, int map, int arg)
{
    int cue;
    (void)scene; (void)map; (void)arg;
    if (song <= SONG_NONE || song >= SONG_COUNT || song > MC_CLASSIC_COUNT) return MC_NONE;
    cue = MC_CLASSIC_FIRST + song - 1;
    return music_cue_exists(cue) ? cue : MC_NONE;
}
