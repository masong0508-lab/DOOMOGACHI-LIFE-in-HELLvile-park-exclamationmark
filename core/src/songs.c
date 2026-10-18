/* songs.c - the instruments and the song book of the 30 Hz chiptune
 * sequencer (owner: story). The MML dialect is described in audio_int.h.
 *
 * Two families, on purpose:
 *   CUTE   title, HaHaland day / night, creator, shop, church, pet,
 *          workshop, festival, credits: bouncy major keys, pulse leads,
 *          arpeggiated chords, triangle bass, light drums.
 *   DOOM   invasion, hell breach, boss: saw power-chord riffs, palm-muted
 *          chugs (short gate), gallops, double kicks, harmonic minor leads.
 *   In between: Hellvile (spooky-cute harmonic minor), exorcism (tolling
 *   bells at EXORCISM_BPM), the vape lounge (too smooth), the Wall (dread).
 *
 * Channels: 0 lead, 1 harmony (also the voice babble channel), 2 bass,
 * 3 drums (K kick, S snare, H hat, O open hat, C crash, T tom, P clap).
 * Every channel of a song has the same length (test_audio checks it), so
 * looping keeps them locked together. Bars are 192 units (4/4).
 *
 * The comments of the form SONG / chN / END mark the songs for the
 * composer's length checker; keep them.
 */
#include "audio_int.h"

/* ---- instruments --------------------------------------------------------- */
/*  wave            atk dec sus rel  vibD vibS vibDly arp          sweep gate lvl */
const AudioInst audio_insts[] = {
    { WAVE_PULSE25,   0, 10,  9, 14,   3,  20, 10,   ARP_NONE,      0, 7, 14 }, /* @0  cute lead      */
    { WAVE_PULSE12,   0,  8,  7, 14,   0,   0,  0,   ARP_NONE,      0, 6, 11 }, /* @1  soft harmony   */
    { WAVE_TRIANGLE,  0,  2, 13, 40,   0,   0,  0,   ARP_NONE,      0, 6, 15 }, /* @2  triangle bass  */
    { WAVE_PULSE50,   0, 20,  0, 20,   0,   0,  0,   ARP_NONE,      0, 8, 13 }, /* @3  bell / pluck   */
    { WAVE_PULSE25,   0,  3,  9, 16,   0,   0,  0,   ARP_MAJOR,     0, 7, 12 }, /* @4  major chord    */
    { WAVE_PULSE25,   0,  3,  9, 16,   0,   0,  0,   ARP_MINOR,     0, 7, 12 }, /* @5  minor chord    */
    { WAVE_SAW,       0,  2, 13, 20,   6,  28,  6,   ARP_NONE,      0, 8, 15 }, /* @6  metal lead     */
    { WAVE_SAW,       0,  4, 12, 30,   0,   0,  0,   ARP_POWER,     0, 7, 15 }, /* @7  power chord    */
    { WAVE_SAW,       0,  2, 14, 40,   0,   0,  0,   ARP_NONE,      0, 7, 15 }, /* @8  saw bass       */
    { WAVE_TRIANGLE,  5,  0, 12,  6,   3,  14, 12,   ARP_NONE,      0, 8, 12 }, /* @9  soft pad       */
    { WAVE_PULSE12,   2,  2, 11,  8,  10,  18,  0,   ARP_NONE,      0, 7, 11 }, /* @10 spooky wobble  */
    { WAVE_PULSE50,   2,  1, 12,  6,   2,  12, 16,   ARP_NONE,      0, 8, 12 }, /* @11 church organ   */
    { WAVE_PULSE12,   0, 12,  2, 10,   0,   0,  0,   ARP_NONE,      0, 8, 12 }, /* @12 music box      */
    { WAVE_PULSE12,   0,  6,  6, 20,   0,   0,  0,   ARP_NONE,     24, 6, 12 }, /* @13 zap            */
    { WAVE_PULSE50,   0,  0, 13,255,   0,   0,  0,   ARP_NONE,      0, 6, 12 }, /* @14 LCD beep       */
    { WAVE_TRIANGLE,  3,  1, 12,  8,   5,  18, 12,   ARP_NONE,      0, 7, 14 }, /* @15 flute          */
    { WAVE_PULSE25,   0,  4,  8, 16,   0,   0,  0,   ARP_OCTAVE,    0, 6, 12 }, /* @16 octave arp     */
    { WAVE_PULSE25,   0,  2, 10, 12,   0,   0,  0,   ARP_TRILL,     0, 8, 12 }, /* @17 trill          */
    { WAVE_TRIANGLE,  0, 10,  8, 20,   0,   0,  0,   ARP_NONE,     -2, 6, 15 }, /* @18 slap bass      */
    { WAVE_SAW,       0, 30,  4, 40,   0,   0,  0,   ARP_POWER,     0, 4, 15 }, /* @19 chug           */
    { WAVE_PULSE50,   1,  2, 12, 12,   4,  22,  8,   ARP_NONE,      0, 7, 14 }, /* @20 brass          */
    { WAVE_PULSE25,   0,  3,  9, 16,   0,   0,  0,   ARP_DIM,       0, 7, 12 }, /* @21 dim chord      */
    { WAVE_PULSE25,   0,  3,  9, 16,   0,   0,  0,   ARP_SUS4,      0, 7, 12 }, /* @22 sus chord      */
    { WAVE_PULSE12,   0,  3,  9, 16,   0,   0,  0,   ARP_MAJ7,      0, 7, 11 }, /* @23 maj7 chord     */
};
const int audio_inst_count = (int)(sizeof(audio_insts) / sizeof(audio_insts[0]));

/* ---- the songs ------------------------------------------------------------- */
/* --- S0: the story SPEC's 9 songs (R16, C15) ---
 * Each song is four channel strings defined by its owner in its own
 * core/src/mml_<name>.c (strong). Until then core/src/stubs.c gives weak
 * silent placeholders. Incomplete arrays: their addresses are constants,
 * legal in this const initializer. Nobody edits these rows after S0. */
#define SONG_EXT(n) extern const char doomo_mml_##n##_0[], doomo_mml_##n##_1[], \
    doomo_mml_##n##_2[], doomo_mml_##n##_3[]
SONG_EXT(mingle);
SONG_EXT(wall_lullaby);
SONG_EXT(two_choirs);
SONG_EXT(boat_floats);
SONG_EXT(thirteen_bells);
SONG_EXT(hell_lullaby);
SONG_EXT(double_act);
SONG_EXT(heehee_jingle);
SONG_EXT(number_ten);
#define SONG_ROW(n, title) { title, ASONG_LOOP, { doomo_mml_##n##_0, doomo_mml_##n##_1, \
    doomo_mml_##n##_2, doomo_mml_##n##_3 } }
/* --- end S0 --- */

const AudioSong audio_songs[SONG_COUNT] = {
    /* SONG_NONE */
    { "Silence", 0, { NULL, NULL, NULL, NULL } },

    /*SONG:TITLE*/
    { "Doomogachi Life!", ASONG_LOOP, {
      /*ch0*/ "t140 @0 o5 l8 v13"
              "e g >c< g a4 g e | f a >c< a g4. r | a >c d c< a4 g f | e d e g g4 r4 |"
              "e g >c< g a4 >c< a | >d c< b a g4 e g | a g f e d4 e f | e2 c4 r4 |",
      /*ch1*/ "o4 v9 @4 [r8 c8]4 | @5 < [r8 a8]4 > | @4 [r8 f8]4 | [r8 g8]4 |"
              "[r8 c8]4 | [r8 g8]2 @5 [r8 e8]2 | @4 [r8 f8]2 @5 [r8 d8]2 | @4 [r8 c8]2 c4 r4 |",
      /*ch2*/ "@2 o3 l8 v13 [c>c<]4 | < [a>a<]4 > | [f>f<]4 | [g>g<]4 |"
              "[c>c<]4 | [g>g<]2 [e>e<]2 | [f>f<]2 [d>d<]2 | c4 g4 c4 r4 |",
      /*ch3*/ "v11 [K8 H8 S8 H8 K8 K8 S8 H8]7 K8 H8 S8 H8 K4 C4 |" } },
    /*END*/

    /*SONG:HAHALAND_DAY*/
    { "HaHaland Day", ASONG_LOOP, {
      /*ch0*/ "t124 @0 o5 l8 v13"
              "c4 <a> c f4 e f | a4 g f d4. r | d4 f d b-4 a g | a4 g4 e4 r4 |"
              "c4 <a> c f4 a g | e4 c e a4 g e | f d f b-4 a g f | e2 r4 c4 |"
              "d4 f a >d4 c< a | b-4 a g f4 d4 | c4 f a >c4< a f | g2. r4 |"
              "f4 g a b-4 a g | a4 g f e4 g4 | f4 a >c f4 c4< | f2 r2 |",
      /*ch1*/ "o4 v8 @4 [r8 f8]4 | @5 [r8 d8]4 | @4 [r8 <b-8>]4 | [r8 c8]4 |"
              "[r8 f8]4 | @5 [r8 <a8>]4 | @4 [r8 <b-8>]4 | [r8 c8]4 |"
              "@5 [r8 d8]4 | @4 [r8 <b-8>]4 | [r8 f8]4 | [r8 c8]4 |"
              "[r8 <b-8>]4 | [r8 c8]4 | [r8 f8]4 | f4 r4 r2 |",
      /*ch2*/ "@2 o3 l4 v13 f >c f c< | d a >d< a | <b-> f b- f | c g >c< g |"
              "f >c f c< | <a> e a e | <b-> f b- f | c g >c< g |"
              "d a >d< a | <b-> f b- f | f >c f c< | c g >c< g |"
              "<b-> f b- f | c g >c< g | f >c f c< | f2 r2 |",
      /*ch3*/ "v10 [K4 H8 H8 S4 H8 H8 | K4 H8 H8 S4 H8 H8 | K4 H8 H8 S4 H8 H8 |"
              "K4 H8 K8 S8 S8 T8 T8 |]4" } },
    /*END*/

    /*SONG:HAHALAND_NIGHT*/
    { "HaHaland Night", ASONG_LOOP, {
      /*ch0*/ "t80 @12 o5 v12 d4 b8 a8 g2 | e4 g8 f+8 e2 | c4 e8 d8 c4 e4 | d2. r4 |"
              "d4 b8 a8 g4 b4 | >c4< b8 a8 g4 e4 | c4 e4 d4 f+4 | g2. r4 |",
      /*ch1*/ "@3 o3 v8 l12 [g b >d< b]3 | [e g b g]3 | [c e g e]3 | [d f+ a f+]3 |"
              "[g b >d< b]3 | [e g b g]3 | c e g e g e d f+ a f+ a f+ | [g b >d< b]3 |",
      /*ch2*/ "@9 o2 v12 g1 | e1 | >c1 | d1< | g1 | e1 | >c2 d2< | g1 |",
      /*ch3*/ "v4 [r4 H8 H8]16" } },
    /*END*/

    /*SONG:CREATOR*/
    { "Make a Friend!", ASONG_LOOP, {
      /*ch0*/ "t132 @0 o5 l8 v13 q5"
              "d f+ a >d c+< a f+ e | d4 f+ e d4 <b4> | g a b g >d4< b4 | a g+ a b a4 r4 |"
              "d f+ a >d c+< a f+ e | f+4 a4 >c+4< a4 | g b >d< b a >c+ e c+< | d4 f+4 d4 r4 |",
      /*ch1*/ "o4 v8 @4 d8 r8 d8 d8 r8 d8 d8 r8 | @5 < b8 r8 b8 b8 r8 b8 b8 r8 > |"
              "@4 < g8 r8 g8 g8 r8 g8 g8 r8 > | < a8 r8 a8 a8 r8 a8 a8 r8 > |"
              "d8 r8 d8 d8 r8 d8 d8 r8 | @5 < f+8 r8 f+8 f+8 r8 f+8 f+8 r8 > |"
              "@4 < g8 r8 g8 g8 a8 r8 a8 a8 > | d8 r8 d8 d8 d4 r4 |",
      /*ch2*/ "@18 o3 v13 d4 >d8< d8 r8 d8 >d4< | <b4 >b8< b8 r8 b8 >b4> |"
              "<g4 >g8< g8 r8 g8 >g4> | <a4 >a8< a8 r8 a8 >a4> |"
              "d4 >d8< d8 r8 d8 >d4< | <f+4 >f+8< f+8 r8 f+8 >f+4> |"
              "<g4 >g8< a4 >a8< a8 r8> | d4 a4 d4 r4 |",
      /*ch3*/ "v11 [K8 H8 S8 K8 H8 K8 S8 H8]7 K8 S8 S8 S8 T8 T8 C4 |" } },
    /*END*/

    /*SONG:SHOP*/
    { "Shop Hop", ASONG_LOOP, {
      /*ch0*/ "t116 @0 o5 v12"
              "f8. a16 >c8.< a16 g4 f4 | f8. e16 d8. <b16> d4 g4 | e2 g4 b4 | a4. g8 e4 c+4 |"
              "d8. f16 a8. >c16 d4< a4 | g8. f16 e8. d16 f4 <b4> | c2 e8 g8 >c4< | r8 g8 e8 c8 c4 r4 |",
      /*ch1*/ "v8 @5 o4 d1 | @4 o3 g1 | @23 o4 c1 | @4 o3 a1 | @5 o4 d1 | @4 o3 g1 | @23 o4 c1 | @4 o4 c2. r4 |",
      /*ch2*/ "@2 v13 l4 o3 d e f a | o3 g f e d | o3 c e g e | o2 a o3 c+ e g |"
              "o3 d e f a | o3 g f e d | o3 c e g e | o3 c g c r |",
      /*ch3*/ "v9 [K8 H16 H16 S8 H8 K8 H16 H16 S8 H8]8" } },
    /*END*/

    /*SONG:CHURCH*/
    { "St. Cuddles Hymn", ASONG_LOOP, {
      /*ch0*/ "t72 @11 o5 v12 l4 f a g f | b- a8 g8 a2 | >c< a b- g | f2. r |"
              "a >c d c< | b- a g2 | f a g e | f2. r |",
      /*ch1*/ "@11 o4 v9 l4 c f e c | d c8 c8 c2 | f f f e | c2. r |"
              "f a b- a | d f e2 | c f d c | c2. r |",
      /*ch2*/ "@2 o3 v12 f2 c2 | <b-2> c2 | f2 <g4> c4 | f2. r4 |"
              "f2 <b-2> | <g2> c2 | <a2 b-4> c4 | f2. r4 |",
      /*ch3*/ NULL } },
    /*END*/

    /*SONG:PET*/
    { "Doomogachi Beeps", ASONG_LOOP, {
      /*ch0*/ "t150 @14 o5 l8 v12"
              "c e g >c< g e c r | d f a f d4 r4 | e g >c e d c< b g | c4 g4 c4 r4 |"
              "e g a g e c d4 | f a b a f d e4 | g4 e4 >c4< g4 | c4 e8 d8 c4 r4 |",
      /*ch1*/ "@14 o4 v7 [c8 r8 g8 r8]2 | [d8 r8 a8 r8]2 | [e8 r8 g8 r8]2 | [c8 r8 g8 r8]2 |"
              "[c8 r8 g8 r8]2 | [d8 r8 g8 r8]2 | [e8 r8 g8 r8]2 | c8 r8 g8 r8 c4 r4 |",
      /*ch2*/ "@2 o3 v12 q4 l4 c g c g | d a d a | c g c g | c g c r |"
              "c g c g | d g <b> g | c g e g | c g c r |",
      /*ch3*/ "v8 [K8 H8 H8 H8]16" } },
    /*END*/

    /*SONG:HELLVILE*/
    { "Hellvile After Dark", ASONG_LOOP, {
      /*ch0*/ "t108 @10 o5 v12"
              "d8 f8 a8 >d8 c+4< a4 | b-8 a8 g8 e8 c+4 e4 | d8 f8 a8 >d8 f4 e4< | d8 c+8 d8 e8 f4 e4 |"
              "d8 f8 a8 >d8 c+4< a4 | b-8 >d8 f8 d8< b-4 f4 | g8 b-8 a8 g8 f4 e4 | d2 r8 <a8> d4 |",
      /*ch1*/ "o4 v8 @5 [r8 d8]4 | @4 [r8 <a8>]4 | @5 [r8 d8]4 | [r8 <g8>]2 @4 [r8 <a8>]2 |"
              "@5 [r8 d8]4 | @4 [r8 <b-8>]4 | @5 [r8 <g8>]2 @4 [r8 <a8>]2 | @5 [r8 d8]2 d4 r4 |",
      /*ch2*/ "@2 o3 v13 d4. d8 <a4. a8> | <a4. a8> e4. e8 | d4. d8 <a4. a8> | <g4 g4 a4 a4> |"
              "d4. d8 <a4. a8> | <b-4. b-8> f4. f8 | <g4 g4 a4 a4> | d4. d8 d4 r4 |",
      /*ch3*/ "v10 [K4 P8 K8 r4 P4]7 K4 P8 K8 T8 T8 P4 |" } },
    /*END*/

    /*SONG:EXORCISM*/
    { "The Ritual", ASONG_LOOP, {
      /*ch0*/ "t120 @15 o5 v13 l4 e e g f+ | e2 <b2> | c e d c | <b2> d+2 |"
              "e g b a | a >c< b a | f+ a g f+ | e2. r |",
      /*ch1*/ "@3 o4 v10 l4 [e]4 | [e]4 | [c]4 | <[b]4> | [e]4 | <[a]4> | <[b]4> | e e e r |",
      /*ch2*/ "@8 o2 v11 l8 [e e >e< e]2 | [e e >e< e]2 | >[c c >c< c]2< | [b b >b< b]2 |"
              "[e e >e< e]2 | [a a >a< a]2 | [b b >b< b]2 | e e e e e4 r4 |",
      /*ch3*/ "v11 [K8 K8 r4 K8 K8 r4 | K8 K8 r4 K8 K8 r4 | K8 K8 r4 K8 K8 r4 | K8 K8 T8 T8 K8 K8 T8 C8 |]2" } },
    /*END*/

    /*SONG:WORKSHOP*/
    { "Rivet's Workshop", ASONG_LOOP, {
      /*ch0*/ "t110 @0 o5 v13"
              "g8 r16 g16 b-8 >c8< r8 d8 f8 g8 | f8 r16 f16 g8 f8 d4 r4 | g8 r16 g16 b-8 >c8< r8 d8 f8 g8 | b-8 a8 g8 f8 g4 r4 |"
              ">c8 r16 c16< b-8 g8 f8 g8 r8 d8 | f8 g8 b-8 >c8 d4< r4 | b-8 g8 f8 d8 f8 g8 r4 | g4 r8 f8 g4 r4 |",
      /*ch1*/ "@3 o6 v6 [r8 g8]4 | [r8 f8]4 | [r8 g8]4 | [r8 g8]4 | [r8 g8]4 | [r8 f8]4 | [r8 g8]4 | [r8 g8]2 g4 r4 |",
      /*ch2*/ "@18 o2 v13 l8 g r >g< g r f g r | f r >f< f r e- f r | g r >g< g r f g r | g r >g< g r f g r |"
              "g r >g< g r f g r | f r >f< f r e- f r | g r >g< g r f g r | g4 r f g4 r4 |",
      /*ch3*/ "v11 [K8 H8 S8 H16 K16 H8 K8 S8 H8 | K8 H8 S8 H16 K16 H8 K8 S8 H8 | K8 H8 S8 H16 K16 H8 K8 S8 H8 | K8 T8 S8 T8 K8 T8 S8 C8 |]2" } },
    /*END*/

    /*SONG:INVASION*/
    { "Hell Invades", ASONG_LOOP, {
      /*ch0*/ "t168 @7 o3 v13"
              "q3 e16 e16 e8 q7 g8 q3 e16 e16 e8 q7 a+8 a8 g8 | q3 e16 e16 e8 q7 g8 q3 e16 e16 e8 q7 b8 a8 f+8 |"
              "q3 e16 e16 e8 q7 g8 q3 e16 e16 e8 q7 a+8 a8 g8 | q7 c4 q3 c16 c16 c8 q7 d4 q3 d16 d16 d8 |"
              "q3 e16 e16 e8 q7 g8 q3 e16 e16 e8 q7 a+8 a8 g8 | q3 e16 e16 e8 q7 g8 q3 e16 e16 e8 q7 b8 a8 f+8 |"
              "q7 c4 q3 c16 c16 c8 q7 d4 q3 d16 d16 d8 | q7 e4 q3 e16 e16 e8 q7 f+4 g8 f+8 |",
      /*ch1*/ "@6 o5 v12 e2 g4 a4 | b2 a4 g4 | f+2 g4 f+4 | e1 | b2 >d4 e4 | f+2 e4 d4< | e4 d4 c4 d4 | e1 |",
      /*ch2*/ "@8 o2 v13"
              "e16 e16 e8 g8 e16 e16 e8 a+8 a8 g8 | e16 e16 e8 g8 e16 e16 e8 b8 a8 f+8 |"
              "e16 e16 e8 g8 e16 e16 e8 a+8 a8 g8 | c4 c16 c16 c8 d4 d16 d16 d8 |"
              "e16 e16 e8 g8 e16 e16 e8 a+8 a8 g8 | e16 e16 e8 g8 e16 e16 e8 b8 a8 f+8 |"
              "c4 c16 c16 c8 d4 d16 d16 d8 | e4 e16 e16 e8 f+4 g8 f+8 |",
      /*ch3*/ "v12 C8 K16 K16 S8 K16 K16 K16 K16 H8 S8 K16 K16 | [K16 K16 H8 S8 K16 K16]2 | [K16 K16 H8 S8 K16 K16]2 | [K16 K16 H8 S8 K16 K16]2 |"
              "C8 K16 K16 S8 K16 K16 K16 K16 H8 S8 K16 K16 | [K16 K16 H8 S8 K16 K16]2 | [K16 K16 H8 S8 K16 K16]2 | K16 K16 S16 S16 T16 T16 T16 T16 C4 S8 S8 |" } },
    /*END*/

    /*SONG:HELL_BREACH*/
    { "Hell Breach", ASONG_LOOP, {
      /*ch0*/ "t150 @7 o3 v13"
              "q3 e8 e8 q8 g8 q3 e8 e8 q8 a8 q3 e8 q8 b8 | q3 e8 e8 q8 g8 q3 e8 e8 q8 a+8 a8 g8 |"
              "q3 e8 e8 q8 g8 q3 e8 e8 q8 a8 q3 e8 q8 b8 | q3 e8 e8 q8 g8 q3 e8 e8 q8 a+8 a8 g8 |"
              "q3 c8 c8 q8 e8 q3 c8 c8 q8 f+8 q3 c8 q8 g8 | q3 c8 c8 q8 e8 q3 c8 c8 q8 f+8 q3 c8 q8 g8 |"
              "q3 e8 e8 q8 g8 q3 e8 e8 q8 a8 q3 e8 q8 b8 | q8 d4 c+4 c4 <b4> |",
      /*ch1*/ "@6 o4 v11 r2 b4. a8 | g2 e2 | r2 b4. >d8< | >e2 d4< b4 | o5 c2 e4 d4 | o5 c4 o4 b4 a4 g4 | o4 a2 b2 | o4 e1 |",
      /*ch2*/ "@8 o2 v13 l8"
              "e e g e e a e b | e e g e e a+ a g | e e g e e a e b | e e g e e a+ a g |"
              "c c e c c f+ c g | c c e c c f+ c g | e e g e e a e b | d4 c+4 c4 <b4> |",
      /*ch3*/ "v12 C8 H8 S8 H8 K8 K8 S8 H8 | [K8 H8 S8 H8 K8 K8 S8 H8]6 | K8 K8 S8 K8 T8 T8 C4 |" } },
    /*END*/

    /*SONG:BOSS*/
    { "Lord Vapula", ASONG_LOOP, {
      /*ch0*/ "t184 @7 o3 v13"
              "[q3 e8 q8 e16 e16]4 | [q3 c8 q8 c16 c16]2 [q3 d8 q8 d16 d16]2 |"
              "[q3 e8 q8 e16 e16]4 | [q3 a8 q8 a16 a16]2 [q3 b8 q8 b16 b16]2 |"
              "[q3 e8 q8 e16 e16]4 | [q3 c8 q8 c16 c16]2 [q3 d8 q8 d16 d16]2 |"
              "[q3 a8 q8 a16 a16]2 [q3 b8 q8 b16 b16]2 | q8 c4 <b4 a+4 b4> |",
      /*ch1*/ "@6 o5 v12 e16 f+16 g16 a16 b8 a8 g8 f+8 e4 | c8 e8 g8 >c8< b4 g4 |"
              "e16 f+16 g16 a16 b8 a8 g8 f+8 e4 | a8 >c8< b8 a8 b4 d+4 |"
              "e8 g8 b8 >e8 d+4< b4 | >c8< b8 a8 g8 f+4 e4 | a8 g8 f+8 e8 d+4 f+4 | e4 d+4 e2 |",
      /*ch2*/ "@8 o2 v13 [e8 e16 e16]4 | [c8 c16 c16]2 [d8 d16 d16]2 | [e8 e16 e16]4 | [a8 a16 a16]2 [b8 b16 b16]2 |"
              "[e8 e16 e16]4 | [c8 c16 c16]2 [d8 d16 d16]2 | [a8 a16 a16]2 [b8 b16 b16]2 | c4 <b4 a+4 b4> |",
      /*ch3*/ "v12 C8 K16 K16 S8 K16 K16 [K16 K16 S8]2 | [K16 K16 S8]12 | [K16 K16 S8]4 | [K16 K16 S8]4 | [K16 K16 S8]4 |"
              "S16 S16 S16 S16 T16 T16 T16 T16 C4 C4 |" } },
    /*END*/

    /*SONG:VICTORY*/
    { "Victory!", ASONG_ONCE, {
      /*ch0*/ "t150 @20 o5 v14 c12 c12 c12 e4 g4 >c4< | a12 a12 a12 >c4 d4 e4< | f12 g12 a12 b4 >d4 c4< | >c2.< r4 |",
      /*ch1*/ "@4 o4 v10 c1 | f1 | g1 | c2. r4 |",
      /*ch2*/ "@2 o3 v13 l4 c c c c | f f f f | g g g g | c2. r4 |",
      /*ch3*/ "v12 K4 S4 K4 S4 | K4 S4 K4 S4 | K8 K8 S8 S8 T8 T8 T8 T8 | C2. r4 |" } },
    /*END*/

    /*SONG:SAD*/
    { "Goodbye, Friend", ASONG_LOOP, {
      /*ch0*/ "t66 @15 o5 v12 e2 d4 c4 | <b2 a2> | c4 d4 e4 a4 | g+2. r4 |"
              "a2 g4 f4 | e2 d4 c4 | d4 c4 <b4 g+4> | <a2.> r4 |",
      /*ch1*/ "@12 o3 v9 l8 [a >c e c<]2 | [e g b g]2 | [f a >c< a]2 | [e g+ b g+]2 |"
              "[f a >c< a]2 | >[c e g e]2< | d f a f e g+ b g+ | [a >c e c<]2 |",
      /*ch2*/ "@9 o2 v12 a1 | e1 | f1 | e1 | f1 | >c1< | d2 e2 | a1 |",
      /*ch3*/ NULL } },
    /*END*/

    /*SONG:FESTIVAL*/
    { "Hellvile Park Festival", ASONG_LOOP, {
      /*ch0*/ "t144 @0 o5 v13"
              "f+8 a8 >d8< a8 b4 a4 | g8 f+8 e8 c+8 e4 <a4> | d8 f+8 b8 f+8 d4 <b4> | d8 e8 g8 b8 a4 g4 |"
              "f+8 a8 >d8< a8 b4 >d4< | c+8 e8 a8 e8 c+4 <a4> | b8 a8 g8 b8 a4 g4 | e8 f+8 g8 a8 e4 c+4 |"
              "@20 d4 f+4 b4 a4 | g4 b4 >d4 c+4< | d4 f+4 a4 >d4< | c+4 e4 a4 g4 |"
              "b8 >d8< b8 g8 b4 a4 | a8 >c+8< a8 e8 a4 g4 | f+8 a8 >d8 f+8 e4 c+4< | d2 >d4< r4 |",
      /*ch1*/ "o4 v8 @4 [r8 d8]4 | [r8 <a8>]4 | @5 [r8 <b8>]4 | @4 [r8 <g8>]4 |"
              "[r8 d8]4 | [r8 <a8>]4 | [r8 <g8>]4 | [r8 <a8>]4 |"
              "@7 o3 v10 q4 b8 b8 r8 b8 b8 r8 b4 | g8 g8 r8 g8 g8 r8 g4 | >d8 d8 r8 d8 d8 r8 d4< | a8 a8 r8 a8 a8 r8 a4 |"
              "g8 g8 r8 g8 g8 r8 g4 | a8 a8 r8 a8 a8 r8 a4 | >d8 d8 r8 d8 d8 r8 d4< | >d4< r4 r2 |",
      /*ch2*/ "@2 o3 v13 l4 d a d a | <a> e a e | <b> f+ b f+ | <g> d g d |"
              "d a d a | <a> e a e | <g> d g d | <a> e a e |"
              "@8 l8 [b]8 | [g]8 | [d]8 | [a]8 | [g]8 | [a]8 | [d]8 | d4 r4 r2 |",
      /*ch3*/ "v11 [K8 H8 S8 H8 K8 K8 S8 H8]8 C8 K8 S8 K8 K16 K16 K8 S8 H8 |"
              "[K8 K8 S8 K8 K16 K16 K8 S8 H8]3 C8 K8 S8 K8 K16 K16 K8 S8 H8 |"
              "[K8 K8 S8 K8 K16 K16 K8 S8 H8]2 K8 S8 S8 S8 T8 T8 C4 |" } },
    /*END*/

    /*SONG:CREDITS*/
    { "Everyone Together", ASONG_LOOP, {
      /*ch0*/ "t120 @20 o5 v13 l4"
              "c f a >c< | b- g e c | d f b- >d< | c2 a r |"
              "a f d a | b- g d b- | a8 g8 f8 e8 g b- | a2 f r |"
              "d f b- a | a f c f | g b- >d c< | c2 e g |"
              "a >c f c< | b- >d< b- g | g8 a8 b-8 >c8< e g | f2. r |",
      /*ch1*/ "o4 v8 @4 [f8 r8]4 | [c8 r8]4 | [<b-8> r8]4 | [f8 r8]4 |"
              "@5 [d8 r8]4 | [<g8> r8]4 | @4 [c8 r8]4 | [f8 r8]4 |"
              "[<b-8> r8]4 | [f8 r8]4 | @5 [<g8> r8]4 | @4 [c8 r8]4 |"
              "[f8 r8]4 | [<b-8> r8]4 | [c8 r8]4 | f4 r4 r2 |",
      /*ch2*/ "@2 o3 v13 l4 f c f c | c <g> c <g> | <b-> f <b-> f | f c f c |"
              "d a d a | <g> d <g> d | c <g> c <g> | f c f c |"
              "<b-> f <b-> f | f c f c | <g> d <g> d | c <g> c <g> |"
              "f c f c | <b-> f <b-> f | c <g> c <g> | f2. r |",
      /*ch3*/ "v10 [K8 S16 S16 S8 S8 K8 S16 S16 S8 S8]15 K8 S16 S16 S8 S8 C4 r4 |" } },
    /*END*/

    /*SONG:VAPE_LOUNGE*/
    { "Cloud Nine Lounge", ASONG_LOOP, {
      /*ch0*/ "t88 @15 o5 v11 e4. g8 a2 | f+4 e8 d8 e2 | c4. d8 e8 g8 a4 | b4 a8 f+8 a2 |"
              "a4. g8 e4 c4 | g+4 f4 e2 | a4. b8 >c4 e4< | b4 g+4 e2 |",
      /*ch1*/ "v7 @5 o4 a1 | @4 d1 | @5 a1 | @4 d1 | @23 f1 | @4 e1 | @5 a1 | @4 e1 |",
      /*ch2*/ "@2 o2 v12 a4. >e8 r4< a4 | o3 d4. a8 r4 d4 | o2 a4. >e8 r4< a4 | o3 d4. a8 r4 d4 |"
              "o2 f4. >c8 r4< f4 | o2 e4. b8 r4 e4 | o2 a4. >e8 r4< a4 | o2 e4. b8 r4 e4 |",
      /*ch3*/ "v6 [H8 H16 H16 r8 P8 H8 H16 H16 r8 H8]8" } },
    /*END*/

    /*SONG:OMINOUS*/
    { "The Wall", ASONG_LOOP, {
      /*ch0*/ "t70 @10 o4 v11 r1 | e2 f2 | r1 | e2 a+2 | r1 | e2 f2 | g2 f+2 | e1 |",
      /*ch1*/ "@9 o3 v10 e1 | e1 | e1 | e1 | c1 | c1 | d+1 | e1 |",
      /*ch2*/ "@12 o5 v7 [r4 b8 r8 a+8 r8 r4 | r4 e8 r8 f8 r8 r4 |]4",
      /*ch3*/ "v8 [K4 K8 r8 r2]8" } },
    /*END*/
    /* --- S0: SONG_MINGLE .. SONG_NUMBER_TEN (R16) --- */
    SONG_ROW(mingle, "Mingle"),
    SONG_ROW(wall_lullaby, "Lullaby for the Wall"),
    SONG_ROW(two_choirs, "Two Choirs"),
    SONG_ROW(boat_floats, "Boat Floats"),
    SONG_ROW(thirteen_bells, "Thirteen Bells"),
    SONG_ROW(hell_lullaby, "Hellvile Lullaby"),
    SONG_ROW(double_act, "Double Act"),
    SONG_ROW(heehee_jingle, "Hee. Hee."),
    SONG_ROW(number_ten, "Number Ten"),
    /* --- end S0 --- */
};
