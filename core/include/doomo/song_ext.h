/* song_ext.h - ST-ADV: the Advanced song extension (owner: studio; docs/design/STUDIO_PLAN.md 2.6 and 3).
 *
 * The classic DoomoSong (892 bytes, 4 tracks x 4 patterns x 32 steps, an order list of 16) is NOT touched. A SongExt rides
 * beside it and adds:
 *   sections      up to 16, each a list of up to 8 patterns, a key shift (-12..+12), a tempo (50..200 %), a fade
 *                 (none / in / out) and a brightness; the six default names (intro .. outro) or a custom id
 *   arrangement   up to 64 section entries (so up to 64 x 8 patterns x 2 bars = 1024 bars)
 *   patterns      up to 32; patterns 0..3 on tracks 0..3 are the classic song's own cells, every other cell lives in a
 *                 small pool of 32-step blocks (an empty cell costs nothing)
 *   tracks        4..8; a console plays songx_tracks_max() of them, the extra ones fold onto the nearest lane
 *   lyrics        a section sings a lyric line: section.line[k] (0..15, 0xFF none) is the line of its k-th pass (k mod 4),
 *                 and that line's singers choose who sings it. Lines 1..8 are the classic song's own (lyrics[], line_singers[]);
 *                 lines 9..16 live in the SongExt (lyr[], lsing[]), so the classic song is untouched
 * A classic song is "no SongExt" and plays exactly as before: song_play() is song_play_ext(s, NULL).
 * The studio keeps its working SongExt in the scene scratch; the book keeps encoded copies (songx_book_*).
 */
#ifndef DOOMO_SONG_EXT_H
#define DOOMO_SONG_EXT_H

#include "song.h"

#define SX_VERSION       1
#define SX_SECTIONS      16
#define SX_SECT_PATS     8
#define SX_ARRANGE       64
#define SX_PATTERNS      32
#define SX_TRACKS        8
#define SX_POOL          48            /* 32-step blocks for every cell outside the classic 4 x 4 */
#define SX_LINES         4             /* lyric passes a section names */
#define SX_NO_LINE       0xFF
#define SX_XLINES        8             /* lyric lines 9..16 live in the SongExt (lyr[], lsing[]) */
#define SX_LYRIC_LINES   (SONG_LYRIC_LINES + SX_XLINES)
#define SX_FLAG_ADVANCED 1             /* written in Advanced */
#define SX_FLAG_FREE     2             /* free notes (no scale snap) */
#define SX_FLAG_SHORT    8             /* in a saved page: a 4-byte one-bar mask follows (set by songx_encode) */
#define SX_FLAG_LYR      16            /* in a saved page: lyric lines 9..16 follow (set by songx_encode) */
#define SX_FLAG_CONT     32            /* in a saved page: the continuation links (3 and 4 bar patterns) follow (set by songx_encode) */
#define SX_FLAG_VEL      4             /* in a saved page: a velocity tail follows the cells (set by songx_encode) */
#define SX_VEL_SOFT      0             /* the four loudness levels of a note (the velocity bar) */
#define SX_VEL_NORMAL    1             /* the default: a note plays at its track's volume */
#define SX_VEL_LOUD      2
#define SX_VEL_PEAK      3

/* the language of the sections */
enum { SX_INTRO, SX_VERSE, SX_PRECHORUS, SX_CHORUS, SX_BRIDGE, SX_OUTRO, SX_NAMES };
enum { SX_FADE_NONE, SX_FADE_IN, SX_FADE_OUT, SX_FADE_COUNT };

/* a console plays this many tracks (4 on the old tone player; a port with more voices defines it) */
#ifndef SONGX_TRACKS_MAX
#define SONGX_TRACKS_MAX 4
#endif
/* encoded pages the book can keep: 0 = none (GBA keeps 2, PS1 none: the studio group would set the slot, plan section 4) */
#ifndef SONGX_BOOK_PAGES
#if defined(DOOMO_PLATFORM_GBA)
#define SONGX_BOOK_PAGES 2             /* GBA: two Advanced pages (book slots 0 and 1), 3,072 B of .bss; plan section 4 */
#elif defined(DOOMO_PLATFORM_PS1)
#define SONGX_BOOK_PAGES 0             /* PS1: none. 6 pages: 32,768 B left; 2 pages with the callbacks in the overlay: 37,920 B left (slot 172,032 B, studio group 170,176 B): docs/STUDIO.md "ST-ADV: PS1 pages, second try" */
#else
#define SONGX_BOOK_PAGES SONG_BOOK_SIZE
#endif
#endif
#define SONGX_PAGE_BYTES 1536          /* the most one encoded page may take */

typedef struct SxSection {
    uint8_t name;                      /* 0..5 default (SX_*), 6.. custom id */
    uint8_t npat;                      /* 1..SX_SECT_PATS */
    uint8_t pat[SX_SECT_PATS];         /* pattern ids 0..SX_PATTERNS-1 */
    uint8_t tempo;                     /* percent, 50..200 (100 = the song's bpm) */
    int8_t  key;                       /* semitones, -12..+12 */
    uint8_t fade;                      /* SX_FADE_* over the whole section */
    uint8_t bright;                    /* 0..3 lead brightness (0 = as set) */
    uint8_t line[SX_LINES];            /* lyric line (0..7) of pass k mod 4, SX_NO_LINE = instrumental */
} SxSection;

typedef struct SxTrack { uint8_t pan, env, swing; } SxTrack;   /* pan 0 left .. 2 right (1 middle), env 0..3, swing 0..8 (0 = the song's) */

typedef struct SongExt {
    uint8_t  version, tracks, sections, arrange_len, patterns, flags, nblocks, pad;
    SxSection sect[SX_SECTIONS];
    uint8_t  arrange[SX_ARRANGE];
    SxTrack  trk[SX_TRACKS];
    uint8_t  map[SX_PATTERNS][SX_TRACKS];   /* pool block + 1 for a cell outside the classic 4 x 4, 0 = empty */
    uint8_t  pool[SX_POOL][SONG_STEPS];
    uint8_t  vel[SX_PATTERNS][SX_TRACKS][SONG_STEPS / 4];   /* per-note loudness, 2 bits a step (see songx_vel); all zero = every note normal */
    uint8_t  short_mask[4];             /* bit p of the 32 bits = pattern p is one bar (16 steps): it plays steps 0..15 and the pass ends; 0 = all two bars */
    uint8_t  cont[SX_PATTERNS];         /* 0 = none, else (pattern id + 1) of the block played straight after this pattern in the same pass: a 3 or 4 bar pattern */
    /* lyric lines 9..16 (the last two members: the studio's undo journal touches them as one block). Plain bytes, so the
     * struct stays byte aligned like the rest: lsing[i] is a little endian u32 (bit n = singer[n] sings it, 0 = singer[0]) */
    char     lyr[SX_XLINES][SONG_LYRIC_LEN];
    uint8_t  lsing[SX_XLINES][4];
} SongExt;

/* a fresh Advanced song: the classic song's order becomes one section per run (all in one is fine): sections = 1, its
 * patterns the order list's entries (up to 8), arrange = [0]. `from` (may be NULL) supplies the order. */
void songx_init(SongExt *x, const DoomoSong *from);
/* clamp every field (a load, a paste, a test); returns the number of fixed fields */
int  songx_sanitize(SongExt *x);
/* cells */
int  songx_cell(const DoomoSong *s, const SongExt *x, int pat, int trk, int step);
int  songx_set_cell(DoomoSong *s, SongExt *x, int pat, int trk, int step, int v);   /* 1 = stored, 0 = pool full / out of range */
int  songx_blocks_free(const SongExt *x);
/* the velocity bar (ST-ADV expert): a note's loudness SX_VEL_SOFT..PEAK, SX_VEL_NORMAL unless set. Kept in its own layer, so a cell's
 * byte (and the classic song) never changes. songx_vel_pct is what the player multiplies the track's volume by. */
int  songx_vel(const SongExt *x, int pat, int trk, int step);
int  songx_set_vel(SongExt *x, int pat, int trk, int step, int level);   /* 1 = stored, 0 = out of range */
int  songx_vel_pct(int level);
int  songx_plen(const SongExt *x, int pat);                              /* steps a pass of pattern `pat` plays: 32 (two bars) or 16 (one bar) */
int  songx_set_plen(SongExt *x, int pat, int steps);                     /* 16 or 32; 1 = stored */
int  songx_vel_any(const SongExt *x);                                    /* any note that is not normal */
/* patterns of 3 and 4 bars (a chain of two blocks): pattern `pat` plays its own cells (two bars, or one if short) and then the cells of its
 * continuation pattern, all in one pass. The arrangement and the sections name the first block only. A continuation is a pattern like any other
 * (it has cells, a pool entry, a length of one or two bars), so a chain is 2, 3 or 4 bars: 1 + 1 is two bars too, which is just what two
 * blocks are. A block is the continuation of one head at most, and a continuation has no continuation of its own. */
int  songx_cont(const SongExt *x, int pat);                              /* the continuation's pattern id, -1 = none */
int  songx_head_of(const SongExt *x, int pat);                           /* the pattern whose continuation `pat` is, -1 = none */
int  songx_set_cont(SongExt *x, int pat, int next);                      /* next = -1 clears the link; 1 = stored, 0 = refused (see above) */
int  songx_pass_steps(const SongExt *x, int pat);                        /* steps of a whole pass starting at pattern `pat`: 16 .. 64 */
int  songx_pass_bars(const SongExt *x, int pat);                         /* the same in bars: 1 .. 4 */
int  songx_free_pattern(const SongExt *x);                               /* a pattern id nobody uses (not in a section, not linked, no cells), -1 = none */
int  songx_cont_any(const SongExt *x);                                    /* any pattern with a continuation link */
/* lyric lines: 0..7 are the song's own, 8..15 the SongExt's. x = NULL is a classic song (8 lines). Every reader goes through
 * these so that nothing indexes lyrics[] / line_singers[] past 7. */
int  songx_lines(const SongExt *x);                                      /* 16 with a SongExt, else 8 */
const char *songx_line_text(const DoomoSong *s, const SongExt *x, int line);   /* \"\" for a line that does not exist */
char *songx_line_buf(DoomoSong *s, SongExt *x, int line);                /* the writable text (SONG_LYRIC_LEN bytes), NULL when the line does not exist */
uint32_t songx_line_singers(const DoomoSong *s, const SongExt *x, int line);   /* the stored mask, 0 = singer[0] */
void songx_set_line_singers(DoomoSong *s, SongExt *x, int line, uint32_t mask);
int  songx_lyr_any(const SongExt *x);                                    /* any of lines 9..16 holds words or a singer mask */
/* the arrangement as one flat list of pattern passes */
int  songx_len(const SongExt *x);                                    /* passes in the whole song (>= 1) */
int  songx_at(const SongExt *x, int pos, int *sect, int *pass);      /* pattern id of pass `pos`; its section and its pass in the section */
uint32_t songx_ticks(const DoomoSong *s, const SongExt *x);          /* exact length in 30 Hz ticks (tempo per section) */
int  songx_tracks_max(void);
/* "This song has 8 tracks, this console plays 4: tracks 5..8 are mixed in": 0 when nothing is folded */
int  songx_folded(const SongExt *x, char *buf, int cap);
int  songx_lane_of(int trk);                                         /* where track trk is heard: 0..3 */
/* tools */
int  songx_add_section(SongExt *x, int name);                        /* new section id or -1 */
int  songx_arrange_push(SongExt *x, int sect);                       /* 1 = added */
int  songx_last_chorus_up(SongExt *x, int steps);                    /* "key change up for the last chorus": sections changed */
/* ST-ADV screens (studio_adv.c) */
void songx_from_order(SongExt *x, const DoomoSong *s);               /* the classic order as an arrangement: up to 4 sections of 4 passes; plays the same, lyric lines included */
int  songx_arrange_insert(SongExt *x, int at, int sect);             /* 1 = entry added before position `at` */
int  songx_arrange_remove(SongExt *x, int at);                       /* 1 = removed (never the last entry); a part nobody uses any more is dropped */
int  songx_sect_users(const SongExt *x, int sect);                   /* arrangement entries that play this part */
int  songx_section_dup(SongExt *x, int sect);                        /* a copy of the part: its id or -1 (16 parts) */
const char *songx_name(int name);                                    /* "Intro" .. "Outro", "Part" for a custom id */
int  songx_entry_pass(const SongExt *x, int entry);                  /* the flat pass number where an entry starts */
int  songx_entry_of_pass(const SongExt *x, int pass);                /* the entry that plays pass `pass` */
int  songx_pattern_cost(const DoomoSong *s, const SongExt *x, int pat);  /* note blocks a copy of pattern `pat` takes (its tracks that hold a note) */
/* the Note grid: a new pattern that is a copy of `from` (it comes last): its id, -1 = the song has 32 patterns, -2 = not enough note blocks.
 * `s` is not written (a pattern past the classic four lives wholly in the pool). */
int  songx_pattern_copy(DoomoSong *s, SongExt *x, int from);
/* start playing the arrangement at pass `pos` (a flat pass number), `step` into it; x = NULL is song_play_at */
void song_play_ext_at(const DoomoSong *s, const SongExt *x, int singer_override, int pos, int step);
/* serialization (one page): returns bytes, 0 when it does not fit in cap; songx_decode 1 = ok */
uint32_t songx_encode(const DoomoSong *s, const SongExt *x, uint8_t *dst, uint32_t cap);
int  songx_decode(const DoomoSong *s, SongExt *x, const uint8_t *src, uint32_t len);
/* the book's encoded pages (SONGX_BOOK_PAGES of them; the 'SONG' chunk version 2 saves them) */
int  songx_book_put(int slot, const DoomoSong *s, const SongExt *x);   /* 1 = kept; 0 = no room (the song stays classic) */
int  songx_book_get(int slot, const DoomoSong *s, SongExt *x);        /* 1 = the page has a SongExt */
void songx_book_drop(int slot);
uint32_t songx_book_raw(int slot, const uint8_t **p);                   /* the save chunk's view of a page */
int  songx_book_set_raw(int slot, const uint8_t *src, uint32_t len);
int  songx_book_has(int slot);
#endif
