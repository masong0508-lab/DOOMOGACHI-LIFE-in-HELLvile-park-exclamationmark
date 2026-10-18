/* mml_mingle.c - SONG_MINGLE, "Mingle" (owner: S2; docs/design/story/SPEC.md
 * R16, C15; story_final.md sec.9): the music box of the old potluck. P0
 * plays it under the crayon prologue, the Mayor hums it walking you home in
 * Ep 1.6 (and stops when he notices), F1 plays it in sepia. Later chapters
 * use the same tune (the hummed and the full version, forwards and
 * reversed for the two-faith rite).
 *
 * F major, 4/4, 8 bars of 192 units on every channel (audio_song_check
 * needs the four channel lengths equal): @12 music box lead, @4 soft major
 * chords, @2 triangle bass, a whisper of shaker.
 */
const char doomo_mml_mingle_0[] =
    "t96 @12 o5 l8 v12"
    "c f a f >c4< a4 | b- a g f g4 c4 | d f b- >d< c4 a4 | g f e g f2 |"
    "c f a >c d4 c4< | b- >d< b- g a4 f4 | g a b- g a f d e | f2 r2 |";
const char doomo_mml_mingle_1[] =
    "@4 o4 v6 f2 f2 | c2 c2 | <b-2 b-2> | c2 f2 |"
    "f2 f2 | <b-2 b-2> | c2 c2 | f1 |";
const char doomo_mml_mingle_2[] =
    "@2 o3 v10 l4 f r c r | c r <g> r | <b- r> f r | c r f r |"
    "f r c r | <b- r> f r | c r <g> r | f r f r |";
const char doomo_mml_mingle_3[] =
    "v4 [H8 r8 H8 r8 H8 r8 H8 r8]8";
