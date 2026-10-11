/* studio_int.h - private glue of the MUSIC STUDIO (owner: studio).
 * Not part of the public contract: that is song.h (format, player, song
 * book) and SCENE_STUDIO (scenes.h).
 *
 *   song_player.c   the shared player + song helpers (song.h)
 *   song_book.c     doomo_song_book + the 'SONG' save extension chunk,
 *                   studio stats, studio_edit() / song_book_store()
 *   studio.c        SCENE_STUDIO: tabs, input, undo journal, popups,
 *                   shared drawing, unlock rules
 *   studio_edit.c   SONG / NOTES (tracker grid) / ORDER tabs
 *   studio_words.c  LYRICS + CHOIR tabs, the citizen picker
 *   studio_stage.c  PERFORM: the choir show
 *   studio_file.c   FILE tab: song book, templates, Inspire me
 *   studio_gen.c    the composer, lyric writer, templates, choir fills
 *   unlocks_studio.c unlock ids 240..255 + the activities table
 *
 * Budgets: .bss = the song book (6 x 892) + this module's ~2 KB (the
 * working song, the undo journal, UI state); const data a few KB.
 */
#ifndef DOOMO_STUDIO_INT_H
#define DOOMO_STUDIO_INT_H

#include "doomo/doomo.h"

/* SCENE_STUDIO debug arg: premade citizens move in (empty island) and
 * template t is loaded (runner scripts, tests) */
#define STUDIO_ARG_DEMO(t) (-100 - (t))

/* ---- tabs ------------------------------------------------------------------ */
enum { STAB_SONG, STAB_NOTES, STAB_ORDER, STAB_LYRICS, STAB_CHOIR, STAB_STAGE,
       STAB_FILE, STAB_COUNT };

/* ---- stats kept in the SONG chunk -------------------------------------------- */
typedef struct StudioStats {
    uint16_t saved;          /* songs saved to the song book              */
    uint16_t performed;      /* stage shows finished                       */
    uint16_t inspired;       /* "Inspire me!" songs                        */
    uint16_t lines;          /* lyric lines written                        */
    uint16_t genres;         /* bit per genre ever saved                   */
    uint8_t  best_choir;     /* biggest choir that performed               */
    uint8_t  encores;        /* standing ovations                          */
} StudioStats;
extern StudioStats studio_stats;
void studio_stats_reset(void);

/* ---- unlocks (UL_CAT_MISC, list = STUL_*, index = the value) ------------------ */
enum { STUL_WAVE = 40, STUL_KIT, STUL_TEMPLATE, STUL_LAYOUT };
#define STUDIO_UL_FIRST 240
enum {
    UL_STUDIO_WAVE_TINY = STUDIO_UL_FIRST,   /* WAVE_PULSE12  */
    UL_STUDIO_WAVE_SAW,                      /* WAVE_SAW      */
    UL_STUDIO_WAVE_STATIC,                   /* WAVE_NOISE    */
    UL_STUDIO_KIT_TOY,
    UL_STUDIO_KIT_BONES,
    UL_STUDIO_KIT_808,
    UL_STUDIO_KIT_FORGE,
    UL_STUDIO_TPL_SHANTY,
    UL_STUDIO_TPL_SYNTH,
    UL_STUDIO_TPL_JAZZ,
    UL_STUDIO_TPL_DISCO,
    UL_STUDIO_TPL_FORGE,
    UL_STUDIO_LAYOUT_PYRAMID,
    UL_STUDIO_LAST
};
/* 1 if the option is free or unlocked */
int  studio_wave_ok(int wave);
int  studio_kit_ok(int kit);
int  studio_layout_ok(int layout);
int  studio_template_ok(int t);
/* grant the unlocks earned by the stats (toasts) */
void studio_check_unlocks(void);

/* ---- the scene state ---------------------------------------------------------- */
#define STUDIO_UNDO_BYTES 896
#define STUDIO_CROWD 12

typedef void (*StudioPopDone)(int choice);   /* -1 = cancelled */

typedef struct Studio {
    DoomoSong song;             /* the working song                          */
    uint32_t t;                 /* ticks in the studio                       */
    uint16_t tab_t;             /* ticks since the tab opened                */
    int16_t  prev_music;
    uint8_t  tab;
    int8_t   slot;              /* book slot the song belongs to, -1 none   */
    uint8_t  dirty, active;
    uint8_t  sel_held, sel_used;
    /* SONG */
    uint8_t  srow, stop;
    /* NOTES */
    uint8_t  pat, trk, stp, ntop;
    uint8_t  last[SONG_TRACKS];
    uint8_t  a_edit;            /* A held on a placed note                   */
    uint8_t  kb_flash, kb_note;
    int8_t   clip_pat, clip_trk;
    /* ORDER */
    uint8_t  osel, otop;
    /* LYRICS */
    uint8_t  lsel, ltop;
    int8_t   sing_pos;          /* previewing one line: its order position  */
    /* CHOIR */
    uint8_t  csel;              /* 0..24 singer slots, 25 = stage row        */
    uint8_t  picking;
    int16_t  pick_id;
    /* PERFORM */
    uint8_t  show;              /* 0 poster, 1 show, 2 bows                  */
    uint8_t  applause;          /* 0..100                                    */
    uint16_t show_t;
    uint8_t  sung_lines, fit_lines, last_line, encore;
    int16_t  conductor;
    int16_t  crowd[STUDIO_CROWD];
    uint8_t  happy_gain;
    /* FILE */
    uint8_t  fsel, ftop, fmode, tsel;
    int16_t  muse;
    /* popup menu */
    uint8_t  pop_n, pop_sel;
    uint16_t pop_t;
    uint32_t pop_lock;
    const char *pop_title;
    const char *pop_items[10];
    StudioPopDone pop_done;
    /* toast-ish message bubble */
    uint16_t msg_t;
    char     msg[40];
    /* text entry */
    char     edit[SONG_LYRIC_LEN];
    int8_t   edit_line;         /* -1 = title                                */
    /* undo journal */
    uint16_t undo_used;
    uint8_t  undo_open, undo_steps;
    uint8_t  undo[STUDIO_UNDO_BYTES];
} Studio;
extern Studio studio;
#define SS (&studio.song)

/* ---- studio.c: shared services ------------------------------------------------ */
void st_msg(const char *fmt, ...);
void st_popup(const char *title, const char *const *items, int n, uint32_t lock,
              StudioPopDone done);
int  st_popup_open(void);
void st_set_tab(int tab);
void st_play_from(int order_pos);       /* toggles: stops if playing     */
void st_stop(void);
void studio_try_leave(void);
/* undo journal: ed_touch() the bytes (inside studio.song) an edit is about
 * to change, change them, then ed_commit(). */
void ed_touch(const void *field, int len);
void ed_commit(void);
int  ed_undo(void);
/* the same with coalescing: repeated edits with one key make one step */
void ed_touch_key(int key, const void *field, int len);
void ed_commit_key(int key);
int  ed_undo_steps(void);
void ed_reset(void);
/* look */
uint8_t st_track_color(int t, int shade);    /* shade 0 dark, 1 base, 2 light */
uint8_t st_pattern_color(int p, int shade);
void st_panel(int x, int y, int w, int h, int style);
void st_chibi(int id, int x, int y, int dir, int frame, int flags, uint8_t col);
void st_drum_icon(int d, int cx, int cy, int t);
void st_note_glyph(int x, int y, uint8_t c);  /* a little eighth note */
int  st_wide(void);                           /* 1 at 320 px and wider */
/* the content rectangle of a tab */
typedef struct { int x, y, w, h; } StRect;
StRect st_content(void);
/* the citizen picker: picks a resident id for done(id) (-1 = cancelled) */
void st_pick_citizen(const char *title, int start_id, void (*done)(int id));
int  st_picker_open(void);
void st_picker_update(void);
void st_picker_draw(void);
/* resident name ("You" for the player), "" if none */
const char *st_name(int id);
int  st_alive(int id);

/* ---- tabs ------------------------------------------------------------------- */
void tab_song_update(void);    void tab_song_draw(StRect r);
void tab_notes_update(void);   void tab_notes_draw(StRect r);
void tab_order_update(void);   void tab_order_draw(StRect r);
void tab_lyrics_update(void);  void tab_lyrics_draw(StRect r);
void tab_choir_update(void);   void tab_choir_draw(StRect r);
void tab_stage_update(void);   void tab_stage_draw(StRect r);
void tab_file_update(void);    void tab_file_draw(StRect r);
const char *tab_hint(int tab);
extern int studio_sel_tap;          /* SELECT was tapped this tick (tools menu) */
/* the full-screen show (PERFORM) */
int  stage_fullscreen(void);
void stage_draw_full(void);
void stage_start(void);
void stage_leave(void);

/* ---- studio_gen.c --------------------------------------------------------------- */
/* Compose music only (patterns, order, instruments) for a genre. */
void studio_compose(DoomoSong *s, int genre, uint32_t seed);
/* Lyrics + title about a citizen (their likes, fears, catchphrase, friends). */
void studio_write_lyrics(DoomoSong *s, int muse, uint32_t seed);
/* Music + lyrics + a small choir around the muse. */
void studio_inspire(DoomoSong *s, int muse, int genre, uint32_t seed);
#define STUDIO_TEMPLATES 14
const char *studio_template_name(int t);
const char *studio_template_blurb(int t);
int  studio_template_genre(int t);
int  studio_template_unlock(int t);           /* unlock id or -1 (free)   */
void studio_template_load(DoomoSong *s, int t);
/* choir quick fills */
enum { FILL_FRIENDS, FILL_HILLVILE, FILL_HELLVILE, FILL_BOTH, FILL_RANDOM, FILL_CLEAR };
int  studio_choir_fill(DoomoSong *s, int mode, int anchor, uint32_t seed);
/* per-line singing parts: verses rotate soloists / sections, repeated
 * patterns (choruses) get the full choir */
void studio_auto_parts(DoomoSong *s);
/* Line part presets (LYRICS tab). */
enum { PART_CHOIR, PART_SOLO, PART_DUET, PART_SECTION, PART_HALF, PART_COUNT };
int  studio_part_of(const DoomoSong *s, int line, int *who);   /* PART_* */
void studio_set_part(DoomoSong *s, int line, int part, int who);
int  studio_part_choices(const DoomoSong *s, int part);        /* # of who values */
void studio_part_label(const DoomoSong *s, int line, char *buf, int cap);
/* Swap rude words for cute ones; returns the number of swaps. */
int  studio_clean_text(char *t, int cap);
/* Auto-fit lyric line `line` to the lead notes of its pattern. 1 = changed. */
int  studio_autofit(DoomoSong *s, int line);
/* first order position singing `line`, -1 if none */
int  studio_line_pos(const DoomoSong *s, int line);

#endif
