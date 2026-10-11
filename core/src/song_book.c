/* song_book.c - the player's song book and its save extension chunk
 * 'SONG' (owner: studio).
 *
 * Chunk layout (version 1, little endian):
 *   u8  stats bytes (n), then n bytes of StudioStats fields (newer
 *       versions may append fields; unknown tail bytes are skipped)
 *   u8  book mask (bit i = slot i holds a song)
 *   per stored song, in slot order:
 *     u8 title length + title bytes
 *     u8 flags, genre, bpm, key, scale, swing
 *     4 x u8 (vol << 4) | wave
 *     u8 order length, then the order bytes
 *     per pattern: u8 track mask, then per used track the 32 cells
 *       run-length coded: 0x00..0x7F a literal cell, 0x80 | (n - 1) a run
 *       of n empty cells, 0xC0 | (n - 1) a run of n cells of value 1
 *       (melodic: hold, drums: kick)
 *     u8 lyric line mask, per line: u8 length + bytes
 *     u32 choir slot mask, per slot: u16 resident id
 *     u8 part mask, per line: u32 line_singers
 *     u8 choir layout, u32 seed
 *     u8 1 + 13 section bytes, or u8 0 (all automatic)
 * A town that never used the studio writes 3 bytes. A full book of busy
 * songs stays well under DOOMO_SAVE_EXT_MAX (worst case ~5.4 KB).
 */
#include "studio_int.h"

DoomoSong doomo_song_book[SONG_BOOK_SIZE];
StudioStats studio_stats;

void studio_stats_reset(void) { memset(&studio_stats, 0, sizeof(studio_stats)); }

int song_book_free_slot(void)
{
    int i;
    for (i = 0; i < SONG_BOOK_SIZE; i++)
        if (!(doomo_song_book[i].flags & SONGF_VALID)) return i;
    return -1;
}

int song_book_store(const DoomoSong *s)
{
    int i = song_book_free_slot();
    if (!s || i < 0) return -1;
    doomo_song_book[i] = *s;
    doomo_song_book[i].flags |= SONGF_VALID;
    song_sanitize(&doomo_song_book[i]);
    return i;
}

/* ====================================================================== */
/* writer / reader                                                          */
typedef struct { uint8_t *dst; uint32_t cap, n; } SbOut;
static void ob(SbOut *o, uint32_t v)
{
    if (o->dst && o->n < o->cap) o->dst[o->n] = (uint8_t)v;
    o->n++;
}
static void o16(SbOut *o, uint32_t v) { ob(o, v & 0xFF); ob(o, v >> 8); }
static void o32(SbOut *o, uint32_t v) { o16(o, v & 0xFFFF); o16(o, v >> 16); }

typedef struct { const uint8_t *src; uint32_t len, pos; int err; } SbIn;
static uint32_t ib(SbIn *in)
{
    if (in->pos >= in->len) { in->err = 1; return 0; }
    return in->src[in->pos++];
}
static uint32_t i16(SbIn *in) { uint32_t lo = ib(in); return lo | (ib(in) << 8); }
static uint32_t i32(SbIn *in) { uint32_t lo = i16(in); return lo | (i16(in) << 16); }

static int text_len(const char *t, int cap)
{
    int n = 0;
    while (n < cap - 1 && t[n]) n++;
    return n;
}

static void put_text(SbOut *o, const char *t, int cap)
{
    int i, n = text_len(t, cap);
    ob(o, (uint32_t)n);
    for (i = 0; i < n; i++) ob(o, (uint8_t)t[i]);
}

static int get_text(SbIn *in, char *t, int cap)
{
    int i, n = (int)ib(in);
    if (n > cap - 1) { in->err = 1; return 0; }
    memset(t, 0, (size_t)cap);
    for (i = 0; i < n; i++) t[i] = (char)ib(in);
    return !in->err;
}

static void put_cells(SbOut *o, const uint8_t *c)
{
    int i = 0;
    while (i < SONG_STEPS) {
        int v = c[i], run = 1;
        if (v <= 1) {
            while (i + run < SONG_STEPS && c[i + run] == v && run < 64) run++;
            if (run > 1) { ob(o, (uint32_t)((v ? 0xC0 : 0x80) | (run - 1))); i += run; continue; }
        }
        ob(o, (uint32_t)(v & 0x7F));
        i++;
    }
}

static int get_cells(SbIn *in, uint8_t *c)
{
    int i = 0;
    while (i < SONG_STEPS && !in->err) {
        uint32_t b = ib(in);
        if (b < 0x80) { c[i++] = (uint8_t)b; continue; }
        {
            int run = (int)(b & 0x3F) + 1, v = (b & 0x40) ? 1 : 0;
            if (i + run > SONG_STEPS) { in->err = 1; return 0; }
            while (run--) c[i++] = (uint8_t)v;
        }
    }
    return !in->err;
}

static int track_used(const DoomoSong *s, int p, int t)
{
    int i;
    for (i = 0; i < SONG_STEPS; i++) if (s->notes[p][t][i]) return 1;
    return 0;
}

static void put_song(SbOut *o, const DoomoSong *s)
{
    int i, p, t, any = 0;
    uint32_t m;
    put_text(o, s->title, SONG_TITLE_LEN);
    ob(o, s->flags); ob(o, s->genre); ob(o, s->bpm);
    ob(o, s->key); ob(o, s->scale); ob(o, s->swing);
    for (t = 0; t < SONG_TRACKS; t++) ob(o, (uint32_t)((s->vol[t] & 15) << 4 | (s->wave[t] & 15)));
    ob(o, s->order_len);
    for (i = 0; i < s->order_len && i < SONG_ORDER_MAX; i++) ob(o, s->order[i]);
    for (p = 0; p < SONG_PATTERNS; p++) {
        m = 0;
        for (t = 0; t < SONG_TRACKS; t++) if (track_used(s, p, t)) m |= 1u << t;
        ob(o, m);
        for (t = 0; t < SONG_TRACKS; t++) if (m & (1u << t)) put_cells(o, s->notes[p][t]);
    }
    for (i = 0, m = 0; i < SONG_LYRIC_LINES; i++) if (s->lyrics[i][0]) m |= 1u << i;
    ob(o, m);
    for (i = 0; i < SONG_LYRIC_LINES; i++) if (m & (1u << i)) put_text(o, s->lyrics[i], SONG_LYRIC_LEN);
    for (i = 0, m = 0; i < SONG_SINGERS; i++) if (s->singer[i] != SONG_NO_SINGER) m |= 1u << i;
    o32(o, m);
    for (i = 0; i < SONG_SINGERS; i++) if (m & (1u << i)) o16(o, s->singer[i]);
    for (i = 0, m = 0; i < SONG_LYRIC_LINES; i++) if (s->line_singers[i]) m |= 1u << i;
    ob(o, m);
    for (i = 0; i < SONG_LYRIC_LINES; i++) if (m & (1u << i)) o32(o, s->line_singers[i]);
    ob(o, s->choir_layout);
    o32(o, s->seed);
    for (i = 0; i < (int)sizeof(s->section); i++) any |= s->section[i];
    ob(o, any ? 1 : 0);
    if (any) for (i = 0; i < (int)sizeof(s->section); i++) ob(o, s->section[i]);
}

static int get_song(SbIn *in, DoomoSong *s)
{
    int i, p, t;
    uint32_t m;
    song_clear(s);
    if (!get_text(in, s->title, SONG_TITLE_LEN)) return 0;
    s->flags = (uint8_t)ib(in); s->genre = (uint8_t)ib(in); s->bpm = (uint8_t)ib(in);
    s->key = (uint8_t)ib(in); s->scale = (uint8_t)ib(in); s->swing = (uint8_t)ib(in);
    for (t = 0; t < SONG_TRACKS; t++) {
        uint32_t b = ib(in);
        s->wave[t] = (uint8_t)(b & 15);
        s->vol[t] = (uint8_t)(b >> 4);
    }
    s->order_len = (uint8_t)ib(in);
    if (s->order_len > SONG_ORDER_MAX) return 0;
    for (i = 0; i < s->order_len; i++) s->order[i] = (uint8_t)ib(in);
    for (p = 0; p < SONG_PATTERNS; p++) {
        m = ib(in);
        for (t = 0; t < SONG_TRACKS; t++)
            if ((m & (1u << t)) && !get_cells(in, s->notes[p][t])) return 0;
    }
    m = ib(in);
    for (i = 0; i < SONG_LYRIC_LINES; i++)
        if ((m & (1u << i)) && !get_text(in, s->lyrics[i], SONG_LYRIC_LEN)) return 0;
    m = i32(in);
    for (i = 0; i < SONG_SINGERS; i++) if (m & (1u << i)) s->singer[i] = (uint16_t)i16(in);
    m = ib(in);
    for (i = 0; i < SONG_LYRIC_LINES; i++) if (m & (1u << i)) s->line_singers[i] = i32(in);
    s->choir_layout = (uint8_t)ib(in);
    s->seed = i32(in);
    if (ib(in)) for (i = 0; i < (int)sizeof(s->section); i++) s->section[i] = (uint8_t)ib(in);
    if (in->err) return 0;
    song_sanitize(s);
    return 1;
}

#define STATS_BYTES 13

static uint32_t encode(uint8_t *dst, uint32_t cap)
{
    SbOut o;
    int i;
    uint32_t mask = 0;
    o.dst = dst; o.cap = cap; o.n = 0;
    ob(&o, STATS_BYTES);
    o16(&o, studio_stats.saved);
    o16(&o, studio_stats.performed);
    o16(&o, studio_stats.inspired);
    o16(&o, studio_stats.lines);
    o16(&o, studio_stats.genres);
    ob(&o, studio_stats.best_choir);
    ob(&o, studio_stats.encores);
    ob(&o, 0);                                    /* reserved */
    for (i = 0; i < SONG_BOOK_SIZE; i++)
        if (doomo_song_book[i].flags & SONGF_VALID) mask |= 1u << i;
    ob(&o, mask);
    for (i = 0; i < SONG_BOOK_SIZE; i++)
        if (mask & (1u << i)) put_song(&o, &doomo_song_book[i]);
    return o.n;
}

/* ====================================================================== */
/* DoomoSaveExt                                                             */
static uint32_t ext_size(void) { return encode(NULL, 0); }

static uint32_t ext_write(uint8_t *dst, uint32_t cap) { return encode(dst, cap); }

static void ext_reset(void)
{
    memset(doomo_song_book, 0, sizeof(doomo_song_book));
    studio_stats_reset();
}

static int ext_read(const uint8_t *src, uint32_t len, uint16_t version)
{
    SbIn in;
    uint32_t n, mask, i;
    StudioStats st;
    if (version < 1) return 0;
    in.src = src; in.len = len; in.pos = 0; in.err = 0;
    n = ib(&in);
    if (n < STATS_BYTES) return 0;
    memset(&st, 0, sizeof(st));
    st.saved = (uint16_t)i16(&in);
    st.performed = (uint16_t)i16(&in);
    st.inspired = (uint16_t)i16(&in);
    st.lines = (uint16_t)i16(&in);
    st.genres = (uint16_t)i16(&in);
    st.best_choir = (uint8_t)ib(&in);
    st.encores = (uint8_t)ib(&in);
    for (i = 12; i < n; i++) ib(&in);
    mask = ib(&in);
    if (in.err || (mask >> SONG_BOOK_SIZE)) return 0;
    ext_reset();
    studio_stats = st;
    for (i = 0; i < SONG_BOOK_SIZE; i++) {
        if (!(mask & (1u << i))) continue;
        if (!get_song(&in, &doomo_song_book[i])) { ext_reset(); return 0; }
        doomo_song_book[i].flags |= SONGF_VALID;
    }
    return !in.err;
}

static const DoomoSaveExt song_ext = {
    SAVE_EXT_TAG('S', 'O', 'N', 'G'), 1, ext_size, ext_write, ext_read, ext_reset
};
const DoomoSaveExt *const doomo_save_ext_studio = &song_ext;
