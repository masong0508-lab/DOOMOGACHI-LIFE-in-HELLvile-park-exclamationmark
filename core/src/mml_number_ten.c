/* mml_number_ten.c - SONG_NUMBER_TEN, the "Number Ten" jingle (owner: S5;
 * docs/design/story/SPEC.md R16, C15; story_final.md sec.14 S192).
 *
 * The Hellvile bakery counter. A foghorn (@20 low brass) calls "num-ber
 * nine" (g . e c, falling), a thimble taps the counter (one bell plink:
 * nobody comes), twice; then, for the first time in eighty years, the
 * call goes up instead: "num-ber TEN" (g . a b c'), the shop bell rings
 * ding-dong and the cymbal cheers. Nobody has a ten. They call it anyway.
 * 4/4, 8 bars of 192 units on every channel (audio_song_check needs the
 * four channel lengths equal), looping.
 */
const char doomo_mml_number_ten_0[] =
    "t112 @20 o4 v13"
    "g4. e8 c2 | r1 | g4. e8 c2 | r1 | g4. e8 c4 r4 | g4. a8 b4 >c4< | >c2< r2 | r1 |";
const char doomo_mml_number_ten_1[] =
    "@3 o6 v9 r1 | e8 r8 r4 r2 | r1 | e8 r8 r4 r2 | r1 | r1 | r2 e4 c4 | e2 c2 |";
const char doomo_mml_number_ten_2[] =
    "@2 o3 v9 c1 | c1 | c1 | c1 | c1 | g1 | c2 <g2> | c1 |";
const char doomo_mml_number_ten_3[] =
    "v6 [K2 r2]6 C2 r2 K2 r2";
