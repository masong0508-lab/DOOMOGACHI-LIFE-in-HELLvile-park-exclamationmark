/* mml_wall_lullaby.c - SONG_WALL_LULLABY, "Lullaby for the Wall" (owner: S4;
 * docs/design/story/SPEC.md R16, C15; story_final.md sec.14.2 S148;
 * docs/design/music/SPEC.md sec.14 / sec.15.2: "WALL redeemed, 3/4").
 *
 * Maestro Mew's piece that rings the bells the right way round. The WALL
 * motif (e f | e b-, the fog's dread) is turned kind: e f+ e | b, in E major,
 * a 3/4 lullaby on the bell (@3), soft chords and a waltz bass. The Wall
 * thumps along on the beat and Mew keeps it in: the drum is the bunny thump,
 * three and one ("are you there / yes", S135). Two 8-bar phrases (the second
 * climbs to C sharp and comes home on E), 16 bars of 144 units on every
 * channel (audio_song_check needs the four channel lengths equal), looping.
 */
const char doomo_mml_wall_lullaby_0[] =
    "t76 @3 o5 v11 l4"
    "e f+ e | b2. | a g+ f+ | g+2 e | e f+ e | >c+2.< | b a f+ | e2. |"
    "g+ a b | >c+2 <b | a g+ f+ | e2 b | e f+ e | b2 a | g+ f+ d+ | e2. |";
const char doomo_mml_wall_lullaby_1[] =
    "@4 o4 v6"
    "g+2. | g+2. | a2. | g+2. | g+2. | a2. | f+2. | g+2. |"
    "g+2. | a2. | f+2. | g+2. | g+2. | f+2. | f+2. | g+2. |";
const char doomo_mml_wall_lullaby_2[] =
    "@2 o3 v9 l4"
    "e r b | e r b | a r >e< | e r b | e r b | a r >e< | b r >f+< | e2. |"
    "e r b | a r >e< | b r >f+< | e r b | e r b | b r >f+< | b r >f+< | e2. |";
const char doomo_mml_wall_lullaby_3[] =
    "v7 [K4 K4 K4 K4 r4 r4]8";
