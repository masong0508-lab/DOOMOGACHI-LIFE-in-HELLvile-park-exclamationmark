/* talk_lex.c - Talk T3: the word tagger of the free-text parser (owner: TK;
 * docs/TALK.md "The parser"; design section 3.2 stage 1).
 *
 * Dreamcast and native only: the PS1 has no keyboard and never references
 * this file (--gc-sections drops it there and on the GBA).
 *
 *   S0 normalise   fold case, accents, Cyrillic / Greek / fullwidth look-
 *                  alikes, leet (3>e 0>o @>a $>s 1>i or l ...), letters
 *                  repeated 3+ times to 2, dots and starred-out letters
 *                  inside a word ("s.e.x", "f*ck"), punctuation or an
 *                  invisible character glued into a word ("se,x",
 *                  "se<ZWSP>x"), spaced-out single letters ("s e x"), a
 *                  safety word split by a space into two non-words ("fu ck")
 *   tokenise       canonical words (talk_lex_canon = talkc_parse.py canon)
 *   lemmatise      -'s -s -es -ies -ed -ied -ing (+e, doubled consonant)
 *   multi-word     longest first, up to the bank's max_words ("hell gate")
 *   persons        a participant's full or first name is PERSON(slot); an
 *                  unknown word the game's lookup knows is an absent person
 *   edit distance  an unknown word of 4+ letters one edit (after the first
 *                  letter) from a known one is that word (TTF_FUZZY)
 *   tags           concept, category root, sentiment, the rule classes and
 *                  the safety word flags of TB_PARSE (talkc_parse.py)
 *   topics         spreading activation over the association network
 *   the cut        a line with more words than the token / raw-word / unit
 *                  caps hold stops at a raw word's first byte (*cut):
 *                  talk_parse() never shows what was not read and has S1
 *                  read the rest (a raw word is at most RAW_WORD_MAX bytes,
 *                  so a cut always falls after a raw word of the call)
 *
 * Everything is read from TB_PARSE through talk_db_read() a few bytes at a
 * time; no state survives a call. Stack: about 5 KB in talk_lex().
 */
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/roster.h"

#define PRS_MAGIC 0x31535250u           /* "PRS1" */
#define WORD_REC  12u

typedef struct PrsHdr {
    uint32_t nwords, words_off, ntagsets, tagsets_off, nrules, rules_off, rules_len,
             nsafe, safe_off, str_off, str_len, max_words, nclasses;
} PrsHdr;

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

/* the bank's header: 1 ok */
static int prs_hdr(PrsHdr *h)
{
    uint8_t b[64];
    if (!talk_db_read(TB_PARSE, 0, b, 64) || le32(b) != PRS_MAGIC) return 0;
    h->nwords = le32(b + 4);    h->words_off = le32(b + 8);
    h->ntagsets = le32(b + 12); h->tagsets_off = le32(b + 16);
    h->nrules = le32(b + 20);   h->rules_off = le32(b + 24);   h->rules_len = le32(b + 28);
    h->nsafe = le32(b + 32);    h->safe_off = le32(b + 36);
    h->str_off = le32(b + 40);  h->str_len = le32(b + 44);
    h->max_words = le32(b + 48); h->nclasses = le32(b + 52);
    if (h->max_words < 1) h->max_words = 1;
    if (h->max_words > 4) h->max_words = 4;
    return 1;
}

/* word i's record and key: 1 ok */
typedef struct PrsWord { uint32_t key; uint16_t concept, wflags, tagset; uint8_t nw, len; } PrsWord;
static int prs_word(const PrsHdr *h, uint32_t i, PrsWord *w, char *key, int cap)
{
    uint8_t r[WORD_REC];
    if (i >= h->nwords || !talk_db_read(TB_PARSE, h->words_off + WORD_REC * i, r, WORD_REC)) return 0;
    w->key = le32(r); w->concept = le16(r + 4); w->wflags = le16(r + 6); w->tagset = le16(r + 8);
    w->nw = r[10]; w->len = r[11];
    if (key) {
        if (w->len + 1 > cap) return 0;
        if (!talk_db_read(TB_PARSE, h->str_off + w->key, key, (uint32_t)w->len + 1u)) return 0;
        key[w->len] = 0;
    }
    return 1;
}

static int bytecmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

/* binary search a canonical key: the word index or -1 */
static int prs_find(const PrsHdr *h, const char *key, PrsWord *out)
{
    int lo = 0, hi = (int)h->nwords - 1;
    char k[96];
    PrsWord w;
    if (!key[0] || strlen(key) >= sizeof k) return -1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1, c;
        if (!prs_word(h, (uint32_t)mid, &w, k, (int)sizeof k)) return -1;
        c = bytecmp(key, k);
        if (c == 0) { if (out) *out = w; return mid; }
        if (c < 0) hi = mid - 1; else lo = mid + 1;
    }
    return -1;
}

static void prs_tags(const PrsHdr *h, uint16_t ts, uint32_t tags[4])
{
    uint8_t b[16];
    int i;
    for (i = 0; i < 4; i++) tags[i] = 0;
    if (ts == 0 || ts >= h->ntagsets || !talk_db_read(TB_PARSE, h->tagsets_off + 16u * ts, b, 16)) return;
    for (i = 0; i < 4; i++) tags[i] = le32(b + 4 * i);
}

static int find_lemma(const PrsHdr *h, const char *w, PrsWord *out);

int talk_lex_known(const char *word, uint16_t *wflags)
{
    PrsHdr h;
    PrsWord w;
    char key[TALK_TOK_LEN * 2];
    if (!prs_hdr(&h)) return 0;
    talk_lex_canon(word, key, (int)sizeof key);
    if (prs_find(&h, key, &w) < 0 && find_lemma(&h, key, &w) < 0) return 0;
    if (wflags) *wflags = w.wflags;
    return 1;
}

int talk_lex_find(const char *key, uint16_t *concept, uint16_t *wflags, uint32_t tags[4])
{
    PrsHdr h;
    PrsWord w;
    int i;
    if (!prs_hdr(&h)) return -1;
    i = prs_find(&h, key, &w);
    if (i < 0) return -1;
    if (concept) *concept = w.concept;
    if (wflags) *wflags = w.wflags;
    if (tags) prs_tags(&h, w.tagset, tags);
    return i;
}

/* ---- S0: characters ------------------------------------------------------ */
static char lower_ascii(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }
static int is_alpha(char c) { return c >= 'a' && c <= 'z'; }
static int is_digit(char c) { return c >= '0' && c <= '9'; }

/* Latin-1 letter (U+00C0..U+00FF) to ASCII: the same table as talkc_parse.py */
static char fold_latin1(unsigned u)
{
    /* the same letters as talkc_parse.py FOLD; ae and ss fold to one letter here
     * (the lexicon is ASCII: only typed input meets them) */
    static const char t[64] = "aaaaaaaceeeeiiii/nooooo/ouuuuy/s" "aaaaaaaceeeeiiii/nooooo/ouuuuy/y";
    if (u >= 0xC0 && u <= 0xFF) {
        char c = t[u - 0xC0];
        return c == '/' ? ' ' : c;
    }
    if (u == 0xB4) return '\'';
    return ' ';
}

/* the next code point of UTF-8 text (bad bytes are Latin-1); *n = bytes used */
static unsigned next_cp(const char *s, int *n)
{
    const uint8_t *p = (const uint8_t *)s;
    if (p[0] < 0x80) { *n = 1; return p[0]; }
    if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        *n = 2; return ((unsigned)(p[0] & 0x1F) << 6) | (p[1] & 0x3F);
    }
    if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *n = 3; return ((unsigned)(p[0] & 0x0F) << 12) | ((unsigned)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    }
    *n = 1;
    return p[0];
}

/* A code point folded to one ASCII character (letters lowercase); look-
 * alikes from Cyrillic, Greek and the fullwidth block count as their Latin
 * twins (input only: the lexicon is ASCII). 0 = a separator. */
static char fold_cp(unsigned u)
{
    static const struct { uint16_t u; char c; } look[] = {
        { 0x0430, 'a' }, { 0x0435, 'e' }, { 0x043E, 'o' }, { 0x0440, 'p' }, { 0x0441, 'c' },
        { 0x0443, 'y' }, { 0x0445, 'x' }, { 0x0455, 's' }, { 0x0456, 'i' }, { 0x0458, 'j' },
        { 0x0410, 'a' }, { 0x0415, 'e' }, { 0x041E, 'o' }, { 0x0420, 'p' }, { 0x0421, 'c' },
        { 0x0425, 'x' }, { 0x0405, 's' }, { 0x0406, 'i' }, { 0x041A, 'k' }, { 0x041C, 'm' },
        { 0x041D, 'h' }, { 0x0422, 't' }, { 0x0412, 'b' }, { 0x043A, 'k' }, { 0x043D, 'h' },
        { 0x03B1, 'a' }, { 0x03B5, 'e' }, { 0x03B9, 'i' }, { 0x03BA, 'k' }, { 0x03BD, 'v' },
        { 0x03BF, 'o' }, { 0x03C1, 'p' }, { 0x03C4, 't' }, { 0x03C5, 'u' }, { 0x03C7, 'x' },
        { 0x0391, 'a' }, { 0x0395, 'e' }, { 0x0399, 'i' }, { 0x039A, 'k' }, { 0x039F, 'o' },
        { 0x03A1, 'p' }, { 0x03A4, 't' }, { 0x03A7, 'x' }, { 0x0392, 'b' }, { 0x0397, 'h' },
        { 0x2018, '\'' }, { 0x2019, '\'' }, { 0x02BC, '\'' }, { 0x0060, '\'' },
    };
    unsigned i;
    if (u < 0x80) return lower_ascii((char)u);
    if (u >= 0xC0 && u <= 0xFF) return fold_latin1(u);
    if (u >= 0xFF21 && u <= 0xFF3A) return (char)('a' + (u - 0xFF21));
    if (u >= 0xFF41 && u <= 0xFF5A) return (char)('a' + (u - 0xFF41));
    if (u >= 0xFF10 && u <= 0xFF19) return (char)('0' + (u - 0xFF10));
    for (i = 0; i < sizeof look / sizeof look[0]; i++)
        if (look[i].u == u) return look[i].c;
    return 0;
}

int talk_lex_canon(const char *in, char *out, int cap)
{
    int o = 0, n, sp = 1;
    if (cap <= 0) return 0;
    while (*in) {
        unsigned u = next_cp(in, &n);
        char c;
        in += n;
        if (u >= 0x80 && !(u >= 0xC0 && u <= 0xFF) && u != 0xB4 && u != 0x2018 && u != 0x2019) c = ' ';
        else c = u < 0x80 ? lower_ascii((char)u) : fold_cp(u);
        if (c == '.') continue;
        if (!(is_alpha(c) || is_digit(c) || c == '\'')) c = ' ';
        if (c == '\'') {
            /* an apostrophe only inside a word */
            if (sp || !(is_alpha(*in) || is_digit(*in) || (*in >= 'A' && *in <= 'Z'))) { c = ' '; }
        }
        if (c == ' ') {
            if (!sp && o < cap - 1) out[o++] = ' ';
            sp = 1;
            continue;
        }
        if (o < cap - 1) out[o++] = c;
        sp = 0;
    }
    while (o > 0 && out[o - 1] == ' ') o--;
    out[o] = 0;
    return o;
}

/* ---- the raw words of the input ----------------------------------------- */
#define RAW_MAX  48
#define UNIT_MAX 64
#define RAW_WORD_MAX 40        /* bytes of one raw word: at most 20 units, so 40 tokens or 64 units
                                * never come from one raw word and a cut is always after one */
typedef struct Raw { uint8_t start, end, kind; } Raw;       /* byte offsets in the input */
typedef struct Unit {
    char    s[TALK_TOK_LEN];
    uint8_t raw;           /* its raw word */
    uint8_t joined;        /* covers raw words up to this one */
    uint8_t leet;          /* spelled with leet characters */
    uint8_t fuzzy;         /* corrected at edit distance 1 */
} Unit;

/* invisible format characters (soft hyphen, zero-width space / joiners,
 * direction marks, word joiner, BOM, variation selectors, Hangul fillers):
 * they split raw words like any separator, but one of them between two
 * parts is no gap for the glue pass ("s<SHY>ex" is "sex") */
static int invisible(unsigned u)
{
    return u == 0xAD || u == 0x34F || u == 0x61C || u == 0x115F || u == 0x1160 || u == 0x17B4 || u == 0x17B5 ||
           (u >= 0x180B && u <= 0x180E) || (u >= 0x200B && u <= 0x200F) || (u >= 0x202A && u <= 0x202E) ||
           (u >= 0x2060 && u <= 0x206F) || u == 0x3164 || (u >= 0xFE00 && u <= 0xFE0F) || u == 0xFEFF ||
           u == 0xFFA0;
}

static int raw_char(unsigned u)
{
    if (u >= 0x80) return fold_cp(u) != 0 && fold_cp(u) != ' ';
    return is_alpha(lower_ascii((char)u)) || is_digit((char)u) || u == '\'' || u == '@' || u == '$' ||
           u == '-' || u == '.' || u == '_' || u == '*' || u == '#' || u == '+';
}

/* the raw words; *cut = the byte of the first one past max (0: all read) */
static int split_raw(const char *s, Raw *r, int max, int *cut)
{
    int i = 0, n = 0, len = (int)strlen(s);
    *cut = 0;
    if (len > TALK_INPUT_MAX * 2) len = TALK_INPUT_MAX * 2;
    while (i < len) {
        int k;
        unsigned u = next_cp(s + i, &k);
        if (n == max && (u == '?' || u == '!' || raw_char(u))) {
            *cut = i;                                  /* more words than the table holds */
            break;
        }
        if (u == '?' || u == '!') {
            int st = i;
            while (i < len && (s[i] == '?' || s[i] == '!')) i++;
            r[n].start = (uint8_t)st; r[n].end = (uint8_t)i; r[n].kind = (uint8_t)(u == '?' ? TTK_QMARK : TTK_EXCL);
            /* "?!" counts as a question */
            { int j; for (j = st; j < i; j++) if (s[j] == '?') r[n].kind = TTK_QMARK; }
            n++;
            continue;
        }
        if (!raw_char(u)) { i += k; continue; }
        r[n].start = (uint8_t)i;
        while (i < len) {
            u = next_cp(s + i, &k);
            if (!raw_char(u) || (i > r[n].start && i + k - r[n].start > RAW_WORD_MAX)) break;
            i += k;
        }
        r[n].end = (uint8_t)i;
        r[n].kind = TTK_WORD;
        n++;
    }
    return n;
}

static int is_leet(char c)
{
    return c == '0' || c == '1' || c == '3' || c == '4' || c == '5' || c == '7' || c == '8' || c == '9' ||
           c == '@' || c == '$';
}
static char leet_of(char c, int one_is_l)
{
    switch (c) {
    case '0': return 'o'; case '1': return one_is_l ? 'l' : 'i'; case '3': return 'e'; case '4': return 'a';
    case '5': return 's'; case '7': return 't'; case '8': return 'b'; case '9': return 'g';
    case '@': return 'a'; case '$': return 's';
    }
    return c;
}

/* the raw text [a, b) folded to plain characters: look-alikes, a '*', '#'
 * or '+' between letters dropped ("f*ck"), runs of 3+ letters cut to 2;
 * leet 1 / 2 folds leet characters too (2: 1 = l); leet 3 keeps the runs */
static void raw_fold(const char *s, int a, int b, int leet, char *out, int cap)
{
    char tmp[TALK_INPUT_MAX * 2 + 8];
    int o = 0, i = a, k;
    while (i < b && o < (int)sizeof tmp - 1) {
        unsigned u = next_cp(s + i, &k);
        char c = u < 0x80 ? lower_ascii((char)u) : fold_cp(u);
        i += k;
        if (!c) c = ' ';
        if (leet && leet < 3 && is_leet(c)) {
            /* a leet digit only inside a word that has letters */
            c = leet_of(c, leet == 2);
        }
        if ((c == '*' || c == '#' || c == '+') && o > 0 && i < b &&
            (is_alpha(tmp[o - 1]) || is_digit(tmp[o - 1]))) {
            unsigned v = next_cp(s + i, &k);
            char d = v < 0x80 ? lower_ascii((char)v) : fold_cp(v);
            if (is_alpha(d) || (leet && is_leet(d))) continue;        /* f*ck */
        }
        if (leet != 3 && o >= 2 && is_alpha(c) && tmp[o - 1] == c && tmp[o - 2] == c) continue;   /* sooo -> soo */
        tmp[o++] = c;
    }
    tmp[o] = 0;
    talk_lex_canon(tmp, out, cap);
}

/* the bytes [a, b) between two raw words are one mark (no space) or one
 * invisible character: the words may be one word cut in two */
static int one_mark(const char *s, int a, int b)
{
    int k;
    unsigned u;
    if (b - a == 1) return s[a] != ' ' && s[a] != '\t';
    if (b <= a) return 0;
    u = next_cp(s + a, &k);
    return a + k == b && invisible(u);
}

/* ---- lemmas and edit distance --------------------------------------------- */
static int ends(const char *w, int n, const char *suf)
{
    int k = (int)strlen(suf);
    return n > k + 1 && memcmp(w + n - k, suf, (size_t)k) == 0;
}

/* a lemma of w that the lexicon knows: its index, or -1 */
static int find_lemma(const PrsHdr *h, const char *w, PrsWord *out)
{
    char b[TALK_TOK_LEN + 2];
    int n = (int)strlen(w), r;
    if (n < 3 || n >= TALK_TOK_LEN) return -1;
#define TRY(len, add) do { memcpy(b, w, (size_t)(len)); strcpy(b + (len), add); \
        if ((r = prs_find(h, b, out)) >= 0) return r; } while (0)
    if (ends(w, n, "'s")) TRY(n - 2, "");
    if (ends(w, n, "s'")) TRY(n - 1, "");
    if (ends(w, n, "ies")) TRY(n - 3, "y");
    if (ends(w, n, "ied")) TRY(n - 3, "y");
    if (ends(w, n, "es")) TRY(n - 2, "");
    if (ends(w, n, "s") && !ends(w, n, "ss")) TRY(n - 1, "");
    if (ends(w, n, "ing")) {
        TRY(n - 3, "");
        TRY(n - 3, "e");
        if (n >= 5 && w[n - 4] == w[n - 5]) TRY(n - 4, "");
    }
    if (ends(w, n, "in") && n >= 5) { TRY(n - 2, "ing"); }          /* "lovin" */
    if (ends(w, n, "ed")) {
        TRY(n - 2, "");
        TRY(n - 1, "");
        if (n >= 5 && w[n - 3] == w[n - 4]) TRY(n - 3, "");
    }
#undef TRY
    return -1;
}

/* the weaker lemma rules (-est, -er, -ly): tried after the safety words,
 * so "morest" is "molest", not "more" */
static int find_lemma_weak(const PrsHdr *h, const char *w, PrsWord *out)
{
    char b[TALK_TOK_LEN + 2];
    int n = (int)strlen(w), r;
    if (n < 6 || n >= TALK_TOK_LEN) return -1;
#define TRY(len, add) do { memcpy(b, w, (size_t)(len)); strcpy(b + (len), add); \
        if ((r = prs_find(h, b, out)) >= 0) return r; } while (0)
    if (ends(w, n, "est")) {                                          /* sweetest, nicest, biggest */
        TRY(n - 3, "");
        TRY(n - 2, "");
        if (w[n - 4] == w[n - 5]) TRY(n - 4, "");
        if (w[n - 4] == 'i') TRY(n - 4, "y");
    }
    if (ends(w, n, "er")) TRY(n - 2, "");
    if (ends(w, n, "ly")) TRY(n - 2, "");
#undef TRY
    return -1;
}

/* edit distance 1, the first letter kept: the first variant the lexicon
 * knows (transpositions, deletions, substitutions, insertions, in that
 * order: the commonest typos first; deterministic). Single-word keys only. */
static int find_fuzzy(const PrsHdr *h, const char *w, PrsWord *out)
{
    char b[TALK_TOK_LEN + 2];
    int n = (int)strlen(w), i, r;
    char c;
    if (n < 4 || n >= TALK_TOK_LEN - 1) return -1;
    for (i = 1; i + 1 < n; i++) {                              /* transposition */
        strcpy(b, w); c = b[i]; b[i] = b[i + 1]; b[i + 1] = c;
        if (b[i] != b[i + 1] && (r = prs_find(h, b, out)) >= 0 && out->nw == 1) return r;
    }
    for (i = 1; i < n; i++) {                                  /* deletion */
        memcpy(b, w, (size_t)i); strcpy(b + i, w + i + 1);
        if ((r = prs_find(h, b, out)) >= 0 && out->nw == 1) return r;
    }
    for (i = 1; i < n; i++) {                                  /* substitution */
        strcpy(b, w);
        for (c = 'a'; c <= 'z'; c++) {
            if (c == w[i]) continue;
            b[i] = c;
            if ((r = prs_find(h, b, out)) >= 0 && out->nw == 1) return r;
        }
    }
    for (i = 1; i <= n; i++) {                                 /* insertion */
        memcpy(b, w, (size_t)i); strcpy(b + i + 1, w + i);
        for (c = 'a'; c <= 'z'; c++) {
            b[i] = c;
            if ((r = prs_find(h, b, out)) >= 0 && out->nw == 1) return r;
        }
    }
    return -1;
}

/* one edit apart, the first letter kept (a and b differ) */
static int ed1_first(const char *a, const char *b)
{
    int la = (int)strlen(a), lb = (int)strlen(b), i = 0, j;
    if (!la || !lb || a[0] != b[0]) return 0;
    if (la == lb) {
        int d = 0, first = -1;
        for (j = 0; j < la; j++) if (a[j] != b[j]) { if (first < 0) first = j; d++; }
        if (d == 1) return 1;
        return d == 2 && first + 1 < la && a[first] == b[first + 1] && a[first + 1] == b[first] &&
               strcmp(a + first + 2, b + first + 2) == 0;                     /* transposed */
    }
    if (la + 1 == lb) { const char *t = a; a = b; b = t; la++; lb--; }
    if (la != lb + 1) return 0;
    while (i < lb && a[i] == b[i]) i++;
    return strcmp(a + i + 1, b + i) == 0;
}

/* a safety word (sexual, hate, distress, minor) one edit from w: its index,
 * or -1. Safety words win over every other near miss ("s3xx", "sexi"). */
static int find_safety_fuzzy(const PrsHdr *h, const char *w, PrsWord *out)
{
    uint32_t k;
    int n = (int)strlen(w);
    char key[TALK_TOK_LEN + 8];
    if (n < 4 || n >= TALK_TOK_LEN) return -1;            /* 3 letters: too many real words one edit away */
    for (k = 0; k < h->nsafe; k++) {
        uint8_t b[2];
        uint16_t ix;
        if (!talk_db_read(TB_PARSE, h->safe_off + 2u * k, b, 2)) return -1;
        ix = le16(b);
        if (!prs_word(h, ix, out, key, (int)sizeof key)) continue;
        if ((int)strlen(key) < 3 || (n < 4 && (int)strlen(key) < 3)) continue;
        if (ed1_first(w, key)) return ix;
    }
    return -1;
}

/* every single-word key at edit distance 1 (first letter kept), in
 * find_fuzzy's order: their word indices, at most max */
static int ed1_candidates(const PrsHdr *h, const char *w, uint16_t *out, int max)
{
    char b[TALK_TOK_LEN + 2];
    int n = (int)strlen(w), i, r, k = 0, j;
    char c;
    PrsWord pw;
#define ADD() do { if ((r = prs_find(h, b, &pw)) >= 0 && pw.nw == 1) { \
        for (j = 0; j < k && out[j] != r; j++) {} if (j == k && k < max) out[k++] = (uint16_t)r; } } while (0)
    if (n < 3 || n >= TALK_TOK_LEN - 1) return 0;
    for (i = 1; i + 1 < n; i++) {
        strcpy(b, w); c = b[i]; b[i] = b[i + 1]; b[i + 1] = c;
        if (b[i] != b[i + 1]) ADD();
    }
    for (i = 1; i < n; i++) { memcpy(b, w, (size_t)i); strcpy(b + i, w + i + 1); if (n > 3) ADD(); }
    for (i = 1; i < n; i++) {
        strcpy(b, w);
        for (c = 'a'; c <= 'z'; c++) { if (c == w[i]) continue; b[i] = c; ADD(); }
    }
    for (i = 1; i <= n; i++) {
        memcpy(b, w, (size_t)i); strcpy(b + i + 1, w + i);
        for (c = 'a'; c <= 'z'; c++) { b[i] = c; ADD(); }
    }
#undef ADD
    return k;
}

/* ---- persons -------------------------------------------------------------- */
typedef struct Names {
    char full[TALK_PARTS_MAX][32];
    char first[TALK_PARTS_MAX][TALK_TOK_LEN];
    char alias[TALK_PARTS_MAX][TALK_TOK_LEN];     /* "doctor" for "Dr. Bloom" */
    uint8_t title[TALK_PARTS_MAX];               /* the first word is a title (Mayor, Dr.) */
    int n;
} Names;

/* titles: a name that starts with one is called by it only as a vocative
 * ("thanks, mayor") or with "the" ("the mayor is right"), never in "a real
 * doctor" or "the best mayor ever" */
static const char *const titles[][2] = {
    { "mayor", "mayor" }, { "dr", "doctor" }, { "doctor", "doc" }, { "doc", "doctor" }, { "father", "father" },
    { "mother", "mother" }, { "lord", "lord" }, { "lady", "lady" }, { "miss", "miss" }, { "mr", "mister" },
    { "mrs", "missus" }, { "prof", "professor" }, { "professor", "prof" }, { "capt", "captain" },
    { "captain", "capt" }, { "chief", "chief" }, { "sir", "sir" }, { "dame", "dame" }, { "duke", "duke" },
    { "uncle", "uncle" }, { "aunt", "auntie" }, { "granny", "granny" }, { "grandma", "grandma" },
    { "grandpa", "grandpa" }, { "sister", "sister" }, { "brother", "brother" },
};

static void names_of(const TalkParseCtx *ctx, Names *nm)
{
    int i;
    memset(nm, 0, sizeof *nm);
    nm->n = ctx ? ctx->nparts : 0;
    if (nm->n > TALK_PARTS_MAX) nm->n = TALK_PARTS_MAX;
    for (i = 0; i < nm->n; i++) {
        char *sp;
        if (!ctx->name[i]) continue;
        talk_lex_canon(ctx->name[i], nm->full[i], (int)sizeof nm->full[i]);
        memcpy(nm->first[i], nm->full[i], TALK_TOK_LEN - 1);
        nm->first[i][TALK_TOK_LEN - 1] = 0;
        sp = strchr(nm->first[i], ' ');
        if (sp) {
            unsigned k;
            *sp = 0;
            for (k = 0; k < sizeof titles / sizeof titles[0]; k++)
                if (strcmp(nm->first[i], titles[k][0]) == 0) {
                    nm->title[i] = 1;
                    memcpy(nm->alias[i], titles[k][1], strlen(titles[k][1]) + 1);
                }
        }
    }
}

/* units from u[i]: words joined with spaces (n units) */
static int join_units(const Unit *u, int nu, int i, int n, char *out, int cap)
{
    int k, o = 0;
    if (i + n > nu) return 0;
    for (k = 0; k < n; k++) {
        int l = (int)strlen(u[i + k].s);
        if (o + l + 2 > cap) return 0;
        if (k) out[o++] = ' ';
        memcpy(out + o, u[i + k].s, (size_t)l);
        o += l;
    }
    out[o] = 0;
    return 1;
}

/* a person at units i..: the units used (0 none); *slot. voc: unit i is
 * said as a vocative (the line's first word, or after a comma) */
static int match_person(const Names *nm, const Unit *u, int nu, int i, int *slot, int voc)
{
    int s, best = 0;
    char b[64];
    for (s = 0; s < nm->n; s++) {
        int words = 1;
        const char *p;
        if (!nm->full[s][0]) continue;
        for (p = nm->full[s]; *p; p++) if (*p == ' ') words++;
        if (words > 1 && words > best && join_units(u, nu, i, words, b, (int)sizeof b) &&
            strcmp(b, nm->full[s]) == 0) { best = words; *slot = s; continue; }
        if (!best && nm->title[s]) {
            /* a title: "the mayor" anywhere, "mayor" / "doctor" as a vocative */
            const char *w = u[i].s;
            int the = strcmp(w, "the") == 0 && i + 1 < nu;
            const char *t = the ? u[i + 1].s : w;
            if ((the || voc) && (strcmp(t, nm->first[s]) == 0 || strcmp(t, nm->alias[s]) == 0)) {
                best = the ? 2 : 1; *slot = s;
            }
            continue;
        }
        if (!best) {
            const char *w = u[i].s;
            int l = (int)strlen(nm->first[s]);
            if (strcmp(w, nm->first[s]) == 0 ||
                (strncmp(w, nm->first[s], (size_t)l) == 0 && strcmp(w + l, "'s") == 0)) {
                best = 1; *slot = s;
                if (w[l]) *slot |= 0x100;                      /* possessive */
            }
        }
    }
    return best;
}

/* ---- concept facts --------------------------------------------------------- */
static void concept_facts(TalkTok *t)
{
    TalkConcept c;
    t->root = 0; t->sent = 0; t->cat = 0;
    if (t->concept && talk_db_concept(t->concept, &c)) {
        t->cat = c.cat;
        t->sent = c.sent;
        t->root = (uint8_t)talk_db_cat_root(c.cat);
    }
}

static void set_word(const PrsHdr *h, TalkTok *t, const PrsWord *w)
{
    t->concept = w->concept;
    t->wflags = w->wflags;
    prs_tags(h, w->tagset, t->tags);
    concept_facts(t);
}

static int all_digits(const char *s)
{
    if (!*s) return 0;
    for (; *s; s++) if (!is_digit(*s)) return 0;
    return 1;
}

/* A person named by a word the lexicon knows (the cast: "Pip", "Wisp") is
 * a word token, not a PERSON; TalkTok.person gets an id so S1 can ask
 * talk_safe_minor() about it: the game's lookup (the concept's name, folded,
 * then its first word) and the roster's own cast member of that exact name
 * ("Wisp" is the kid, roster 563, not "Wisp Willow"). When the two differ
 * and one of them is a child, the child (fail safe). */
static void named_person(const TalkParseCtx *ctx, TalkTok *t)
{
    TalkConcept c;
    char name[48], key[48], *sp;
    int id = -1, r;
    if (t->kind != TTK_WORD || !t->concept || t->person >= 0) return;
    if (!talk_db_concept(t->concept, &c) || !(c.flags & TCF_PROPER)) return;
    if (talk_db_form(t->concept, TF_LEMMA, name, (int)sizeof name) <= 0) return;
    talk_lex_canon(name, key, (int)sizeof key);
    if (!key[0]) return;
    if (ctx && ctx->person) {
        id = ctx->person(key);
        if (id < 0 && (sp = strchr(key, ' ')) != NULL) {
            *sp = 0;
            id = ctx->person(key);
        }
    }
    r = roster_find(name);
    if (r >= 0 && (id < 0 || (r != id && talk_safe_minor(r) && !talk_safe_minor(id)))) id = r;
    if (id >= 0) t->person = (int16_t)id;
}

/* ---- the tagger ------------------------------------------------------------ */
int talk_lex(const char *text, const TalkParseCtx *ctx, TalkTok *tok, int max)
{
    return talk_lex_cut(text, ctx, tok, max, NULL);
}

int talk_lex_cut(const char *text, const TalkParseCtx *ctx, TalkTok *tok, int max, int *cut_out)
{
    PrsHdr h;
    Raw raw[RAW_MAX];
    Unit u[UNIT_MAX];
    Names nm;
    int nraw, nu = 0, i, nt = 0, cut = 0;
    if (cut_out) *cut_out = 0;
    if (!text || !tok || max <= 0 || !prs_hdr(&h)) return 0;
    names_of(ctx, &nm);
    nraw = split_raw(text, raw, RAW_MAX, &cut);
    /* raw words -> units (canonical words); QMARK / EXCL stay units of their own.
     * A raw word whose units do not all fit is left out whole: the cut is
     * its first byte (there is always a raw word before it: RAW_WORD_MAX) */
    for (i = 0; i < nraw; i++) {
        char plain[TALK_INPUT_MAX * 2], leet[TALK_INPUT_MAX * 2], *p, *q;
        int has_leet = 0, j, nu0 = nu, full = 0;
        if (nu >= UNIT_MAX) { cut = raw[i].start; break; }
        if (raw[i].kind != TTK_WORD) {
            memset(&u[nu], 0, sizeof u[nu]);
            u[nu].s[0] = raw[i].kind == TTK_QMARK ? '?' : '!';
            u[nu].raw = (uint8_t)i; u[nu].joined = (uint8_t)i;
            nu++;
            continue;
        }
        raw_fold(text, raw[i].start, raw[i].end, 0, plain, (int)sizeof plain);
        {
            /* letters said 3+ times may be the word itself ("kkk", "kkkkk") */
            char full[TALK_INPUT_MAX * 2], three[TALK_INPUT_MAX * 2];
            int a, o = 0;
            raw_fold(text, raw[i].start, raw[i].end, 3, full, (int)sizeof full);
            for (a = 0; full[a] && o < (int)sizeof three - 1; a++)
                if (!(o >= 3 && full[a] == three[o - 1] && full[a] == three[o - 2] && full[a] == three[o - 3]))
                    three[o++] = full[a];
            three[o] = 0;
            if (strcmp(full, plain) != 0 && !strchr(full, ' ')) {
                if (prs_find(&h, full, NULL) >= 0) memcpy(plain, full, sizeof plain);
                else if (prs_find(&h, three, NULL) >= 0) memcpy(plain, three, sizeof plain);
            }
        }
        for (j = raw[i].start; j < raw[i].end; j++) if (is_leet(text[j])) has_leet = 1;
        leet[0] = 0;
        if (has_leet) {
            /* a leet spelling only for a word with letters in it ("h3ll0", "$ex"), not "10" */
            int letters = 0;
            for (j = raw[i].start; j < raw[i].end; j++) if (is_alpha(lower_ascii(text[j]))) letters = 1;
            if (letters) {
                raw_fold(text, raw[i].start, raw[i].end, 1, leet, (int)sizeof leet);
                if (prs_find(&h, leet, NULL) < 0) {
                    char l2[TALK_INPUT_MAX * 2];
                    raw_fold(text, raw[i].start, raw[i].end, 2, l2, (int)sizeof l2);
                    if (prs_find(&h, l2, NULL) >= 0) memcpy(leet, l2, sizeof l2);
                }
            }
        }
        /* a leet spelling replaces the plain one when the plain one is not a
         * word and the leet one is (every part known), or is one edit from a
         * safety word ("s3xx") */
        if (leet[0] && !all_digits(plain)) {
            PrsWord lw;
            if (!strchr(leet, ' ') && prs_find(&h, leet, &lw) >= 0 &&
                (lw.wflags & (TW_SEXUAL | TW_SUGGEST | TW_HATE | TW_DISTRESS | TW_MINOR | TW_GROUP)))
                plain[0] = 0, memcpy(plain, leet, sizeof plain), has_leet = 2;
        }
        if (leet[0] && has_leet != 2 && prs_find(&h, plain, NULL) < 0 && !all_digits(plain)) {
            int leet_known = 1;
            char tmp[TALK_INPUT_MAX * 2];
            PrsWord sw;
            memcpy(tmp, leet, sizeof tmp);
            for (p = tmp; p && *p; p = q) {
                q = strchr(p, ' ');
                if (q) *q++ = 0;
                if (prs_find(&h, p, NULL) < 0 && find_lemma(&h, p, &sw) < 0) leet_known = 0;
            }
            if (!leet_known && !strchr(leet, ' ') && find_safety_fuzzy(&h, leet, &sw) >= 0) leet_known = 1;
            if (leet_known) { memcpy(plain, leet, sizeof plain); has_leet = 2; }
        }
        /* a word spelled with dots / dashes between letters ("s-e-x") as one */
        for (p = plain; p && *p; p = q) {
            q = strchr(p, ' ');
            if (q) *q++ = 0;
            if (nu >= UNIT_MAX) { full = 1; break; }
            memset(&u[nu], 0, sizeof u[nu]);
            memcpy(u[nu].s, p, strlen(p) < TALK_TOK_LEN ? strlen(p) : TALK_TOK_LEN - 1);
            u[nu].raw = (uint8_t)i; u[nu].joined = (uint8_t)i;
            u[nu].leet = (uint8_t)(has_leet == 2);
            nu++;
        }
        if (full) { nu = nu0; cut = raw[i].start; break; }
    }
    /* spaced-out single letters ("s e x", "a h o o k e r", "m e"): inside a
     * run of single letters the longest stretch the lexicon knows is joined
     * (two letters only when they are not two words, like "u r") */
    for (i = 0; i < nu; i++) {
        int j = i, l, p, done = 0;
        char b[TALK_TOK_LEN];
        while (j < nu && u[j].s[0] && !u[j].s[1] && is_alpha(u[j].s[0])) j++;
        if (j - i < 2) continue;
        for (l = j - i; l >= 2 && !done; l--) {
            if (l >= TALK_TOK_LEN) continue;
            for (p = i; p + l <= j && !done; p++) {
                int k, words = 0;
                for (k = 0; k < l; k++) { b[k] = u[p + k].s[0]; words += prs_find(&h, u[p + k].s, NULL) >= 0; }
                b[l] = 0;
                if (l == 2 && words > 0) continue;            /* "u r", "a n...": a word is in it */
                if (prs_find(&h, b, NULL) < 0) continue;
                memcpy(u[p].s, b, (size_t)l + 1);
                u[p].joined = u[p + l - 1].joined;
                memmove(&u[p + 1], &u[p + l], sizeof(Unit) * (size_t)(nu - p - l));
                nu -= l - 1;
                done = 1;
            }
        }
        if (done) i--;                         /* look at what is left of the run again */
    }
    /* a word cut by punctuation ("se,x", "t_ake", "asia-ns"): two units of one
     * raw word join when the joined word is known and a part is not (or is a
     * single letter); two raw words with one mark or one invisible character
     * and no space between join only into a safety word; two raw words with
     * one space between join only into a safety word, and only when neither
     * is a word ("fu ck", "se x"; never "pen is") */
    for (i = 0; i + 1 < nu; i++) {
        char b[TALK_TOK_LEN * 2];
        PrsWord w;
        int la = (int)strlen(u[i].s), lb = (int)strlen(u[i + 1].s), glued, spaced = 0, ka, kb;
        if (!u[i].s[0] || !u[i + 1].s[0] || u[i].s[0] == '?' || u[i].s[0] == '!' ||
            u[i + 1].s[0] == '?' || u[i + 1].s[0] == '!' || la + lb >= TALK_TOK_LEN) continue;
        glued = u[i].raw == u[i + 1].raw ||
                (u[i + 1].raw == u[i].raw + 1 && one_mark(text, raw[u[i].raw].end, raw[u[i + 1].raw].start));
        if (!glued && u[i + 1].raw == u[i].raw + 1 && la + lb >= 3 &&
            raw[u[i + 1].raw].start == raw[u[i].raw].end + 1 && text[raw[u[i].raw].end] == ' ')
            spaced = 1;
        if (!glued && !spaced) continue;
        memcpy(b, u[i].s, (size_t)la);
        memcpy(b + la, u[i + 1].s, (size_t)lb + 1);
        ka = prs_find(&h, u[i].s, NULL) >= 0;
        kb = prs_find(&h, u[i + 1].s, NULL) >= 0;
        if (spaced && (ka || kb || find_lemma(&h, u[i].s, &w) >= 0 || find_lemma(&h, u[i + 1].s, &w) >= 0 ||
                       all_digits(u[i].s) || all_digits(u[i + 1].s) ||
                       (prs_find(&h, b, &w) < 0 && !talk_safe_hate_word(b, 0))))
            continue;
        if (spaced && prs_find(&h, b, &w) >= 0 &&
            !(w.wflags & (TW_SEXUAL | TW_SUGGEST | TW_HATE | TW_DISTRESS | TW_MINOR | TW_STRONG | TW_GROUP)) &&
            !talk_safe_hate_word(b, 0))
            continue;
        /* two known words stay two ("well-known"), unless one raw word joins
         * into a safety word ("hot_tie", "pen.is") */
        if (ka && kb && la > 2 && lb > 2 && u[i].raw != u[i + 1].raw) continue;
        if (!((prs_find(&h, b, &w) >= 0 &&
               ((w.wflags & (TW_SEXUAL | TW_SUGGEST | TW_HATE | TW_DISTRESS | TW_MINOR | TW_STRONG | TW_GROUP)) ||
                (u[i].raw == u[i + 1].raw && (!ka || !kb || la == 1 || lb == 1)))) ||
              talk_safe_hate_word(b, 0))) continue;
        memcpy(u[i].s, b, strlen(b) + 1);
        u[i].joined = u[i + 1].joined;
        memmove(&u[i + 1], &u[i + 2], sizeof(Unit) * (size_t)(nu - i - 2));
        nu--;
    }
    /* a misspelled word ("kill myslef", "white powe"): an unknown unit that
     * is no name is corrected before the phrases are matched. Letters
     * stretched for effect come back first ("meee", "boneeer"); then an
     * edit-distance-1 word that completes a phrase with its neighbours wins,
     * then a safety word, then (4+ letters) the first word found. A 3-letter
     * unit only takes a phrase word or a 4-letter safety word ("prn", "slt"). */
    for (i = 0; i < nu; i++) {
        PrsWord w;
        char key[TALK_TOK_LEN + 8], c1[TALK_TOK_LEN];
        uint16_t cand[16];
        int k, nc, pick = -1, name = 0, len = (int)strlen(u[i].s);
        if (len < 3 || len >= TALK_TOK_LEN || all_digits(u[i].s) || u[i].s[0] == '?' || u[i].s[0] == '!' ||
            prs_find(&h, u[i].s, NULL) >= 0 || find_lemma(&h, u[i].s, &w) >= 0) continue;
        for (k = 0; k < nm.n; k++)
            if (strcmp(u[i].s, nm.first[k]) == 0 || strcmp(u[i].s, nm.alias[k]) == 0 ||
                strncmp(u[i].s, nm.full[k], (size_t)len) == 0) name = 1;
        if (name || (ctx && ctx->person && ctx->person(u[i].s) >= 0)) continue;
        /* doubled letters made single ("mee" -> "me") */
        {
            int a, o = 0;
            for (a = 0; u[i].s[a]; a++) if (!o || c1[o - 1] != u[i].s[a]) c1[o++] = u[i].s[a];
            c1[o] = 0;
            if (o != len && o >= 2 && prs_find(&h, c1, NULL) >= 0) {
                memcpy(u[i].s, c1, (size_t)o + 1);
                continue;
            }
        }
        nc = ed1_candidates(&h, u[i].s, cand, 16);
        for (k = 0; k < nc && pick < 0; k++)                      /* a safety word first: refusing is the safe error */
            if (prs_word(&h, cand[k], &w, key, (int)sizeof key) &&
                (w.wflags & (TW_SEXUAL | TW_SUGGEST | TW_HATE | TW_DISTRESS | TW_MINOR | TW_GROUP)) &&
                (len >= 4 || (int)strlen(key) == 4)) pick = k;
        for (k = 0; k < nc && pick < 0; k++) {
            char b[96];
            int m;
            if (!prs_word(&h, cand[k], &w, key, (int)sizeof key)) continue;
            /* a phrase with the neighbours: every window of 2..4 units that holds it */
            for (m = 2; m <= (int)h.max_words && pick < 0; m++) {
                int a0;
                for (a0 = i - m + 1; a0 <= i && pick < 0; a0++) {
                    int o = 0, a, a1 = a0 + m - 1;
                    if (a0 < 0 || a1 >= nu) continue;
                    for (a = a0; a <= a1; a++) {
                        const char *t = a == i ? key : u[a].s;
                        int l = (int)strlen(t);
                        if (o + l + 2 > (int)sizeof b) break;
                        if (a > a0) b[o++] = ' ';
                        memcpy(b + o, t, (size_t)l);
                        o += l;
                    }
                    b[o] = 0;
                    if (a > a1 && prs_find(&h, b, NULL) >= 0) pick = k;
                }
            }
        }
        for (k = 0; k < nc && pick < 0; k++)
            if (prs_word(&h, cand[k], &w, key, (int)sizeof key) &&
                (w.wflags & (TW_SEXUAL | TW_SUGGEST | TW_HATE | TW_DISTRESS | TW_MINOR | TW_GROUP)) &&
                (len >= 4 || (int)strlen(key) == 4)) pick = k;
        if (pick < 0 && len >= 4 && nc > 0) pick = 0;
        if (pick < 0 || !prs_word(&h, cand[pick], &w, key, (int)sizeof key) || strlen(key) >= TALK_TOK_LEN) continue;
        memcpy(u[i].s, key, strlen(key) + 1);
        u[i].fuzzy = 1;
    }
    /* tokens: persons and multi-word keys longest first */
    for (i = 0; i < nu && nt < max; ) {
        TalkTok *t = &tok[nt];
        PrsWord w;
        int used = 0, slot = -1, pn, n, idx = -1;
        char b[96];
        memset(t, 0, sizeof *t);
        t->person = -1;
        t->src0 = raw[u[i].raw].start;
        if (u[i].leet) t->flags |= TTF_LEET;
        if (u[i].fuzzy) t->flags |= TTF_FUZZY;
        if (u[i].s[0] == '?' || u[i].s[0] == '!') {
            t->kind = (uint8_t)(u[i].s[0] == '?' ? TTK_QMARK : TTK_EXCL);
            t->text[0] = u[i].s[0];
            t->src1 = raw[u[i].joined].end;
            nt++; i++;
            continue;
        }
        {
            /* a vocative: the first word, after a comma, or the last word after a comma */
            int voc = i == 0, j;
            if (i > 0) {
                for (j = raw[u[i - 1].joined].end; j < raw[u[i].raw].start; j++) if (text[j] == ',') voc = 1;
            }
            pn = match_person(&nm, u, nu, i, &slot, voc);
        }
        for (n = (int)h.max_words; n >= 1; n--) {
            if (!join_units(u, nu, i, n, b, (int)sizeof b)) continue;
            idx = prs_find(&h, b, &w);
            if (idx >= 0) { used = n; break; }
            if (n >= 2) {
                /* a phrase with a stretched little word in it ("want tooo die", "better ooff dead") */
                char c[96];
                int a, o = 0;
                for (a = 0; b[a] && o < (int)sizeof c - 1; a++)
                    if (!(o > 0 && is_alpha(b[a]) && c[o - 1] == b[a])) c[o++] = b[a];
                c[o] = 0;
                if (strcmp(c, b) != 0 && (idx = prs_find(&h, c, &w)) >= 0) { used = n; memcpy(b, c, (size_t)o + 1); break; }
            }
        }
        if (pn && pn >= used) {
            t->kind = TTK_PERSON;
            if (slot & 0x100) t->flags |= TTF_POSS;
            slot &= 0xFF;
            t->person = (int16_t)slot;
            join_units(u, nu, i, pn, b, (int)sizeof b);
            memcpy(t->text, b, strlen(b) < TALK_TOK_LEN ? strlen(b) : TALK_TOK_LEN - 1);
            /* the person's name may also be a concept (the cast): keep it for gossip / topic */
            if (used == pn) set_word(&h, t, &w);
            t->wflags |= TW_FUNC;
            t->src1 = raw[u[i + pn - 1].joined].end;
            nt++; i += pn;
            continue;
        }
        if (used) {
            memcpy(t->text, b, strlen(b) < TALK_TOK_LEN ? strlen(b) : TALK_TOK_LEN - 1);
            if (used == 1 && (w.wflags & TW_NAME) && ctx && ctx->person) {
                /* a first name of names_<lang>.txt: a resident who has it is a person */
                int id = ctx->person(b);
                if (id >= 0) {
                    t->kind = TTK_PERSON;
                    t->person = (int16_t)id;
                    t->flags |= TTF_ABSENT;
                    t->wflags |= TW_FUNC;
                    t->src1 = raw[u[i].joined].end;
                    nt++; i++;
                    continue;
                }
            }
            set_word(&h, t, &w);
            t->src1 = raw[u[i + used - 1].joined].end;
            nt++; i += used;
            continue;
        }
        /* one unknown unit */
        memcpy(t->text, u[i].s, TALK_TOK_LEN);
        t->src1 = raw[u[i].joined].end;
        if (all_digits(u[i].s)) {
            int v = 0, k;
            for (k = 0; u[i].s[k] && v < 1000; k++) v = v * 10 + (u[i].s[k] - '0');
            t->kind = TTK_NUM;
            t->value = (uint8_t)(v > 255 ? 255 : v);
            nt++; i++;
            continue;
        }
        {
            /* "12yo", "9yr": a number glued to a unit */
            int k = 0;
            while (is_digit(u[i].s[k])) k++;
            if (k > 0 && u[i].s[k] && nt + 1 < max) {
                int v = 0, m;
                for (m = 0; m < k && v < 1000; m++) v = v * 10 + (u[i].s[m] - '0');
                t->kind = TTK_NUM;
                t->value = (uint8_t)(v > 255 ? 255 : v);
                t->text[k] = 0;
                nt++;
                t = &tok[nt];
                memset(t, 0, sizeof *t);
                t->person = -1;
                t->src0 = raw[u[i].raw].start; t->src1 = raw[u[i].raw].end;
                memcpy(t->text, u[i].s + k, strlen(u[i].s + k) + 1);
                if ((idx = prs_find(&h, t->text, &w)) >= 0) set_word(&h, t, &w);
                else t->flags |= TTF_UNK;
                nt++; i++;
                continue;
            }
        }
        if ((idx = find_lemma(&h, u[i].s, &w)) >= 0) {
            set_word(&h, t, &w);
            nt++; i++;
            continue;
        }
        if (ctx && ctx->person) {
            int id = ctx->person(u[i].s);
            if (id >= 0) {
                t->kind = TTK_PERSON;
                t->person = (int16_t)id;
                t->flags |= TTF_ABSENT;
                t->wflags |= TW_FUNC;
                nt++; i++;
                continue;
            }
        }
        if ((idx = find_safety_fuzzy(&h, u[i].s, &w)) >= 0 || (idx = find_lemma_weak(&h, u[i].s, &w)) >= 0 ||
            (idx = find_fuzzy(&h, u[i].s, &w)) >= 0) {
            set_word(&h, t, &w);
            t->flags |= TTF_FUZZY;
            nt++; i++;
            continue;
        }
        t->flags |= TTF_UNK;
        nt++; i++;
    }
    if (i < nu) cut = raw[u[i].raw].start;     /* more tokens than max: the rest is not in tok[] */
    for (i = 0; i < nt; i++) {                 /* the rules' literals match this hash */
        const char *p = tok[i].text;
        uint32_t x = 0x811C9DC5u;
        while (*p) { x ^= (uint8_t)*p++; x *= 0x01000193u; }
        tok[i].hash = x;
        named_person(ctx, &tok[i]);
    }
    if (cut_out) *cut_out = cut;
    return nt;
}

/* ---- topics: spreading activation (design 3.2) ---------------------------- */
static int candidate(const TalkTok *t)
{
    TalkConcept c;
    uint8_t parent, fl;
    if (!t->concept || (t->wflags & (TW_FUNC | TW_SEXUAL | TW_HATE | TW_DISTRESS | TW_REALWORLD)))
        return 0;
    if (t->kind == TTK_PERSON && !(t->flags & TTF_ABSENT)) return 0;
    if (!talk_db_concept(t->concept, &c) || c.age == TAGE_NEVER) return 0;
    if (!talk_db_cat(c.cat, &parent, &fl)) return 0;
    return (fl & TCAT_TALKABLE) && !(fl & (TCAT_SAFETY | TCAT_DESCRIBE));
}

static int concept_ok(int id)
{
    TalkConcept c;
    uint8_t parent, fl;
    if (!talk_db_concept(id, &c) || c.age == TAGE_NEVER || !talk_db_cat(c.cat, &parent, &fl)) return 0;
    return (fl & TCAT_TALKABLE) && !(fl & (TCAT_SAFETY | TCAT_DESCRIBE));
}

/* how much a mentioned concept counts (per mille): the everyday words
 * (today, time, thanks, friend) less than the things people talk about */
static int salience(const TalkTok *t)
{
    char name[24];
    if (t->root == TCR_TIME || t->root == TCR_FEELINGS) {
        if (talk_db_cat_name(t->cat, name, (int)sizeof name) > 0 &&
            (strcmp(name, "time.word") == 0 || strcmp(name, "feelings.social") == 0)) return 700;
        return 900;
    }
    if (t->root == TCR_PEOPLE) {
        if (talk_db_cat_name(t->cat, name, (int)sizeof name) > 0) {
            if (strcmp(name, "people.social") == 0) return 800;
            if (strcmp(name, "people.family") == 0) return 850;     /* "my mum makes great soup": the soup */
        }
    }
    return 1000;
}

#define ACT_MAX 96
typedef struct Act { uint16_t id; int16_t score, pos; uint8_t mentioned; } Act;

static Act *act_slot(Act *a, int *n, uint16_t id, int pos)
{
    int i;
    for (i = 0; i < *n; i++) if (a[i].id == id) return &a[i];
    if (*n >= ACT_MAX) return NULL;
    a[*n].id = id; a[*n].score = 0; a[*n].pos = (int16_t)pos; a[*n].mentioned = 0;
    return &a[(*n)++];
}

void talk_lex_topics(const TalkTok *tok, int n, const TalkParseCtx *ctx, uint16_t topic[2])
{
    Act a[ACT_MAX];
    int na = 0, i, j, best = -1, second = -1;
    topic[0] = topic[1] = TC_NONE;
    for (i = 0; i < n; i++) {
        Act *s;
        if (!candidate(&tok[i])) continue;
        s = act_slot(a, &na, tok[i].concept, i);
        if (s) { s->mentioned = 1; s->score = (int16_t)salience(&tok[i]); }
    }
    /* the recent topics: +300, halving per turn back */
    if (ctx) {
        int bonus = 300;
        for (i = 0; i < ctx->nstack && i < TALK_STACK_MAX; i++, bonus /= 2) {
            Act *s;
            if (!ctx->stack[i] || !concept_ok(ctx->stack[i])) continue;
            s = act_slot(a, &na, ctx->stack[i], 100 + i);
            if (s) s->score = (int16_t)(s->score + bonus);
        }
    }
    /* one hop (two on the Dreamcast) from every mentioned concept: the
     * first hop passes w/15 x 0.5, the second w/15 x 0.25 */
    {
        int hops = ctx && ctx->hops >= 2 ? 2 : 1, h, from = 0, to = na;
        for (h = 0; h < hops; h++) {
            int next = na;
            for (i = from; i < to; i++) {
                uint16_t ids[TALK_ASSOC_TOP];
                uint8_t w[TALK_ASSOC_TOP];
                int k;
                if (h == 0 && !a[i].mentioned) continue;
                k = talk_db_assoc(a[i].id, ids, w);
                for (j = 0; j < k; j++) {
                    Act *s;
                    int add = (h == 0 ? 500 : 250) * w[j] / 15;
                    if (!concept_ok(ids[j])) continue;
                    s = act_slot(a, &na, ids[j], 200 + i);
                    if (!s) break;
                    s->score = (int16_t)(s->score + add);
                    if (!s->mentioned && s->score > 900) s->score = 900;   /* never over a word said */
                }
            }
            from = next;
            to = na;
        }
    }
    for (i = 0; i < na; i++) {
        if (best < 0 || a[i].score > a[best].score ||
            (a[i].score == a[best].score && a[i].pos < a[best].pos)) best = i;
    }
    if (best < 0 || a[best].score < 300) return;
    topic[0] = a[best].id;
    for (i = 0; i < na; i++) {
        if (i == best) continue;
        if (second < 0 || a[i].score > a[second].score ||
            (a[i].score == a[second].score && a[i].pos < a[second].pos)) second = i;
    }
    if (second >= 0 && a[second].score * 10 >= a[best].score * 6 && a[second].mentioned) topic[1] = a[second].id;
}
