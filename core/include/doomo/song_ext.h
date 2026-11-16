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
 *   lyrics        a section sings the classic song's lyric lines: section.line[k] (0..7, 0xFF none) is the line of its
 *                 k-th pass (k mod 4), and that line's line_singers choose the singers
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
#define SX_FLAG_ADVANCED 1             /* written in Advanced */
#define SX_FLAG_FREE     2             /* free notes (no scale snap) */

/* the language of the sections */
enum { SX_INTRO, SX_VERSE, SX_PRECHORUS, SX_CHORUS, SX_BRIDGE, SX_OUTRO, SX_NAMES };
enum { SX_FADE_NONE, SX_FADE_IN, SX_FADE_OUT, SX_FADE_COUNT };

/* a console plays this many tracks (4 on the old tone player; a port with more voices defines it) */
#ifndef SONGX_TRACKS_MAX
#define SONGX_TRACKS_MAX 4
#endif
/* encoded pages the book can keep: 0 = none (GBA / PS1 until the coordinator grants resident bytes, plan section 4) */
#ifndef SONGX_BOOK_PAGES
#if defined(DOOMO_PLATFORM_GBA) || defined(DOOMO_PLATFORM_PS1)
#define SONGX_BOOK_PAGES 0
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
