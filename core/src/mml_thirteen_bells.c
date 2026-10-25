/* mml_thirteen_bells.c - SONG_THIRTEEN_BELLS, "Thirteen Bells" (owner: S5;
 * docs/design/story/SPEC.md R16, C15; story_final.md sec.14 S211;
 * docs/design/music/SPEC.md sec.15.2: "THIRTEEN_BELLS = FM bells -> F").
 *
 * Midnight in the bell tower. Twelve bells ring backwards, down the F-major
 * scale over a low F pedal; the thirteenth is a B natural, a tritone away,
 * on the spooky wobble, and then nothing at all (the rope moving by
 * itself). You hum the lullaby's first line (S148's, in F: f g f | c'),
 * a clock ticks under it, and the tower answers by ringing forwards: twelve
 * up the scale and a thirteenth on F, on purpose. 4/4, 16 bars of 192
 * units on every channel (audio_song_check needs the four channel lengths
 * equal), looping.
 */
const char doomo_mml_thirteen_bells_0[] =
    "t80 @3 o6 v12"
    "f2 e2 | d2 c2 | <b-2 a2 | g2 f2 | e2 d2 | c2 <b-2 | b1 | r1 |"
    "r1 | r1 | r1 | r1 |"
    "f4 g4 a4 b-4 | >c4 d4 e4 f4 | g4 a4 b-4 >c4< | f1 |";
const char doomo_mml_thirteen_bells_1[] =
    "r1 | r1 | r1 | r1 | r1 | r1 | @10 o4 v8 b1 | r1 |"
    "@15 o5 v11 f2 g4 f4 | >c1< | b-2 a4 g4 | a2. f4 |"
    "@4 o4 v6 f1 | c1 | <b-2> c2 | f1 |";
const char doomo_mml_thirteen_bells_2[] =
    "@2 o2 v9 f1 | f1 | f1 | f1 | f1 | f1 | b1 | r1 |"
    "f1 | f1 | b-1 | f2 c2 | f1 | c1 | b-2 c2 | f1 |";
const char doomo_mml_thirteen_bells_3[] =
    "v4 [r1]8 [H4 H4 H4 H4]4 [K4 H4 K4 H4]3 C1";
