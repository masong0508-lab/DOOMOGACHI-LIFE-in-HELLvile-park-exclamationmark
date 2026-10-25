/* mml_double_act.c - SONG_DOUBLE_ACT, "Double Act" (owner: S5;
 * docs/design/story/SPEC.md R16, C15; story_final.md sec.14 S156).
 *
 * The comedy tent's soft-shoe for Honkers and Mr. Solemn: C major, swung
 * (dotted eighths), brushes on the kit and a walking bass. Honkers plays
 * the bright lead (@0) in two-bar bits; Mr. Solemn answers on the low pad
 * (@9), one deadpan note at a time; in the middle the act clicks and they
 * play together. The button: Honkers lands the big finish, the band stops,
 * and Solemn's answer arrives a beat and a half late, on its own, with a
 * rimshot (he cannot do timing; it is the funniest thing in the tent).
 * 4/4, 16 bars of 192 units on every channel (audio_song_check needs the
 * four channel lengths equal), looping.
 */
const char doomo_mml_double_act_0[] =
    "t132 @0 o5 v12"
    "c8. e16 g8. a16 g4 e4 | f8. e16 d8. c16 d2 | r1 | r1 |"
    "e8. g16 >c8. d16 c4< a4 | g8. f16 e8. d16 c2 | r1 | r1 |"
    "c8. e16 g8. a16 g8. e16 c8. e16 | f8. a16 >c8. d16 c4< r4 | g8. f16 e8. d16 e8. f16 g8. e16 | a4 g4 e4 c4 |"
    "c8. e16 g8. a16 g4 e4 | f8. e16 d8. c16 d4 g4 | c4 e4 g4 >c4< | c2 r2 |";
const char doomo_mml_double_act_1[] =
    "@9 o3 v11 r1 | r1 | g2 g2 | c1 |"
    "r1 | r1 | c2 c2 | g1 |"
    "c1 | f1 | g1 | c1 |"
    "c1 | g1 | c1 | r2. c4 |";
const char doomo_mml_double_act_2[] =
    "@2 o3 v10 l4"
    "c e g e | <g b> d <b> | <g b> d <b> | c e g e |"
    "c e g a | g <b> c e | c e g e | <g b> d <b> |"
    "c e g e | f a >c< a | c e g e | f e d <g> |"
    "c e g e | <g b> d g | c e g >c< | c2 r2 |";
const char doomo_mml_double_act_3[] =
    "v7 [K8. H16 S8. H16 K8. H16 S8. H16]15 C2 r4 S4";
