/* song_ext.c - ST-ADV: the Advanced song extension (owner: studio). See doomo/song_ext.h.
 *
 * Encoded page (little endian, all bytes):
 *   version, tracks, sections, arrange_len, patterns, flags
 *   per section: name, npat, npat x pattern id, tempo, key + 12, fade, bright, 4 x line
 *   arrange_len x section id
 *   per track: pan (2 bits) | env << 2 | swing << 4
 *   per pattern: u8 mask of the tracks that hold a pool block; per set bit the 32 cells run-length coded
 *     (0x00..0x7F a literal cell, 0x80 | (n-1) a run of n empty cells, 0xC0 | (n-1) a run of n cells of value 1)
 * A page that does not fit SONGX_PAGE_BYTES is not kept: the song stays classic (the studio says so).
 */
#include "doomo/doomo.h"
#include "doomo/song_ext.h"

int songx_tracks_max(void) { return SONGX_TRACKS_MAX; }

void songx_init(SongExt *x, const DoomoSong *from)
{
    int i, n;
    memset(x, 0, sizeof(*x));
    x->version = SX_VERSION;
    x->tracks = SONG_TRACKS;
    x->sections = 1;
    x->arrange_len = 1;
    x->patterns = SONG_PATTERNS;
    x->flags = SX_FLAG_ADVANCED;
    x->sect[0].name = SX_VERSE;
    x->sect[0].tempo = 100;
    memset(x->sect[0].line, SX_NO_LINE, SX_LINES);
    n = from ? DOOMO_CLAMP(from->order_len, 1, SONG_ORDER_MAX) : 1;
    n = DOOMO_MIN(n, SX_SECT_PATS);
    for (i = 0; i < n; i++) x->sect[0].pat[i] = from ? (uint8_t)(from->order[i] % SONG_PATTERNS) : 0;
    x->sect[0].npat = (uint8_t)n;
    for (i = 0; i < SX_TRACKS; i++) x->trk[i].pan = 1;
}

int songx_sanitize(SongExt *x)
{
    int n = 0, i, k, p, t;
#define FIX(c, st) do { if (c) { st; n++; } } while (0)
    FIX(x->version != SX_VERSION, x->version = SX_VERSION);
    FIX(x->tracks < SONG_TRACKS || x->tracks > SX_TRACKS, x->tracks = (uint8_t)DOOMO_CLAMP(x->tracks, SONG_TRACKS, SX_TRACKS));
    FIX(x->sections < 1 || x->sections > SX_SECTIONS, x->sections = (uint8_t)DOOMO_CLAMP(x->sections, 1, SX_SECTIONS));
    FIX(x->arrange_len < 1 || x->arrange_len > SX_ARRANGE, x->arrange_len = (uint8_t)DOOMO_CLAMP(x->arrange_len, 1, SX_ARRANGE));
    FIX(x->patterns < SONG_PATTERNS || x->patterns > SX_PATTERNS,
        x->patterns = (uint8_t)DOOMO_CLAMP(x->patterns, SONG_PATTERNS, SX_PATTERNS));
    FIX(x->flags & ~3, x->flags &= 3);
    FIX(x->nblocks > SX_POOL, x->nblocks = SX_POOL);
    for (i = 0; i < SX_SECTIONS; i++) {
        SxSection *c = &x->sect[i];
        FIX(c->name >= 32, c->name = SX_VERSE);
        FIX(c->npat < 1 || c->npat > SX_SECT_PATS, c->npat = (uint8_t)DOOMO_CLAMP(c->npat, 1, SX_SECT_PATS));
        for (k = 0; k < SX_SECT_PATS; k++) FIX(c->pat[k] >= x->patterns, c->pat[k] = 0);
        FIX(c->tempo < 50 || c->tempo > 200, c->tempo = (uint8_t)DOOMO_CLAMP(c->tempo ? c->tempo : 100, 50, 200));
        FIX(c->key < -12 || c->key > 12, c->key = (int8_t)DOOMO_CLAMP(c->key, -12, 12));
        FIX(c->fade >= SX_FADE_COUNT, c->fade = 0);
        FIX(c->bright > 3, c->bright = 3);
        for (k = 0; k < SX_LINES; k++) FIX(c->line[k] >= SONG_LYRIC_LINES && c->line[k] != SX_NO_LINE, c->line[k] = SX_NO_LINE);
    }
    for (i = 0; i < SX_ARRANGE; i++) FIX(x->arrange[i] >= x->sections, x->arrange[i] = 0);
    for (i = 0; i < SX_TRACKS; i++) {
        FIX(x->trk[i].pan > 2, x->trk[i].pan = 1);
        FIX(x->trk[i].env > 3, x->trk[i].env = 0);
        FIX(x->trk[i].swing > 8, x->trk[i].swing = 8);
    }
    for (p = 0; p < SX_PATTERNS; p++)
        for (t = 0; t < SX_TRACKS; t++) {
            uint8_t *m = &x->map[p][t];
            int dead = p < SONG_PATTERNS && t < SONG_TRACKS;     /* those cells are the classic song's: no block */
            FIX(*m > SX_POOL || (dead && *m), *m = 0);
        }
    for (i = 0; i < SX_POOL; i++)
        for (k = 0; k < SONG_STEPS; k++) {
            uint8_t *c = &x->pool[i][k];
            FIX(*c > 127, *c = 0);
        }
#undef FIX
    return n;
}

/* ---- cells -------------------------------------------------------------------------------------------- */
int songx_cell(const DoomoSong *s, const SongExt *x, int pat, int trk, int step)
{
    if (pat < 0 || pat >= SX_PATTERNS || trk < 0 || trk >= SX_TRACKS || step < 0 || step >= SONG_STEPS) return 0;
    if (pat < SONG_PATTERNS && trk < SONG_TRACKS) return s->notes[pat][trk][step];
    if (!x || !x->map[pat][trk]) return 0;
    return x->pool[x->map[pat][trk] - 1][step];
}

int songx_blocks_free(const SongExt *x) { return SX_POOL - x->nblocks; }

int songx_set_cell(DoomoSong *s, SongExt *x, int pat, int trk, int step, int v)
{
    if (pat < 0 || pat >= SX_PATTERNS || trk < 0 || trk >= SX_TRACKS || step < 0 || step >= SONG_STEPS) return 0;
    if (v < 0 || v > 127) return 0;
    if (pat < SONG_PATTERNS && trk < SONG_TRACKS) { s->notes[pat][trk][step] = (uint8_t)v; return 1; }
    if (!x->map[pat][trk]) {
        if (!v) return 1;                                  /* an empty cell stays unallocated */
        if (x->nblocks >= SX_POOL) return 0;
        x->map[pat][trk] = (uint8_t)(++x->nblocks);
        memset(x->pool[x->nblocks - 1], 0, SONG_STEPS);
    }
    x->pool[x->map[pat][trk] - 1][step] = (uint8_t)v;
    return 1;
}

/* ---- the arrangement ------------------------------------------------------------------------------------ */
static int sect_passes(const SongExt *x, int sec) { return DOOMO_CLAMP(x->sect[sec % SX_SECTIONS].npat, 1, SX_SECT_PATS); }

int songx_len(const SongExt *x)
{
    int i, n = 0, len = DOOMO_CLAMP(x->arrange_len, 1, SX_ARRANGE);
    for (i = 0; i < len; i++) n += sect_passes(x, x->arrange[i]);
    return n;
}

int songx_at(const SongExt *x, int pos, int *sect, int *pass)
{
    int i, len = DOOMO_CLAMP(x->arrange_len, 1, SX_ARRANGE);
    if (pos < 0) pos = 0;
    for (i = 0; i < len; i++) {
        int sec = x->arrange[i] % SX_SECTIONS, n = sect_passes(x, sec);
        if (pos < n) {
            if (sect) *sect = sec;
            if (pass) *pass = pos;
            return x->sect[sec].pat[pos] % SX_PATTERNS;
        }
        pos -= n;
    }
    if (sect) *sect = x->arrange[len - 1] % SX_SECTIONS;       /* past the end: the last pass */
    if (pass) *pass = sect_passes(x, x->arrange[len - 1]) - 1;
    return x->sect[x->arrange[len - 1] % SX_SECTIONS].pat[sect_passes(x, x->arrange[len - 1]) - 1] % SX_PATTERNS;
}

uint32_t songx_ticks(const DoomoSong *s, const SongExt *x)
{
    uint32_t bpm = DOOMO_CLAMP(s->bpm, 60, 220), total = 0;
    int i, len = DOOMO_CLAMP(x->arrange_len, 1, SX_ARRANGE);
    for (i = 0; i < len; i++) {
        const SxSection *c = &x->sect[x->arrange[i] % SX_SECTIONS];
        uint32_t b = bpm * DOOMO_CLAMP(c->tempo, 50, 200) / 100u;
        b = DOOMO_CLAMP(b, 30, 440);
        total += ((uint32_t)sect_passes(x, x->arrange[i]) * 14400u + b - 1) / b;
    }
    return total;
}

/* ---- folding ----------------------------------------------------------------------------------------------- */
int songx_lane_of(int trk)
{
    static const uint8_t lane[SX_TRACKS] = { 0, 1, 2, 3, 1, 1, 2, 3 };   /* 5, 6 mix into the harmony, 7 into the bass, 8 into the drums */
    return lane[DOOMO_CLAMP(trk, 0, SX_TRACKS - 1)];
}

int songx_folded(const SongExt *x, char *buf, int cap)
{
    int have = x ? x->tracks : SONG_TRACKS, play = songx_tracks_max();
    if (have <= play) { if (cap > 0) buf[0] = 0; return 0; }
    doomo_snprintf(buf, (size_t)cap, "This song has %d tracks, this console plays %d: tracks %d..%d are mixed in",
                   have, play, play + 1, have);
    return have - play;
}

/* ---- tools -------------------------------------------------------------------------------------------------- */
int songx_add_section(SongExt *x, int name)
{
    SxSection *c;
    if (x->sections >= SX_SECTIONS) return -1;
    c = &x->sect[x->sections];
    memset(c, 0, sizeof(*c));
    c->name = (uint8_t)DOOMO_CLAMP(name, 0, 31);
    c->npat = 1;
    c->tempo = 100;
    memset(c->line, SX_NO_LINE, SX_LINES);
    return x->sections++;
}

int songx_arrange_push(SongExt *x, int sect)
{
    if (sect < 0 || sect >= x->sections || x->arrange_len >= SX_ARRANGE) return 0;
    x->arrange[x->arrange_len++] = (uint8_t)sect;
    return 1;
}

int songx_last_chorus_up(SongExt *x, int steps)
{
    int i, n = 0, last = -1;
    for (i = 0; i < x->arrange_len; i++) if (x->sect[x->arrange[i] % SX_SECTIONS].name == SX_CHORUS) last = i;
    if (last < 0) return 0;
    /* the last chorus gets its own copy of the section when an earlier entry shares it, so only it changes */
    {
        int sec = x->arrange[last], shared = 0;
        for (i = 0; i < last; i++) if (x->arrange[i] == sec) shared = 1;
        if (shared) {
            int nw = songx_add_section(x, SX_CHORUS);
            if (nw < 0) return 0;
            x->sect[nw] = x->sect[sec];
            x->arrange[last] = (uint8_t)nw;
            sec = nw;
        }
        x->sect[sec].key = (int8_t)DOOMO_CLAMP(x->sect[sec].key + steps, -12, 12);
        n = 1;
    }
    return n;
}


/* ---- ST-ADV screens ------------------------------------------------------------------------------------- */
static const char *const sx_names[SX_NAMES] = { "Intro", "Verse", "Pre-chorus", "Chorus", "Bridge", "Outro" };
const char *songx_name(int name) { return name >= 0 && name < SX_NAMES ? sx_names[name] : "Part"; }

void songx_from_order(SongExt *x, const DoomoSong *s)
{
    static const uint8_t nm[4] = { SX_VERSE, SX_CHORUS, SX_BRIDGE, SX_OUTRO };
    int n = DOOMO_CLAMP(s->order_len, 1, SONG_ORDER_MAX), c, k, secs = (n + 3) / 4;
    songx_init(x, s);
    for (c = 0; c < secs; c++) {
        SxSection *q = &x->sect[c];
        int np = DOOMO_MIN(4, n - c * 4);
        if (c) { memset(q, 0, sizeof(*q)); q->tempo = 100; }
        q->name = nm[c];
        q->npat = (uint8_t)np;
        memset(q->line, SX_NO_LINE, SX_LINES);
        for (k = 0; k < np; k++) {
            q->pat[k] = (uint8_t)(s->order[c * 4 + k] % SONG_PATTERNS);
            q->line[k] = (uint8_t)SONG_LINE_OF(c * 4 + k);       /* the classic song's lyric line of that order position */
        }
        x->arrange[c] = (uint8_t)c;
    }
    x->sections = (uint8_t)secs;
    x->arrange_len = (uint8_t)secs;
}

int songx_arrange_insert(SongExt *x, int at, int sect)
{
    if (x->arrange_len >= SX_ARRANGE || sect < 0 || sect >= x->sections) return 0;
    at = DOOMO_CLAMP(at, 0, x->arrange_len);
    memmove(x->arrange + at + 1, x->arrange + at, (size_t)(x->arrange_len - at));
    x->arrange[at] = (uint8_t)sect;
    x->arrange_len++;
    return 1;
}

int songx_sect_users(const SongExt *x, int sect)
{
    int i, n = 0;
    for (i = 0; i < x->arrange_len && i < SX_ARRANGE; i++) n += x->arrange[i] == sect;
    return n;
}

int songx_arrange_remove(SongExt *x, int at)
{
    int sec, i;
    if (x->arrange_len <= 1 || at < 0 || at >= x->arrange_len) return 0;
    sec = x->arrange[at];
    memmove(x->arrange + at, x->arrange + at + 1, (size_t)(x->arrange_len - at - 1));
    x->arrange[--x->arrange_len] = 0;
    if (sec < x->sections && !songx_sect_users(x, sec) && x->sections > 1) {      /* nobody plays it: the part goes, the ids close up */
        memmove(&x->sect[sec], &x->sect[sec + 1], (size_t)(x->sections - sec - 1) * sizeof(SxSection));
        x->sections--;
        memset(&x->sect[x->sections], 0, sizeof(SxSection));
        for (i = 0; i < x->arrange_len; i++) if (x->arrange[i] > sec) x->arrange[i]--;
    }
    return 1;
}

int songx_section_dup(SongExt *x, int sect)
{
    int n;
    if (sect < 0 || sect >= x->sections) return -1;
    n = songx_add_section(x, x->sect[sect].name);
    if (n < 0) return -1;
    x->sect[n] = x->sect[sect];
    return n;
}

int songx_entry_pass(const SongExt *x, int entry)
{
    int i, n = 0;
    for (i = 0; i < entry && i < x->arrange_len; i++) n += sect_passes(x, x->arrange[i]);
    return n;
}

int songx_entry_of_pass(const SongExt *x, int pass)
{
    int i, len = DOOMO_CLAMP(x->arrange_len, 1, SX_ARRANGE);
    for (i = 0; i < len; i++) {
        int n = sect_passes(x, x->arrange[i]);
        if (pass < n) return i;
        pass -= n;
    }
    return len - 1;
}

/* ---- serialization --------------------------------------------------------------------------------------------- */
typedef struct { uint8_t *d; uint32_t cap, n; } Out;
static void ob(Out *o, uint32_t v) { if (o->n < o->cap) o->d[o->n] = (uint8_t)v; o->n++; }

static void put_cells(Out *o, const uint8_t *c)
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

uint32_t songx_encode(const DoomoSong *s, const SongExt *x, uint8_t *dst, uint32_t cap)
{
    Out o;
    int i, k, p, t;
    (void)s;
    o.d = dst; o.cap = cap; o.n = 0;
    ob(&o, x->version); ob(&o, x->tracks); ob(&o, x->sections); ob(&o, x->arrange_len); ob(&o, x->patterns); ob(&o, x->flags);
    for (i = 0; i < x->sections && i < SX_SECTIONS; i++) {
        const SxSection *c = &x->sect[i];
        ob(&o, c->name); ob(&o, c->npat);
        for (k = 0; k < c->npat && k < SX_SECT_PATS; k++) ob(&o, c->pat[k]);
        ob(&o, c->tempo); ob(&o, (uint32_t)(c->key + 12)); ob(&o, c->fade); ob(&o, c->bright);
        for (k = 0; k < SX_LINES; k++) ob(&o, c->line[k]);
    }
    for (i = 0; i < x->arrange_len && i < SX_ARRANGE; i++) ob(&o, x->arrange[i]);
    for (i = 0; i < SX_TRACKS; i++) ob(&o, (uint32_t)(x->trk[i].pan | x->trk[i].env << 2 | x->trk[i].swing << 4));
    for (p = 0; p < x->patterns && p < SX_PATTERNS; p++) {
        uint32_t m = 0;
        for (t = 0; t < SX_TRACKS; t++) if (x->map[p][t]) m |= 1u << t;
        ob(&o, m);
        for (t = 0; t < SX_TRACKS; t++) if (m & (1u << t)) put_cells(&o, x->pool[x->map[p][t] - 1]);
    }
    return o.n > cap ? 0 : o.n;
}

typedef struct { const uint8_t *s; uint32_t len, pos; int err; } In;
static uint32_t ib(In *in) { if (in->pos >= in->len) { in->err = 1; return 0; } return in->s[in->pos++]; }

static int get_cells(In *in, uint8_t *c)
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

int songx_decode(const DoomoSong *s, SongExt *x, const uint8_t *src, uint32_t len)
{
    In in;
    int i, k, p, t;
    (void)s;
    in.s = src; in.len = len; in.pos = 0; in.err = 0;
    memset(x, 0, sizeof(*x));
    x->version = (uint8_t)ib(&in);
    if (x->version != SX_VERSION) return 0;
    x->tracks = (uint8_t)ib(&in); x->sections = (uint8_t)ib(&in); x->arrange_len = (uint8_t)ib(&in);
    x->patterns = (uint8_t)ib(&in); x->flags = (uint8_t)ib(&in);
    if (in.err || x->sections < 1 || x->sections > SX_SECTIONS || x->arrange_len < 1 || x->arrange_len > SX_ARRANGE ||
        x->patterns < SONG_PATTERNS || x->patterns > SX_PATTERNS || x->tracks < SONG_TRACKS || x->tracks > SX_TRACKS) return 0;
    for (i = 0; i < x->sections; i++) {
        SxSection *c = &x->sect[i];
        c->name = (uint8_t)ib(&in); c->npat = (uint8_t)ib(&in);
        if (c->npat < 1 || c->npat > SX_SECT_PATS) return 0;
        for (k = 0; k < c->npat; k++) c->pat[k] = (uint8_t)ib(&in);
        c->tempo = (uint8_t)ib(&in); c->key = (int8_t)((int)ib(&in) - 12); c->fade = (uint8_t)ib(&in); c->bright = (uint8_t)ib(&in);
        for (k = 0; k < SX_LINES; k++) c->line[k] = (uint8_t)ib(&in);
    }
    for (i = 0; i < x->arrange_len; i++) x->arrange[i] = (uint8_t)ib(&in);
    for (i = 0; i < SX_TRACKS; i++) {
        uint32_t b = ib(&in);
        x->trk[i].pan = (uint8_t)(b & 3); x->trk[i].env = (uint8_t)((b >> 2) & 3); x->trk[i].swing = (uint8_t)(b >> 4);
    }
    for (p = 0; p < x->patterns; p++) {
        uint32_t m = ib(&in);
        for (t = 0; t < SX_TRACKS; t++) {
            if (!(m & (1u << t))) continue;
            if ((p < SONG_PATTERNS && t < SONG_TRACKS) || x->nblocks >= SX_POOL) return 0;
            x->map[p][t] = (uint8_t)(++x->nblocks);
            if (!get_cells(&in, x->pool[x->nblocks - 1])) return 0;
        }
    }
    if (in.err) return 0;
    songx_sanitize(x);
    return 1;
}

/* ---- the book's encoded pages ------------------------------------------------------------------------------------ */
#define PAGES (SONGX_BOOK_PAGES > 0 ? SONGX_BOOK_PAGES : 1)
static uint8_t book_data[PAGES][SONGX_PAGE_BYTES];
static uint16_t book_len[PAGES];

int songx_book_has(int slot) { return SONGX_BOOK_PAGES > 0 && slot >= 0 && slot < SONGX_BOOK_PAGES && book_len[slot] > 0; }

void songx_book_drop(int slot) { if (slot >= 0 && slot < PAGES) book_len[slot] = 0; }

int songx_book_put(int slot, const DoomoSong *s, const SongExt *x)
{
    uint32_t n;
    if (SONGX_BOOK_PAGES <= 0 || slot < 0 || slot >= SONGX_BOOK_PAGES) return 0;
    n = songx_encode(s, x, book_data[slot], SONGX_PAGE_BYTES);
    if (!n) return 0;
    book_len[slot] = (uint16_t)n;
    return 1;
}

int songx_book_get(int slot, const DoomoSong *s, SongExt *x)
{
    if (!songx_book_has(slot)) return 0;
    return songx_decode(s, x, book_data[slot], book_len[slot]);
}

/* the save chunk reads and writes the raw pages (song_book.c) */
uint32_t songx_book_raw(int slot, const uint8_t **p) { if (!songx_book_has(slot)) return 0; *p = book_data[slot]; return book_len[slot]; }
int songx_book_set_raw(int slot, const uint8_t *src, uint32_t len)
{
    if (SONGX_BOOK_PAGES <= 0 || slot < 0 || slot >= SONGX_BOOK_PAGES || len == 0 || len > SONGX_PAGE_BYTES) return 0;
    memcpy(book_data[slot], src, len);
    book_len[slot] = (uint16_t)len;
    return 1;
}
