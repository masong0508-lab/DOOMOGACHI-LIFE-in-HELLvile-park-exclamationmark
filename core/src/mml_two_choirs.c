/* mml_two_choirs.c - SONG_TWO_CHOIRS, "Two Choirs" (owner: S5;
 * docs/design/story/SPEC.md R16, C15; story_final.md sec.14 S179;
 * docs/design/music/SPEC.md sec.15.2: "TWO_CHOIRS = FUZZ/NOX canon").
 *
 * Cantor Coo's disco hymn for the Bone Cathedral: the Hillvile choir sings
 * FUZZ's head in F major on the organ (f a g f | b- a g a) over a disco
 * beat; the Hellvile choir answers in F minor at dirge speed, NOX's mirror
 * (f d- e- f) on low brass, while a bell drops down the minor scale; then
 * the two sing FUZZ as a canon a bar apart, and the shared chorus closes
 * on the plagal amen (B-flat to F). 4/4, 16 bars of 192 units on every
 * channel (audio_song_check needs the four channel lengths equal), looping.
 */
const char doomo_mml_two_choirs_0[] =
    "t116 @11 o5 v11 l4"
    "f a g f | b- a8 g8 a2 | a >c< b- a | g2 f2 |"
    "@3 v9 r2 >c4< r4 | r2 b-4 r4 | r2 a-4 r4 | r2 g4 r4 |"
    "@11 v11 f a g f | b- a8 g8 a2 | a >c< b- a | g2 f2 |"
    ">c c< b- a | g f e f | b-2 a2 | g2 f2 |";
const char doomo_mml_two_choirs_1[] =
    "@4 o4 v6 f1 | b-2 f2 | f1 | c2 f2 |"
    "@20 v11 f2 d-2 | e-2 f2 | a-2 g2 | f1 |"
    "r1 | f4 a4 g4 f4 | b-4 a8 g8 a2 | a4 >c4< b-4 a4 |"
    "@4 o5 v8 a4 a4 g4 f4 | e4 d4 c4 d4 | d2 c2 | <b-2 a2> |";
const char doomo_mml_two_choirs_2[] =
    "@2 o3 v10 l8"
    "[f >f<]4 | [b- >b-<]2 [f >f<]2 | [f >f<]4 | [c >c<]2 [f >f<]2 |"
    "f2 d-2 | e-2 f2 | a-2 c2 | f1 |"
    "[f >f<]4 | [b- >b-<]2 [f >f<]2 | [f >f<]4 | [c >c<]2 [f >f<]2 |"
    "[f >f<]4 | [c >c<]2 [f >f<]2 | [b- >b-<]2 [f >f<]2 | b-2 f2 |";
const char doomo_mml_two_choirs_3[] =
    "v9 [K8 O8 S8 O8 K8 O8 S8 O8]4 [K2 S2]3 T4 T4 T8 T8 S4"
    "[K8 O8 S8 O8 K8 O8 S8 O8]7 C2 K4 r4";
