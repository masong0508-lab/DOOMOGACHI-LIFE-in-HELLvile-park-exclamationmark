/* mg_themes.c - musgen's reference themes (owner: MU1T; verbatim from the
 * prototype - they define the fingerprint, so never edit them): the HOOK (HaHaland), the Invasion
 * RIFF, HOME (Dream House waltz) and TITLE (imported from SONG_TITLE's
 * first two bars and its harmony channel). Degrees are from the tonic of the
 * theme's mode; durations are 16th steps. */
#include "mg.h"

#define N(d, a, len) { (int8_t)(d), (int8_t)(a), (uint8_t)(len), 0 }
#define NR(len)      { 0, 0, (uint8_t)(len), GNF_REST }

/* HOOK, C major: | e e g e c'4 g4 | a a g e d4 e4 |  (direction.md 2.1) */
static const MgNote hook_n[] = {
    N(2, 0, 2), N(2, 0, 2), N(4, 0, 2), N(2, 0, 2), N(7, 0, 4), N(4, 0, 4),
    N(5, 0, 2), N(5, 0, 2), N(4, 0, 2), N(2, 0, 2), N(1, 0, 4), N(2, 0, 4),
};
static const MgMotif hook_m[] = { { hook_n, 12, 5, 2, 0 } };
static const uint8_t hook_anchor[] = { AC(0, 0, GCQ_MAJ), AC(5, 0, GCQ_MIN), AC(3, 0, GCQ_MAJ), AC(4, 0, GCQ_MAJ) };
const MgTheme mg_theme_hook = {
    "HOOK (HaHaland Day)", 0, GMODE_MAJOR, 124, 16, 1, 64, 88, 3,
    1, hook_m, 4, hook_anchor, 0, GCAD_PAC
};

/* Invasion, E minor. Lead: the shared cell E-E-G, then the tritone.
 * | e e g e b-4 a g | f+ g a b a4 g4 |  Riff (bass): the trailer's RIFF_A1. */
static const MgNote riff_lead_n[] = {
    N(0, 0, 2), N(0, 0, 2), N(2, 0, 2), N(0, 0, 2), N(4, -1, 4), N(3, 0, 2), N(2, 0, 2),
    N(1, 0, 2), N(2, 0, 2), N(3, 0, 2), N(4, 0, 2), N(3, 0, 4), N(2, 0, 4),
};
static const MgNote riff_bass_n[] = {
    N(0, 0, 1), N(0, 0, 1), N(2, 0, 2), N(0, 0, 1), N(0, 0, 1), N(4, -1, 2),
    N(0, 0, 1), N(0, 0, 1), N(3, 0, 2), N(0, 0, 1), N(0, 0, 1), N(2, 0, 1), N(1, 0, 1),
};
static const MgMotif riff_m[] = { { riff_lead_n, 13, 5, 2, 0 }, { riff_bass_n, 13, 13, 1, 1 } };
static const uint8_t riff_anchor[] = { AC(0, 0, GCQ_POW), AC(0, 0, GCQ_POW), AC(5, 0, GCQ_POW), AC(6, 0, GCQ_POW) };
const MgTheme mg_theme_riff = {
    "RIFF (Hell Invades)", 4, GMODE_MINOR, 168, 16, 4, 64, 88, 3,
    2, riff_m, 4, riff_anchor, 1, GCAD_AEOL
};

/* HOME, C major 3/4 music box: | g4 c'4 e'4 | g'2 e'4 |  (direction.md 2.8) */
static const MgNote home_n[] = { N(4, 0, 4), N(7, 0, 4), N(9, 0, 4), N(11, 0, 8), N(9, 0, 4) };
static const MgMotif home_m[] = { { home_n, 5, 3, 2, 0 } };
static const uint8_t home_anchor[] = { AC(0, 0, GCQ_MAJ), AC(3, 0, GCQ_MAJ), AC(4, 0, GCQ_MAJ), AC(0, 0, GCQ_MAJ) };
const MgTheme mg_theme_home = {
    "HOME (Dream House waltz)", 0, GMODE_MAJOR, 84, 12, 0, 67, 96, 2,
    1, home_m, 4, home_anchor, 0, GCAD_PAC
};

/* TITLE (SONG_TITLE, songs.c): "e g >c< g a4 g e | f a >c< a g4. r |" over
 * the harmony channel's C | Am | F | G. Imported by the migration tool. */
static const MgNote title_n[] = {
    N(2, 0, 2), N(4, 0, 2), N(7, 0, 2), N(4, 0, 2), N(5, 0, 4), N(4, 0, 2), N(2, 0, 2),
    N(3, 0, 2), N(5, 0, 2), N(7, 0, 2), N(5, 0, 2), N(4, 0, 6), NR(2),
};
static const MgMotif title_m[] = { { title_n, 13, 4, 2, 0 } };
static const uint8_t title_anchor[] = { AC(0, 0, GCQ_MAJ), AC(5, 0, GCQ_MIN), AC(3, 0, GCQ_MAJ), AC(4, 0, GCQ_MAJ) };
const MgTheme mg_theme_title = {
    "TITLE (Doomogachi Life!)", 0, GMODE_MAJOR, 140, 16, 1, 64, 88, 3,
    1, title_m, 4, title_anchor, 0, GCAD_PAC
};
