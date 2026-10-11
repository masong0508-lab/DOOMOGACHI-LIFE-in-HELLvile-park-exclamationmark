/* concert_int.h - private glue of the CONCERT HALL + DREAMS expansion
 * (owner: concert). Not part of the public contract; the scenes are
 * SCENE_CONCERT and SCENE_DREAM (scenes.h) and the activities are in
 * doomo_activities_concert (activities.h).
 *
 *   concert_song.c    procedural composer (key, scale, chords, melody,
 *                     bass, drums), the band & choir casting, the
 *                     4-channel plat_tone sequencer with choir harmony and
 *                     voice babble, DoomoSong export (Music Studio) and the
 *                     Hit Parade singles
 *   concert_lyrics.c  song titles + lyrics from templates (name, likes,
 *                     fears, catchphrase, job, friends, crush, rival),
 *                     band names, critics' reviews, syllables
 *   concert_gfx.c     the stage: backdrop, LED wall light show, truss,
 *                     spotlight beams, band + instruments, choir risers,
 *                     crowd, pyro, confetti, karaoke pill
 *   concert_scene.c   SCENE_CONCERT: setup, show, encore, results,
 *                     singles + Hit Parade; the activities table
 *   dream.c           SCENE_DREAM: bedroom peek, vignettes, nightmares
 *   dream_gfx.c       the dream vignettes' drawing
 *   unlocks_concert.c unlock ids 896..943 (+ dream journal bits 944..959)
 *
 * Budgets: no malloc, no floats; .bss well under 3 KB for the module.
 */
#ifndef DOOMO_CONCERT_INT_H
#define DOOMO_CONCERT_INT_H

#include "doomo/doomo.h"

/* ---- scene args ---------------------------------------------------------- */
/* SCENE_CONCERT: arg >= 0 is the lead singer's resident id (the hub picks
 * it). CONCERT_ARG_PARADE opens the Hit Parade (saved singles). Debug /
 * runner: -1 - id (-1..-1001) sings with resident `id`; on an island
 * with nobody in that slot the premade citizens move in first.
 * CONCERT_ARG_CHOIR - id / CONCERT_ARG_BAND - id preselect the act. */
#define CONCERT_ARG_PARADE (-5000)
#define CONCERT_ARG_BAND   (-6000)
#define CONCERT_ARG_CHOIR  (-7000)
/* The Music Studio's PERFORM tab hands its song over with
 * concert_stage_song() (song.h), which pushes SCENE_CONCERT with this. */
#define CONCERT_ARG_STUDIO (-8000)
/* SCENE_DREAM: arg >= 0 is the sleeper; -1 - id = debug sleeper (premade
 * island if empty); DREAM_ARG_NIGHTMARE - id forces the nightmare. */
#define DREAM_ARG_NIGHTMARE (-3000)

/* Resident id for a debug arg (moves the premade citizens in when the
 * slot is empty), or -1. Shared by both scenes. */
int  concert_demo_resident(int id);
/* Display name of a resident ("???" if none). */
const char *concert_name(int id);

/* ---- music --------------------------------------------------------------- */
/* (CSCALE_*: song.h owns SONG_SCALE_*) */
enum { CSCALE_MAJOR, CSCALE_MINOR, CSCALE_DORIAN, CSCALE_PHRYGIAN,
       CSCALE_HARMONIC, CSCALE_MIXO, CSCALE_COUNT };
/* how the chord channel plays */
enum { CH_ARP, CH_STAB, CH_PAD, CH_ALBERTI, CH_POWER, CH_OFFBEAT };
/* how the bass channel plays */
enum { BS_ROOTS, BS_OCTAVES, BS_WALK, BS_EIGHTHS, BS_LONG, BS_GALLOP };
/* how the crowd moves */
enum { CROWD_BOB, CROWD_SWAY, CROWD_JUMP, CROWD_HEADBANG, CROWD_WAVE,
       CROWD_MOSH };
/* LED wall / light show */
enum { SHOW_HEARTS, SHOW_EQ, SHOW_SKYLINE, SHOW_PIXELS, SHOW_STARS,
       SHOW_SPOTS, SHOW_DISCO, SHOW_WAVES, SHOW_GLASS, SHOW_GRID,
       SHOW_BATS, SHOW_FLAMES, SHOW_SKULLS, SHOW_GEARS, SHOW_NOTES,
       SHOW_FIELDS, SHOW_COUNT };

typedef struct ConcertGenre {
    uint8_t bpm;
    uint8_t scale;            /* CSCALE_*                                 */
    uint8_t pent;             /* 1 = pentatonic-flavoured melody          */
    uint8_t root;             /* lowest key (MIDI) of the lead octave     */
    uint8_t lead_wave, chord_wave, bass_wave;
    uint8_t chord_style, bass_style;
    uint8_t drums;            /* DRUMS_* pattern (concert_song.c)         */
    uint8_t crowd;            /* CROWD_*                                  */
    uint8_t show;             /* SHOW_*                                   */
    uint8_t edgy;             /* 1 = heavy music (darker stage)           */
    uint8_t c0, c1, c2;       /* stage paints: main, accent, beams        */
    const char *adlib;        /* "BLEGH!", "Yo ho!" ...                   */
} ConcertGenre;
extern const ConcertGenre concert_genres[MUSIC_COUNT];

#define SONG_STEPS_BAR    16
#define SONG_SECTION_BARS 4
#define CONCERT_LINES     12
#define CONCERT_LINE_LEN  SONG_LYRIC_LEN    /* 28: 27 chars + NUL         */
#define CONCERT_TITLE_LEN SONG_TITLE_LEN    /* 16: 15 chars + NUL         */
#define CONCERT_SYL_MAX   20

typedef struct ConcertSong {
    uint16_t singer;          /* lead resident id                         */
    uint8_t  genre, take;     /* MUSIC_*, 0..3                            */
    uint8_t  key;             /* MIDI root of the lead octave             */
    uint8_t  bpm;
    uint8_t  edgy;            /* Hellvile lyrics                          */
    uint8_t  pad;
    uint8_t  prog[2][4];      /* chord degree per bar: verse / chorus     */
    uint8_t  mel[2][SONG_SECTION_BARS * SONG_STEPS_BAR];
                              /* 0 rest, 1 hold, else MIDI note (onset)   */
    uint32_t seed;
    char     title[CONCERT_TITLE_LEN];
    char     lines[CONCERT_LINES][CONCERT_LINE_LEN];
                              /* 0..3 verse 1, 4..7 chorus, 8..11 verse 2 */
} ConcertSong;

/* Song layout: intro 2 bars, verse 1, chorus, verse 2, chorus, outro 2
 * (20 bars); an encore is chorus + outro. */
enum { SEC_INTRO, SEC_VERSE1, SEC_CHORUS1, SEC_VERSE2, SEC_CHORUS2,
       SEC_OUTRO, SEC_END };
#define SONG_BARS        20
#define SONG_ENCORE_BARS 6

/* Compose everything (deterministic: same singer/genre/take + same town
 * data -> same song on every platform). */
void concert_song_make(ConcertSong *s, int singer, int genre, int take);
/* 32-bit fingerprint of a song (tests). */
uint32_t concert_song_hash(const ConcertSong *s);
/* Frequency in Hz of a MIDI note. */
uint16_t concert_note_hz(int midi);

/* ---- the act: solo, band or choir ------------------------------------------- */
enum { CMODE_SOLO, CMODE_BAND, CMODE_CHOIR, CMODE_COUNT };
enum { BAND_GUITAR, BAND_BASS, BAND_DRUMS, BAND_KEYS, BAND_COUNT };
#define CONCERT_NOBODY 0xFFFF
typedef struct ConcertCast {
    uint8_t  mode;                     /* CMODE_*                           */
    uint8_t  layout;                   /* CHOIR_* (song.h)                  */
    uint8_t  n;                        /* singers incl. the lead, 1..25     */
    uint8_t  nband;                    /* band members (band mode)          */
    uint16_t ids[SONG_SINGERS];        /* [0] = the lead                    */
    uint8_t  section[SONG_SINGERS];    /* SECTION_* (song.h)                */
    uint16_t band[BAND_COUNT];         /* CONCERT_NOBODY = empty            */
    uint32_t line_mask[CONCERT_LINES]; /* bit i: ids[i] sings that line     */
} ConcertCast;
/* Cast the act around `lead`: band mates and choir members are their
 * friends first, then neighbours (deterministic). choir_n 2..25. */
void concert_cast_make(ConcertCast *c, int lead, int mode, int choir_n, int layout);
int  concert_mask_count(uint32_t m);

/* Player events (bit mask returned by concert_play_tick). */
enum { PEV_STEP = 1, PEV_BEAT = 2, PEV_BAR = 4, PEV_ONSET = 8, PEV_KICK = 16,
       PEV_SNARE = 32, PEV_END = 64, PEV_SECTION = 128 };

typedef struct ConcertPlayer {
    const ConcertSong *song;
    const ConcertCast *cast;
    uint16_t step;            /* 16th steps since the start               */
    uint16_t total;           /* steps in this performance                */
    uint16_t step_len;        /* ticks * 256 per step                     */
    uint16_t phase;           /* ticks * 256 into the step                */
    uint8_t  encore;
    uint8_t  lead_note, lead_vol, lead_age, lead_wave;
    uint8_t  harm_note;       /* choir harmony (0 none)                   */
    uint8_t  bass_vol, chord_vol, drum_kind, drum_age;
    uint8_t  bass_note, chord_note;
    uint8_t  onsets, line_syl, sung;   /* karaoke state for the bar       */
    int8_t   line;            /* lyric line of the bar, -1 none           */
    int8_t   voice_k;         /* choir member whose voice sang last       */
    uint8_t  syl_start[CONCERT_SYL_MAX];
    uint8_t  kick_flash, snare_flash, onset_flash;
    uint16_t ticks;
    uint32_t singing;         /* line_mask of the current line (0 none)   */
} ConcertPlayer;

void concert_play_start(ConcertPlayer *p, const ConcertSong *s,
                        const ConcertCast *cast, int encore);
int  concert_play_tick(ConcertPlayer *p);       /* PEV_* this tick         */
void concert_play_silence(void);                /* all 4 channels off      */
int  concert_play_section(const ConcertPlayer *p);  /* SEC_*               */
int  concert_play_bar(const ConcertPlayer *p);      /* bar in the section  */
/* 0..255 progress through the performance */
int  concert_play_progress(const ConcertPlayer *p);
/* Ticks per beat of the song (x256) */
int  concert_beat_len256(const ConcertPlayer *p);
/* Music restored after a show (the hub's song for this time of day). */
void concert_restore_music(void);

/* Copy a song (+ its choir) into the shared song format for the Music
 * Studio: 4 patterns (verse a/b, chorus a/b), order verse-chorus x2,
 * lyrics = verse 1 + chorus, choir + per-line singers. */
void concert_song_export(const ConcertSong *s, const ConcertCast *c, DoomoSong *out);

/* ---- lyrics ---------------------------------------------------------------- */
/* Fill s->title and s->lines for s->singer / s->genre / s->take. */
void concert_lyrics_make(ConcertSong *s, uint32_t seed);
/* Syllable starts of a line (byte offsets); returns the count (>= 1 for
 * a non-empty line). */
int  concert_syllables(const char *line, uint8_t *starts, int max);
/* The singer's job as a lowercase phrase ("imp accountant"). */
void concert_job_of(int id, char *buf, int cap);
/* "Grimbert & the Spiders" - a band name for a lead. */
void concert_band_name(int lead, int genre, uint32_t seed, char *buf, int cap);
/* A critic's one-line review for 1..5 stars. */
const char *concert_review(int stars, int genre, int edgy, uint32_t seed);

/* ---- singles (the Hit Parade, kept in two progress counters) ---------------- */
#define CONCERT_SINGLES 4
typedef struct ConcertSingle { int16_t singer; uint8_t genre, take; } ConcertSingle;
int  concert_single_get(int i, ConcertSingle *out);   /* 1 if slot used    */
/* Save a single at the top of the chart (older ones move down, the 4th
 * drops off; the same song moves back to #1). Returns its rank (0). */
int  concert_single_save(int singer, int genre, int take);
void concert_single_remove(int i);
int  concert_single_count(void);
uint16_t concert_single_pack(int singer, int genre, int take);
int  concert_single_unpack(uint16_t code, ConcertSingle *out);

/* ---- stage (concert_gfx.c) -------------------------------------------------- */
enum { STAGE_SUGAR_BOWL, STAGE_DISCO_INFERNO, STAGE_GRAVEYARD,
       STAGE_HELLFIRE, STAGE_DREAMLAND, STAGE_COUNT };
extern const char *const concert_stage_names[STAGE_COUNT];

#define CROWD_MAX 18
typedef struct StageView {
    int genre, stage;
    int lights;               /* 0 dark .. 16 full show                    */
    int curtain;              /* 0 open .. 256 closed                      */
    int beat;                 /* ticks since the last beat (pulse)         */
    int kick, snare;          /* flash ticks                               */
    int hype;                 /* 0..100 crowd hype                         */
    int pyro;                 /* ticks of the A-button pyro burst          */
    int confetti;             /* ticks of confetti                         */
    int cheer;                /* ticks of a crowd cheer (arms up)          */
    int encore;
    int onset;                /* lead onset flash ticks                    */
    int sec;                  /* SEC_* (lights per section)                */
    uint32_t singing;         /* cast members singing now (bit = ids[i])   */
    uint32_t t;
    const ConcertCast *cast;
    int16_t crowd[CROWD_MAX]; /* resident ids (-1 = anonymous silhouette)  */
    int ncrowd;
} StageView;

/* layout for this resolution */
typedef struct StageLayout {
    int w, h, big;
    int floor_y;              /* back of the stage floor                   */
    int stage_front;          /* front lip of the stage                    */
    int wall_x, wall_y, wall_w, wall_h;   /* LED wall                      */
    int truss_h;
    int lyric_y;              /* top of the karaoke pill                   */
} StageLayout;
void concert_layout(StageLayout *L);
void concert_draw_stage(const StageView *v, const StageLayout *L);
void concert_draw_performers(const StageView *v, const StageLayout *L);
void concert_draw_crowd(const StageView *v, const StageLayout *L);
void concert_draw_fx(const StageView *v, const StageLayout *L);
void concert_draw_curtain(const StageView *v, const StageLayout *L);
/* Lyric pill: line with the first `lit` bytes highlighted and a bouncing
 * heart over byte `ball` (-1 none); who = resident shown singing (-1). */
void concert_draw_lyric(const StageLayout *L, const char *line, int lit, int ball,
                        int edgy, uint32_t t);
/* Small helpers shared with the dreams. */
void concert_note_glyph(int x, int y, uint8_t c);
void concert_confetti(int x0, int y0, int w, int h, uint32_t t, int n, int fall);

/* ---- dreams ---------------------------------------------------------------- */
#define DREAM_VIGNETTES 16
#define DREAM_SHOW 3              /* vignettes per dream                   */
typedef struct DreamCast {
    int16_t dreamer, friend_id, crush, rival;
    uint8_t food, hate_food, hobby, fear, color, music;
    uint8_t edgy, possessed;
} DreamCast;
/* The vignettes a sleeper dreams tonight (deterministic per id + day);
 * returns 1 if tonight is a nightmare. */
int  dream_plan(int id, uint32_t day, uint8_t *out, int force_nightmare);
void dream_cast(int id, DreamCast *c);
/* Caption of a vignette into buf (cute part, then the twist after `split`
 * bytes; returns split). */
int  dream_caption(int vignette, const DreamCast *c, int nightmare, char *buf, int cap);
extern const char *const dream_vignette_names[DREAM_VIGNETTES];
/* Draw a vignette; t = ticks into it, creep 0..16 (cute -> creepy). */
void dream_draw_vignette(int vignette, const DreamCast *c, int t, int creep,
                         int nightmare);
void dream_draw_sky(int creep, int nightmare, uint32_t t);
void dream_draw_frame(int creep, int nightmare, uint32_t t);
/* The sleeper's night bedroom (sleeper's portrait under a quilt). */
void dream_draw_bedroom(int id, const DreamCast *c, int expr, uint32_t t, int shake);
/* Thought bubbles rising from the sleeper, the last one swelling to fill
 * the screen (t of total ticks). */
void dream_draw_bubble_in(int id, int t, int total, uint32_t clock);
/* A procedural food icon (48 foods) at (cx, cy); size ~ radius; teeth
 * 0..16 (cute -> fanged). */
void dream_food(int food, int cx, int cy, int size, int teeth, uint32_t t);
/* A procedural icon for a fear (24 fears); creep 0..16. */
void dream_fear(int fear, int cx, int cy, int size, int creep, uint32_t t);
/* Dream journal (which vignettes were ever seen; unlock bits 944..959). */
int  dream_journal_has(int v);
int  dream_journal_mark(int v);           /* 1 if new                   */
int  dream_journal_count(void);

/* ---- progress & unlocks ------------------------------------------------------ */
/* unlock ids (unlocks_concert.c): the concert owns 896..959 */
enum {
    UL_CONCERT_FIRST = 896,
    UL_PART_BAND_TEE = 896, UL_PART_HEADPHONES, UL_PART_PLATFORM,
    UL_PART_LEATHER, UL_PART_POMPADOUR, UL_PART_MULLET, UL_PART_SPARKLE_AURA,
    UL_PART_SPARKLE_EYES, UL_PART_STAR_FRECKLES, UL_PART_RIPPED_JEANS,
    UL_PART_STAR_HALO, UL_PART_EMO_FRINGE,
    UL_STAGE_DISCO, UL_STAGE_GRAVEYARD, UL_STAGE_HELLFIRE, UL_STAGE_DREAMLAND,
    UL_PART_DROOPY_EYES, UL_PART_WAVY_MOUTH, UL_PART_MESSY_HAIR,
    UL_PART_SLEEPY_EYES, UL_PART_WAVY_HAIR,
    UL_CONCERT_LAST
};
#define UL_DREAM_JOURNAL 944      /* 16 raw bits: vignettes seen          */
/* MISC unlock list id for this module's own content (stage themes). */
#define UL_LIST_CONCERT 0x43
/* 1 if this stage can be picked (default stage always). */
int  concert_stage_unlocked(int stage);

#endif
