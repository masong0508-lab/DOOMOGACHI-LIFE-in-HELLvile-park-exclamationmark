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
 *   studio_start.c  ST-EASY: the "Start a song" card + studio_make_song
 *   studio_easy.c   ST-EASY: START = play (target named), the cursor's one-line description
 *   studio_help.c   ST-EASY: one-line help, the control card, tab labels, Studio level
 *   studio_gen.c    the composer, lyric writer, templates, choir fills
 *   unlocks_studio.c unlock ids 240..255 + the activities table
 *
 * Budgets: .bss = the song book (6 x 892) + this module's ~2 KB (the
 * working song, the undo journal, UI state); const data a few KB.
 */
#ifndef DOOMO_STUDIO_INT_H
#define DOOMO_STUDIO_INT_H

#include "doomo/doomo.h"
#include "doomo/tips.h"
#include "doomo/song_ext.h"

/* SCENE_STUDIO debug arg: premade citizens move in (empty island) and
 * template t is loaded (runner scripts, tests) */
#define STUDIO_ARG_DEMO(t) (-100 - (t))

/* ---- tabs ------------------------------------------------------------------ */
enum { STAB_SONG, STAB_NOTES, STAB_PLAY, STAB_ORDER, STAB_LYRICS, STAB_CHOIR, STAB_STAGE,
       STAB_FILE, STAB_COUNT };

/* ---- stats kept in the SONG chunk -------------------------------------------- */
typedef struct StudioStats {
    uint16_t saved;          /* songs saved to the song book              */
    uint16_t performed;      /* stage shows finished                       */
    uint16_t inspired;       /* "Inspire me!" songs                        */
    uint16_t lines;          /* lyric lines written (bits 0..14); bit 15 = Studio level Advanced (ST-EASY) */
    uint16_t genres;         /* bit per genre ever saved                   */
    uint8_t  best_choir;     /* biggest choir that performed               */
    uint8_t  encores;        /* standing ovations                          */
} StudioStats;
extern StudioStats studio_stats;
void studio_stats_reset(void);
/* ST-EASY: the Studio level (Easy 0 / Advanced 1) lives in the spare top bit of `lines` (no new save tag: old
 * saves read 0 = Easy). Count users mask it with STUDIO_LINES(). */
#define STUDIO_LINES_LEVEL 0x8000u
#define STUDIO_LINES_MASK  0x7FFFu
#define STUDIO_LINES(s) ((s)->lines & STUDIO_LINES_MASK)

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
#define STUDIO_UNDO_BYTES 768
#define STUDIO_REDO_BYTES 384        /* ST-EASY: the redo journal (scene scratch, not .bss) */
#define STUDIO_CROWD 11            /* the dark crowd: one short row of 5 behind a row of 6 */

typedef void (*StudioPopDone)(int choice);   /* -1 = cancelled */

/* ST-ADV screens: builds with room for arrangement pages (host, Dreamcast) keep a working SongExt `xw` right behind the
 * song, so the undo journal (offsets from &studio.song) covers both. GBA / PS1 (SONGX_BOOK_PAGES 0) have no xw. */
#if SONGX_BOOK_PAGES > 0
#define STUDIO_ADV 1
#else
#define STUDIO_ADV 0
#endif

typedef struct Studio {
    DoomoSong song;             /* the working song                          */
#if STUDIO_ADV
    SongExt  xw;                /* ST-ADV: the working arrangement (version 0 = none: a classic song); MUST follow `song` */
    uint8_t  aview, asel, atop; /* ORDER (Advanced): 0 the arrangement, 1 one part; its cursor and top row */
#endif
    uint32_t t;                 /* ticks in the studio                       */
    uint16_t tab_t;             /* ticks since the tab opened                */
    int16_t  prev_music;
    uint8_t  tab;
    int8_t   slot;              /* book slot the song belongs to, -1 none   */
    uint8_t  dirty, active;
    uint8_t  sel_held, sel_used;
    uint8_t  card, help;        /* ST-EASY: the Start card is open on SONG / the control card is open */
    /* SONG */
    uint8_t  srow, stop;
    /* NOTES */
    uint8_t  pat, trk, stp, ntop;
    uint8_t  last[SONG_TRACKS];
    uint8_t  a_edit;            /* A held on a placed note                   */
    uint8_t  kb_flash, kb_note;
    int8_t   clip_pat, clip_trk;
    int8_t   rng_a;             /* ST-TOOLS: the marked start step of the range tools, -1 = none (the cursor's bar) */
    uint8_t  sub, lane;         /* ST-TOOLS: 0 the grid, SUB_DRUMS the drum editor, SUB_CHORDS the chord track (NOTES). Their
                                 * cursor: studio.pat (the pattern), studio.stp (the step), studio.lane (the drum lane 0..7) */
    /* ORDER */
    uint8_t  osel, otop;
    uint8_t  o_a;               /* ORDER: the A press that may become a tap (not one that chose a menu item) */
    /* LYRICS */
    uint8_t  lsel, ltop;
    int8_t   sing_pos;          /* previewing one line: its order position  */
    /* CHOIR */
    uint8_t  csel;              /* 0..SONG_SINGERS-1 singer slots, SONG_SINGERS = stage row */
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
    const char *pop_items[10];   /* ST-TOOLS: up to 10 (the Pattern tools menu) */
    const char *const *pop_help; /* ST-TOOLS: the one-line help of each item, the card's last line (NULL = none) */
    uint16_t pop_hw;             /* the widest of them, in px */
    StudioPopDone pop_done;
    /* toast-ish message bubble */
    uint16_t msg_t;
    char     msg[32];
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
/* the bytes of the studio the undo journal reaches: the song and, where there is one, the arrangement behind it */
#if STUDIO_ADV
#define STUDIO_JBYTES (sizeof(DoomoSong) + sizeof(SongExt))
#else
#define STUDIO_JBYTES sizeof(DoomoSong)
#endif

/* ---- ST-EASY state: lives in the shared scene scratch (on the GBA the tail of studio.c's BakeSt), not in .bss.
 * st_easy() returns it, claiming + zeroing it when another scene took the scratch (the redo history is simply
 * gone then; the level comes back from studio_stats). */
#define STUDIO_DESC_LEN 44
typedef struct StEasy {
    uint16_t redo_used;
    uint8_t  redo_steps;
    uint8_t  level;             /* 0 Easy, 1 Advanced (mirrors studio_stats.lines bit 15) */
    uint8_t  edited;            /* an edit was made this session: the one-line help stops */
    uint8_t  redo_lost;         /* the last undo was too big to keep for a redo (say so) */
    uint8_t  mk_valid;          /* the top undo step is the Start card's make: recompose it on redo (no big image) */
    uint8_t  mk_style, mk_mood, mk_tempo, mk_length;
    uint16_t mk_mark;           /* studio.undo_used right after that step */
    uint32_t mk_seed;
    uint8_t  before_ok;         /* STUDIO-30: `before` holds the song a take (Make it! / Shuffle / a template) was made over */
    uint8_t  before[sizeof(DoomoSong)];   /* its raw image, for a step whose RLE image is over the journal's size */
    uint8_t  csel, style, mood, tempo, length;   /* the Start card */
    uint16_t help_t;            /* ticks of the one-line help left on the bar */
    uint32_t seed;              /* the Start card's seed */
    uint32_t desc_key;          /* NOTES cursor line cache */
    char     desc[STUDIO_DESC_LEN];
    uint32_t ord_key;           /* NOTES "in Order 2,3,5" cache (the order slots of the pattern) */
    char     ord[24];
    uint8_t  bpm_val, bpm_wf, bpm_ws;   /* the tab strip's "110 bpm" / "110" text and widths, cached */
    char     bpm_full[10], bpm_short[5];
    uint8_t  redo[STUDIO_REDO_BYTES];
} StEasy;
StEasy *st_easy(void);

/* ---- studio.c: shared services ------------------------------------------------ */
void st_msg(const char *fmt, ...);
void st_popup(const char *title, const char *const *items, int n, uint32_t lock,
              StudioPopDone done);
int  st_popup_open(void);
void st_set_tab(int tab);
void st_play_from(int order_pos);       /* toggles: stops if playing     */
void st_stop(void);
void st_popup_help(const char *const *help);   /* ST-TOOLS: call right after st_popup: the card's last line says what the highlighted item does */
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
int  ed_redo(void);                     /* ST-EASY: 1 = redone; the step the last undo took back */
int  ed_redo_steps(void);
void ed_reset(void);
/* look */
uint8_t st_track_color(int t, int shade);    /* shade 0 dark, 1 base, 2 light */
uint8_t st_pattern_color(int p, int shade);
void st_panel(int x, int y, int w, int h, int style);
int st_bulbs(int rx, int ry, int rw, int rh, int phase);   /* the stage poster's marquee (studio_stage.c) */
void st_chibi(int id, int x, int y, int dir, int frame, int flags, uint8_t col);
void st_chibi_ui(int id, int x, int y, int dir, int frame, int flags, uint8_t col);   /* S3-2 */
/* Baked shapes (GBA, or any build with DOOMO_STUDIO_BAKE): a shape drawn from many rects is painted once into an image
 * (through ST_RECT / the UI kit's own rasteriser, so the same pixels) and from then on drawn with one plat_image.
 * st_bake() draws fn(x, y, a) that way and returns 1, or returns 0 (nothing drawn) when it cannot and the caller draws
 * it straight. k0 / k1 name the shape: equal keys must mean equal pixels. */
#if defined(DOOMO_PLATFORM_GBA) || defined(DOOMO_STUDIO_BAKE)
#define ST_BAKE 1
void ur_rect(int x, int y, int w, int h, uint8_t c);   /* ui_raster.c: plat_rect, or the offscreen target while baking */
#define ST_RECT ur_rect
#else
#define ST_BAKE 0
#define ST_RECT plat_rect
#endif
typedef void (*StBakeFn)(int x, int y, const int *a);
int  st_bake(uint32_t k0, uint32_t k1, int x, int y, StBakeFn fn, const int *a, int maxpix);
void st_drum_icon(int d, int cx, int cy, int t);
void st_note_glyph(int x, int y, uint8_t c);  /* a little eighth note */
int  st_wide(void);                           /* 1 at 320 px and wider */
/* The open tab's own small state (ST-PLAY: the pads): STUDIO_TABSTATE bytes of the shared scene scratch (never a static:
 * the GBA's EWRAM is full), zero-filled when claimed anew (*fresh = 1: set your defaults); gone when another scene takes
 * the scratch, so keep nothing in it that cannot be rebuilt. NULL when the scratch is not available. */
#define STUDIO_TABSTATE 64
void *st_tab_state(int *fresh);
void st_tab_state_drop(void);                  /* forget it (studio_enter): the next st_tab_state is fresh */
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
void tab_play_update(void);    void tab_play_draw(StRect r);   /* studio_play.c (ST-PLAY) */
void tab_play_shift(void);     /* the PLAY tab while SELECT is held: octave, chord set, record */
void tab_play_leave(void);     /* a take ends, the jam lets go, the loop stops (tab change, leaving the studio) */
int  play_prompts(UiPrompt *p);
void tab_order_update(void);   void tab_order_draw(StRect r);
void tab_lyrics_update(void);  void tab_lyrics_draw(StRect r);
void tab_choir_update(void);   void tab_choir_draw(StRect r);
void tab_stage_update(void);   void tab_stage_draw(StRect r);
void tab_file_update(void);    void tab_file_draw(StRect r);
int  tab_prompts(int tab, UiPrompt *p);        /* C3: the tab's prompt bar items (at most UI_PROMPTS_MAX) */
extern int studio_sel_tap;          /* SELECT was tapped this tick (tools menu) */
/* the full-screen show (PERFORM) */
int  stage_fullscreen(void);
void stage_draw_full(void);
void stage_start(void);
void stage_leave(void);

/* ---- studio_start.c / studio_easy.c / studio_help.c (ST-EASY) --------------------- */
enum { SMOOD_HAPPY, SMOOD_SWEET, SMOOD_SAD, SMOOD_SPOOKY, SMOOD_FIERCE, SMOOD_COUNT };
enum { STEMPO_SLOW, STEMPO_MEDIUM, STEMPO_FAST, STEMPO_FRANTIC, STEMPO_COUNT };
enum { SLEN_SHORT, SLEN_SONG, SLEN_LONG, SLEN_COUNT, SLEN_EPIC = SLEN_COUNT };   /* Epic (64 bars) is Advanced only: an arrangement of 8 parts */
typedef struct StartPick { uint8_t style, mood, tempo, length; uint32_t seed; } StartPick;
/* A whole working song from four picks and a seed: a pure function of (picks, the song's choir and words). */
void studio_make_song(DoomoSong *s, const StartPick *p);
void ed_mark_make(const StartPick *p);          /* the step just committed was studio_make_song(p): redo recomposes it */
int  ed_undo_make(void);                        /* take back the top step if it is that make (no redo kept); 1 = done */
int  studio_pick_bpm(int style, int tempo);
int  studio_pick_passes(int length);
int  st_adv_folded(char *buf, int cap);        /* FILE: the fold message of the open arrangement (0 = nothing to say) */            /* order passes (2 bars each) */
const char *studio_style_name(int style);        /* the friendly short name */
const char *studio_mood_name(int mood);
const char *studio_tempo_name(int tempo);
const char *studio_length_name(int length);
void st_card_open(void);                         /* the Start card (on SONG) */
void st_card_close(void);
void st_card_update(void);
void st_card_draw(StRect r);
int  st_card_prompts(UiPrompt *p);
void st_file_inspire(void);                      /* studio_file.c: the Inspire me! picker */
void st_file_templates(void);
/* START: play, with the target named ("Play song" / "Play from here", "Stop" while playing) */
const char *st_play_verb(void);
void st_start(void);
/* the cursor's cell on NOTES as one line (text is cached in st_easy) */
const char *st_describe_cell(int pat, int trk, int stp);
int  st_describe_text(char *out, int cap, int maxw, int pat, int trk, int stp);
/* help: one line per tab, the control card, labels, level */
const char *st_tab_help(int tab);
const char *st_tab_label(int tab);
void st_help_arm(void);                          /* show the one-line help on the bar (until the first edit) */
void st_bar_draw(void);                          /* the bottom bar: help line or the tab's prompts */
void st_help_open(void);
int  st_help_is_open(void);
void st_help_update(void);
void st_help_draw(void);
int  st_help_rect(int *x, int *y, int *w, int *h);
int  st_level(void);
void st_level_set(int level);


/* ---- studio_adv.c (ST-ADV): the Advanced ORDER screen. All of it is a no-op where STUDIO_ADV is 0 ------------------ */
int  st_adv_on(void);                  /* an arrangement is open (the ORDER tab shows it) */
int  st_adv_eats_b(void);              /* B left the part view this frame: the studio must not take it as "leave" */
void st_adv_enter(int slot);           /* a song came in from the book slot (-1: not from the book): load its page or none */
void st_adv_take(void);                /* a take / template / new song replaces the song: drop the arrangement (inside the open undo step) */
int  st_adv_saved(int slot);           /* after the classic song was saved to the slot: keep the page; 0 = it did not fit */
void st_adv_play(int pos, int singer); /* play (the arrangement when there is one) from flat pass `pos` */
int  st_adv_pass_of_cursor(void);      /* ORDER's cursor as a pass number */
uint32_t st_adv_ticks(void);           /* the whole song in 30 Hz ticks: the arrangement's (tempo per part) or the classic order's */
const SongExt *st_adv_ext(void);       /* the arrangement for the Concert Hall, NULL for a classic song */
int  st_adv_line_pattern(int line);    /* the pattern (0..3) the song sings lyric line `line` over, -1 = never or not in the 4 classic ones */
int  st_adv_epic(void);                /* Advanced, after a take: lay the made song out as a 64-bar arrangement (one undo step); 1 = did */
int  st_adv_try_open(void);            /* the ORDER tab opened on Advanced: make the arrangement from the classic order; 1 = did */
void tab_order_adv_update(void);
void tab_order_adv_draw(StRect r);
int  st_adv_prompts(UiPrompt *p);

/* ---- studio_grid.c (ST-TOOLS): the drum editor and the chord track, two screens of the NOTES tab ---------------- */
enum { SUB_NONE, SUB_DRUMS, SUB_CHORDS };
void st_sub_open(int sub);
void st_sub_update(void);
void st_sub_draw(StRect r);
int  st_sub_prompts(UiPrompt *p);
/* studio_edit.c: shared by the two screens */
typedef int (*ToolFn)(DoomoSong *s, const int *a);
int  st_tool_apply(void *reg, int len, ToolFn fn, const int *a);   /* one undo step, none when nothing changes: returns the cells changed */
void st_tool_done(int n, const char *ok);
void st_pattern_pills(int x, int y);
void st_groove_menu(void);
void st_chord_menu(void);
void st_drum_hear(int p);                                /* the first drum hit of the pattern sounds */

/* ---- studio_tools.c (ST-TOOLS): pure edits on a song; the caller ed_touch()es the cells and ed_commit()s. Each returns
 * the cells it changed (0 = nothing to do). Ranges are steps [from, to) of track t of pattern p. */
void tl_range(int mark, int stp, int *from, int *to);      /* the mark..cursor range, or the cursor's bar */
int  tl_shift(DoomoSong *s, int p, int t, int from, int to, int steps);
int  tl_octave(DoomoSong *s, int p, int t, int from, int to, int dir);
int  tl_repeat(DoomoSong *s, int p, int t, int from, int to);
int  tl_clear(DoomoSong *s, int p, int t, int from, int to);
int  tl_reverse(DoomoSong *s, int p, int t, int from, int to);
int  tl_stretch(DoomoSong *s, int p, int t, int from, int to, int faster);
int  tl_degree(const DoomoSong *s, int midi);
int  tl_chord_count(const DoomoSong *s);
int  tl_chord_at(const DoomoSong *s, int p, int h);        /* half bar h 0..3: the chord's degree, -1 none */
int  tl_suggest_chords(const DoomoSong *s, int p, int h, int n, uint8_t out[4]);
void tl_chord_name(const DoomoSong *s, int deg, char *buf, int cap);
int  tl_chord_write(DoomoSong *s, int p, int h, int deg);
int  tl_chord_clear(DoomoSong *s, int p, int h);           /* the half bar's chord taken out */
int  tl_chord_repeat(DoomoSong *s, int p, int h);          /* its chord copied to the half bars after it */
int  tl_drum_toggle(DoomoSong *s, int p, int stp, int drum);   /* the drum editor's cell: set / take out; the new value */
int  tl_drum_lane_clear(DoomoSong *s, int p, int drum);
void tl_phrase(const DoomoSong *s, int p, int bar, int variant, uint8_t out[16]);
int  tl_phrase_write(DoomoSong *s, int p, int bar, const uint8_t ph[16]);
int  tl_groove_write(DoomoSong *s, int p, int g, int with_fill);
int  studio_groove_count(void);
const char *studio_groove_name(int g);
int  tl_harmonize(DoomoSong *s, int p, int kind);          /* 0 third, 1 fifth, 2 octave below the lead */
int  tl_section_copy(DoomoSong *s, int src, int dst, int shift, int groove);
int  tl_free_pattern(const DoomoSong *s, int src);
int  tl_mix_balance(DoomoSong *s);
int  tl_mix_flags(const DoomoSong *s);
int  st_harmonize_song(int kind);                        /* studio_edit.c: the choir's auto-harmony over the whole song */
int  tl_fit_status(const DoomoSong *s, int line, int *words, int *notes);

/* ---- studio_gen.c --------------------------------------------------------------- */
/* Compose music only (patterns, order, instruments) for a genre. */
void studio_compose(DoomoSong *s, int genre, uint32_t seed);
/* The same with the key mode forced (a SONG_SCALE_*, or -1 = the genre's own): the Start card's mood. */
void studio_compose_scale(DoomoSong *s, int genre, uint32_t seed, int scale);
void studio_style_bpm(int genre, int *lo, int *hi);
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
void studio_part_memo_reset(void);                                 /* forget the memo (studio_enter) */
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
