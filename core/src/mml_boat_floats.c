/* mml_boat_floats.c - SONG_BOAT_FLOATS, "Boat Floats" (owner: S5;
 * docs/design/story/SPEC.md R16, C15; story_final.md sec.14 S191).
 *
 * Barnaby's first happy shanty, written at Capt. Whisk's request ("try one
 * where the boat floats"): a D-major jig in 6/8 for the pier at the Lake of
 * Fire. A tin whistle (@15) carries the tune, the squeezebox (@4 major
 * chords) holds each bar, the bass goes oom-pah on the dotted beats and the
 * kit stamps one-and-four like boots on planks. The middle eight climbs
 * ("the boat floats!") and every phrase comes home on D; nobody sinks.
 * 16 bars of 144 units on every channel (audio_song_check needs the four
 * channel lengths equal), looping.
 */
const char doomo_mml_boat_floats_0[] =
    "t160 @15 o5 v12 l8"
    "a4 f+ a4 b | a4 f+ d4. | e4 f+ g4 a | b4 a f+4. |"
    "a4 f+ a4 b | >d4 c+< b4. | a4 f+ e4 c+ | d4. r4. |"
    "b4 b >c+4 d< | a4 a f+4. | g4 g b4 g | f+4 e a4. |"
    "a4 f+ a4 b | >d4 e f+4.< | e4 c+ <a4> c+ | d4. r4 r |";
const char doomo_mml_boat_floats_1[] =
    "@4 o4 v7 d2. | d2. | g2. | d2. | d2. | g2. | a2. | d2. |"
    "g2. | d2. | g2. | a2. | d2. | d2. | a2. | d2. |";
const char doomo_mml_boat_floats_2[] =
    "@2 o3 v10"
    "d4. <a4.> | d4. <a4.> | <g4.> d4. | d4. <a4.> | d4. <a4.> | <g4.> d4. | <a4.> e4. | d4. <a4.> |"
    "<g4.> d4. | d4. <a4.> | <g4.> d4. | <a4.> e4. | d4. <a4.> | d4. <a4.> | <a4.> e4. | d4. <a4.> |";
const char doomo_mml_boat_floats_3[] =
    "v8 [K4 H8 S4 H8]15 K4 H8 C4.";
