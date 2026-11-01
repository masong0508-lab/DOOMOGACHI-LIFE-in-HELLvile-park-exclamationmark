/* lang.c - the languages and the UI string lookup (owner: foundation; L1;
 * the contract is in include/doomo/lang.h).
 *
 * lang_tr() is on the draw path of every text call, so:
 *   - English (or a language with no table): a compare and a return;
 *   - a pointer into the current table's strings was translated already:
 *     returned as it is (ui_text_shadow / center and the native and PS1
 *     plat_text translate again what ui_text translated, and a
 *     translation must never be translated a second time);
 *   - a constant string (plat_text_is_const) is looked up once per
 *     language: a 64-entry cache (16 sets of 4) keyed by its address
 *     remembers the answer, misses (untranslated text) included;
 *   - otherwise one FNV-1a hash and a binary search inside one bucket of
 *     the table (about 4 probes for 3,000 strings).
 * The state below follows the option byte itself (compared on every
 * call), so whatever writes it (Options, a load, opt_defaults, a test)
 * switches the tables and empties the cache.
 */
#include "doomo/doomo.h"
#include "doomo/lang.h"

#define LANG_WAYS  4                  /* the cache: 16 sets of 4 entries */
#define LANG_SETS  16
#define LANG_CACHE (LANG_SETS * LANG_WAYS)

__attribute__((weak)) const uint32_t *plat_lang_table(int lang)
{
    return lang >= 0 && lang < LANG_COUNT ? doomo_lang_tables[lang] : NULL;
}
__attribute__((weak)) void plat_lang_changed(int lang) { (void)lang; }
__attribute__((weak)) int plat_text_is_const(const void *p) { (void)p; return 0; }

static int8_t s_pref = -1;            /* the session's chosen language, -1 = none yet */

/* The lookup state: one block, so the hot path loads one base address
 * (on the GBA every literal-pool load is a ROM access). */
typedef struct LangSlot { const char *in, *out; } LangSlot;
static struct {
    const uint32_t *tab;              /* the table (LNG2), NULL = English / none */
    uintptr_t       blob;             /* its strings: [blob, blob + blob_len) */
    uint32_t        blob_len;
    unsigned        key;              /* the option byte it is for + 1; 0 = stale */
    LangSlot        cache[LANG_CACHE];   /* constant text only */
} L;

int lang_get(void)
{
    unsigned v = doomo_town.opt[OPT_LANGUAGE];
    return v < LANG_COUNT ? (int)v : LANG_EN;
}

uint32_t lang_hash(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
    return h;
}

/* entry i of a u16 array packed in words (no 16-bit aliasing) */
static unsigned half(const uint32_t *w, unsigned i)
{
    return (w[i >> 1] >> ((i & 1u) << 4)) & 0xFFFFu;
}

static const char *find(const uint32_t *t, uint32_t h)
{
    unsigned bits = t[2] & 0xFFu, b = bits ? h >> (32 - bits) : 0;
    unsigned lo = half(t + LANG_TAB_HEAD, b), end = half(t + LANG_TAB_HEAD, b + 1), hi = end;
    const uint32_t *key = t + LANG_TAB_HEAD + (((1u << bits) + 2) >> 1);
    while (lo < hi) {
        unsigned mid = (lo + hi) >> 1;
        if (key[mid] < h) lo = mid + 1;
        else hi = mid;
    }
    if (lo >= end || key[lo] != h) return NULL;
    key += t[1];                                        /* the offsets */
    return (const char *)t + (t[2] & LANG_TAB_OFF16 ? half(key, lo) : key[lo]);
}

const char *lang_table_find(const uint32_t *t, uint32_t h)
{
    return t && t[0] == LANG_TAB_MAGIC ? find(t, h) : NULL;
}

/* the text of a context key (LCTX): after the second mark */
static const char *ctx_text(const char *s)
{
    const char *p = s + 1;
    while (*p && (uint8_t)*p != LANG_CTX_MARK) p++;
    return *p ? p + 1 : s;
}

static const char *lookup(const char *s)
{
    const char *r = find(L.tab, lang_hash(s));
    if (r) return r;
    if ((uint8_t)s[0] == LANG_CTX_MARK) {
        s = ctx_text(s);
        r = find(L.tab, lang_hash(s));
        if (r) return r;
    }
    return s;
}

void lang_flush(void)
{
    L.key = 0;                        /* the next lang_tr() asks for the table again */
}

__attribute__((noinline)) static void setup(void)
{
    int lang = lang_get();
    const uint32_t *t = lang != LANG_EN ? plat_lang_table(lang) : NULL;
    memset(&L, 0, sizeof(L));
    if (t && t[0] != LANG_TAB_MAGIC) t = NULL;
    L.key = doomo_town.opt[OPT_LANGUAGE] + 1u;
    L.tab = t;
    L.blob = t ? (uintptr_t)t + t[3] : 0;
    L.blob_len = t ? t[4] - t[3] : 0;
}

/* a cache miss: look it up, and remember it if the text is constant (the
 * newest first). Out of line: the hit path stays a small leaf. */
__attribute__((noinline)) static const char *miss(const char *s, LangSlot *e)
{
    const char *r = lookup(s);
    if (plat_text_is_const(s)) {
        e[3] = e[2];
        e[2] = e[1];
        e[1] = e[0];
        e[0].in = s;
        e[0].out = r;
    }
    return r;
}

const char *lang_tr(const char *s)
{
    LangSlot *e;
    if (!s || !s[0]) return s;
    if (doomo_town.opt[OPT_LANGUAGE] + 1u != L.key) setup();
    if (!L.tab) return (uint8_t)s[0] == LANG_CTX_MARK ? ctx_text(s) : s;
    if ((uintptr_t)s - L.blob < L.blob_len) return s;           /* a translation already */
    /* Only constant text is ever put in the cache, so an address found
     * there is constant: no need to ask the platform again. 4-way sets:
     * literals are packed 4-byte aligned, and a direct-mapped cache kept
     * evicting pairs of them every frame (the GBA Options screen: 742
     * misses in 3,536 lookups; with the sets, only the 20 first ones). */
    e = &L.cache[((((uintptr_t)s >> 2) ^ ((uintptr_t)s >> 5)) & (LANG_SETS - 1)) * LANG_WAYS];
    if (e[0].in == s) return e[0].out;
    if (e[1].in == s) return e[1].out;
    if (e[2].in == s) return e[2].out;
    if (e[3].in == s) return e[3].out;
    return miss(s, e);
}

int lang_find(const char *code)
{
    int i;
    if (!code) return -1;
    for (i = 0; i < LANG_COUNT; i++) {
        const char *a = code, *b = doomo_lang_info[i].code;
        for (; *a && *b; a++, b++) {
            int x = (uint8_t)*a, y = (uint8_t)*b;
            if (x >= 'A' && x <= 'Z') x += 32;
            if (y >= 'A' && y <= 'Z') y += 32;
            if (x != y) break;
        }
        if (!*a && !*b) return i;
    }
    return -1;
}

int lang_next(int lang, int dir)
{
    int i, l = lang >= 0 && lang < LANG_COUNT ? lang : LANG_EN;
    for (i = 0; i < LANG_COUNT; i++) {
        l = (l + (dir < 0 ? LANG_COUNT - 1 : 1)) % LANG_COUNT;
        if (doomo_lang_info[l].ready) return l;
    }
    return lang;
}

int lang_plural(int lang, uint32_t n)
{
    uint32_t m10, m100;
    switch (lang >= 0 && lang < LANG_COUNT ? doomo_lang_info[lang].plural : LANG_PLURAL_ONE) {
    case LANG_PLURAL_ZERO_ONE: return n > 1;
    case LANG_PLURAL_NONE:     return 0;
    case LANG_PLURAL_PL:
        if (n == 1) return 0;
        m10 = n % 10;
        m100 = n % 100;
        return m10 >= 2 && m10 <= 4 && (m100 < 12 || m100 > 14) ? 1 : 2;
    default:                   return n != 1;
    }
}

void lang_changed(void)
{
    s_pref = (int8_t)lang_get();
    plat_lang_changed(s_pref);
    lang_flush();                     /* the platform may have (re)loaded a table */
}

void lang_loaded(void)
{
    if (s_pref < 0) s_pref = (int8_t)lang_get();             /* nothing chosen yet: adopt the save's */
    else doomo_town.opt[OPT_LANGUAGE] = (uint8_t)s_pref;     /* keep this session's language */
    plat_lang_changed(s_pref);
    lang_flush();
}
