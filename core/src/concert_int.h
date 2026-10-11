/* concert_int.h - private glue of the CONCERT HALL + DREAMS expansion
 * (owner: concert). Not part of the public contract; the scenes are
 * SCENE_CONCERT and SCENE_DREAM (scenes.h) and the activities are in
 * doomo_activities_concert (activities.h).
 *
 *   concert_song.c    procedural composer (key, scale, chords, melody,
 *                     bass, drums) and the 4-channel plat_tone sequencer
 *   concert_lyrics.c  song titles + lyrics from templates (name, likes,
 *                     fears, catchphrase, job, friends, crush, rival),
 *                     syllables for karaoke + voice babble
 *   concert_gfx.c     the stage: backdrop, LED wall light show, truss,
 *                     spotlight beams, band + instruments, crowd
 *   concert_scene.c   SCENE_CONCERT: setup, show, encore, results,
 *                     singles + Hit Parade; the activities table
 *   dream.c           SCENE_DREAM: bedroom peek, vignettes, nightmares
 *   dream_gfx.c       the dream vignettes' drawing
 *   unlocks_concert.c unlock ids 864..895
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
 * with nobody in that slot the premade citizens move in first. */
#define CONCERT_ARG_PARADE (-5000)
/* SCENE_DREAM: arg >= 0 is the sleeper; -1 - id = debug sleeper (premade
 * island if empty); DREAM_ARG_NIGHTMARE - id forces the nightmare. */
#define DREAM_ARG_NIGHTMARE (-3000)

/* Resident id for a debug arg (moves the premade citizens in when the
 * slot is empty), or -1. Shared by both scenes. */
int  concert_demo_resident(int id);

/* ---- music --------------------------------------------------------------- */
enum { SCALE_MAJOR, SCALE_MINOR, SCALE_DORIAN, SCALE_PHRYGIAN,
       SCALE_HARMONIC, SCALE_MIXO, SCALE_COUNT };
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
       SHOW_FIELDS };

typedef struct ConcertGenre {
    uint8_t bpm;
    uint8_t scale;            /* SCALE_*                                  */
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

#define SONG_STEPS_BAR   16
#define SONG_SECTION_BARS 4
#define CONCERT_LINES    12
#define CONCERT_LINE_LEN 40
#define CONCERT_TITLE_LEN 34
#define CONCERT_SYL_MAX  20

typedef struct ConcertSong {
    uint16_t singer;          /* resident id                              */
    uint8_t  genre, take;     /* MUSIC_*, 0..3                            */
    uint8_t  key;             /* MIDI root of the lead octave             */
    uint8_t  bpm;
    uint8_t  prog[2][4];      /* chord degree per bar: verse / chorus     */
    uint8_t  mel[2][SONG_SECTION_BARS * SONG_STEPS_BAR];
                              /* 0 rest, 1 hold, else MIDI note (onset)   */
    uint8_t  edgy;            /* Hellvile lyrics                          */
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

/* Player events (bit mask returned by concert_play_tick). */
enum { PEV_STEP = 1, PEV_BEAT = 2, PEV_BAR = 4, PEV_ONSET = 8, PEV_KICK = 16,
       PEV_SNARE = 32, PEV_END = 64, PEV_SECTION = 128 };

typedef struct ConcertPlayer {
    const ConcertSong *song;
    const uint8_t *voice;     /* singer's voice sliders                   */
    uint16_t step;            /* 16th steps since the start               */
    uint16_t total;           /* steps in this performance                */
    uint16_t step_len;        /* ticks * 256 per step                     */
    uint16_t phase;           /* ticks * 256 into the step                */
    uint8_t  encore;
    uint8_t  lead_note, lead_vol, lead_age;
    uint8_t  bass_vol, chord_vol, drum_kind, drum_age;
    uint8_t  bass_note, chord_note;
    uint8_t  onsets, line_syl, sung;   /* karaoke state for the bar       */
    int8_t   line;            /* lyric line of the bar, -1 none           */
    uint8_t  syl_start[CONCERT_SYL_MAX];
    uint8_t  kick_flash, snare_flash, onset_flash;
    uint16_t ticks;
} ConcertPlayer;

void concert_play_start(ConcertPlayer *p, const ConcertSong *s,
                        const uint8_t *voice, int encore);
int  concert_play_tick(ConcertPlayer *p);       /* PEV_* this tick         */
void concert_play_silence(void);                /* all 4 channels off      */
int  concert_play_section(const ConcertPlayer *p);  /* SEC_*               */
int  concert_play_bar(const ConcertPlayer *p);      /* bar in the section  */
/* 0..255 progress through the performance */
int  concert_play_progress(const ConcertPlayer *p);
/* Music restored after a show (the hub's song for this time of day). */
void concert_restore_music(void);

/* ---- lyrics ---------------------------------------------------------------- */
/* Fill s->title and s->lines for s->singer / s->genre / s->take. */
void concert_lyrics_make(ConcertSong *s, uint32_t seed);
/* Syllable starts of a line (byte offsets); returns the count (>= 1 for
 * a non-empty line). */
int  concert_syllables(const char *line, uint8_t *starts, int max);
/* The singer's job as a lowercase phrase ("imp accountant"). */
void concert_job_of(int id, char *buf, int cap);

/* ---- singles (the Hit Parade, kept in progress counters) ------------------- */
#define CONCERT_SINGLES 4
typedef struct ConcertSingle { int16_t singer; uint8_t genre, take; } ConcertSingle;
int  concert_single_get(int i, ConcertSingle *out);   /* 1 if slot used    */
/* Save a single at the top of the chart (older ones move down, the 4th
 * drops off). Returns 1. */
int  concert_single_save(int singer, int genre, int take);
int  concert_single_count(void);
uint16_t concert_single_pack(int singer, int genre, int take);
int  concert_single_unpack(uint16_t code, ConcertSingle *out);

/* ---- stage (concert_gfx.c) -------------------------------------------------- */
enum { STAGE_SUGAR_BOWL, STAGE_DISCO_INFERNO, STAGE_GRAVEYARD,
       STAGE_HELLFIRE, STAGE_DREAMLAND, STAGE_COUNT };
extern const char *const concert_stage_names[STAGE_COUNT];

typedef struct StageView {
    int genre, stage;
    int lights;               /* 0 dark .. 16 full show                    */
    int curtain;              /* 0 open .. 256 closed                      */
    int beat;                 /* ticks since the last beat (pulse)         */
    int kick, snare;          /* flash ticks                               */
    int hype;                 /* 0..100 crowd hype                         */
    int pyro;                 /* ticks of the A-button pyro burst          */
    int confetti;             /* ticks of confetti                         */
    int encore;
    int nightmare;            /* unused by the stage (0)                   */
    uint32_t t;
} StageView;

/* layout for this resolution */
typedef struct StageLayout {
    int w, h, floor_y, stage_front, crowd_y, lyric_y, truss_h;
} StageLayout;
void concert_layout(StageLayout *L);
void concert_draw_stage(const StageView *v, const StageLayout *L);
void concert_draw_band(const StageView *v, const StageLayout *L,
                       const int16_t *band, int nband, int singer_x);
void concert_draw_singer(const StageView *v, const StageLayout *L, int id,
                         int x, int expr, int sing);
void concert_draw_beams_front(const StageView *v, const StageLayout *L, int target_x);
void concert_draw_crowd(const StageView *v, const StageLayout *L,
                        const int16_t *crowd, int n);
void concert_draw_fx(const StageView *v, const StageLayout *L);
/* Lyric pill: line with the first `lit` bytes highlighted and a bouncing
 * heart over byte `ball` (-1 none). */
void concert_draw_lyric(const StageLayout *L, const char *line, int lit, int ball,
                        int edgy, uint32_t t);

/* ---- dreams ---------------------------------------------------------------- */
#define DREAM_VIGNETTES 16
#define DREAM_SHOW 3              /* vignettes per dream                   */
typedef struct DreamCast {
    int16_t dreamer, friend_id, crush, rival;
    uint8_t food, hate_food, hobby, fear, color, music;
    uint8_t edgy;
} DreamCast;
/* The vignettes a sleeper dreams tonight (deterministic per id + day);
 * returns 1 if tonight is a nightmare. */
int  dream_plan(int id, uint32_t day, uint8_t *out, int force_nightmare);
void dream_cast(int id, DreamCast *c);
/* Caption of a vignette into buf. */
void dream_caption(int vignette, const DreamCast *c, int nightmare, char *buf, int cap);
extern const char *const dream_vignette_names[DREAM_VIGNETTES];
/* Draw a vignette; t = ticks into it, creep 0..16 (cute -> creepy). */
void dream_draw_vignette(int vignette, const DreamCast *c, int t, int creep,
                         int nightmare);
void dream_draw_sky(int creep, int nightmare, uint32_t t);
void dream_draw_frame(int nightmare, uint32_t t);
/* A procedural food icon (48 foods) at (cx, cy); size ~ radius; teeth
 * 0..16 (cute -> fanged). */
void dream_food(int food, int cx, int cy, int size, int teeth, uint32_t t);

/* ---- progress & unlocks ------------------------------------------------------ */
/* unlock ids (unlocks_concert.c) */
enum {
    UL_CONCERT_BAND = 864, UL_CONCERT_ENCORE, UL_CONCERT_DUET,
    UL_DREAM_NIGHTMARE_WATCH,
    UL_STAGE_DISCO, UL_STAGE_GRAVEYARD, UL_STAGE_HELLFIRE, UL_STAGE_DREAMLAND,
    UL_PART_BAND_TEE, UL_PART_PLATFORM, UL_PART_POMPADOUR, UL_PART_SPARKLE_AURA,
    UL_PART_SPARKLE_EYES, UL_PART_MULLET, UL_PART_STAR_FRECKLES,
    UL_PART_STAR_HALO, UL_PART_DROOPY_EYES, UL_PART_WAVY_MOUTH,
    UL_CONCERT_LAST
};
#define UL_CONCERT_FIRST 864
#define UL_CONCERT_RANGE_END 895
/* MISC unlock list id for this module's own content (stage themes). */
#define UL_LIST_CONCERT 0x43
/* 1 if this stage can be picked (default stage always). */
int  concert_stage_unlocked(int stage);

#endif
