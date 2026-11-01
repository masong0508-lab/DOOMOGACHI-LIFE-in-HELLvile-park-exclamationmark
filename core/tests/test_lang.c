/* test_lang.c - the language registry and the UI string lookup (L1; lang.h).
 *
 * The tests switch languages by writing the option byte directly (lang.c
 * follows it), not through opt_set(), so the session's language choice
 * (lang_changed / lang_loaded) stays as the other tests expect; the one
 * test that goes through the Options screen puts English back. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/scenes.h"

static void set_lang(int lang) { doomo_town.opt[OPT_LANGUAGE] = (uint8_t)lang; }

/* lang_tr() of a copy on the stack: never cached (not constant) */
static const char *tr_uncached(const char *s, char *buf, size_t n)
{
    const char *r;
    snprintf(buf, n, "%s", s);
    r = lang_tr(buf);
    return r == buf ? s : r;
}

/* ---- a hand-made table for the paths no shipped TSV uses yet ----------- */
static const uint32_t *s_fake;        /* installed for s_fake_lang */
static int s_fake_lang = -1;

/* Strong over lang.c's weak default for this test binary: the shipped
 * tables, except a test's hand-made one. */
const uint32_t *plat_lang_table(int lang)
{
    if (lang == s_fake_lang && s_fake) return s_fake;
    return lang >= 0 && lang < LANG_COUNT ? doomo_lang_tables[lang] : NULL;
}

/* an LNG2 table with bucket bits 0 from n English -> other pairs */
static void build_table(uint32_t *t, size_t words, const char *const *en, const char *const *tr, int n)
{
    uint32_t keys[8];
    int order[8], i, j;
    size_t at;
    memset(t, 0, words * 4);
    for (i = 0; i < n; i++) { keys[i] = lang_hash(en[i]); order[i] = i; }
    for (i = 1; i < n; i++)                      /* sort by key */
        for (j = i; j > 0 && keys[order[j - 1]] > keys[order[j]]; j--) {
            int x = order[j]; order[j] = order[j - 1]; order[j - 1] = x;
        }
    t[0] = LANG_TAB_MAGIC;
    t[1] = (uint32_t)n;
    t[2] = 0;                                    /* one bucket: [0, n) */
    t[LANG_TAB_HEAD] = (uint32_t)n << 16;
    at = 4 * (LANG_TAB_HEAD + 1 + 2 * (size_t)n);
    t[3] = (uint32_t)at;
    for (i = 0; i < n; i++) {
        t[LANG_TAB_HEAD + 1 + i] = keys[order[i]];
        t[LANG_TAB_HEAD + 1 + n + i] = (uint32_t)at;
        memcpy((char *)t + at, tr[order[i]], strlen(tr[order[i]]) + 1);
        at += strlen(tr[order[i]]) + 1;
    }
    t[4] = (uint32_t)((at + 3) & ~(size_t)3);
}

static uint32_t half(const uint32_t *w, uint32_t i) { return (w[i >> 1] >> ((i & 1) * 16)) & 0xFFFF; }

/* entry i of a table: its key and its string */
static uint32_t tab_key(const uint32_t *t, uint32_t i)
{
    return t[LANG_TAB_HEAD + (((1u << (t[2] & 0xFF)) + 2) >> 1) + i];
}
static uint32_t tab_off(const uint32_t *t, uint32_t i)
{
    const uint32_t *off = t + LANG_TAB_HEAD + (((1u << (t[2] & 0xFF)) + 2) >> 1) + t[1];
    return t[2] & LANG_TAB_OFF16 ? half(off, i) : off[i];
}

/* every check a table must pass (the generator's promises, lang.h) */
static int table_ok(const uint32_t *t, char *why, size_t n)
{
    uint32_t i, cnt, bits, nb, at, size, owords;
    const uint32_t *key;
    if (!t || t[0] != LANG_TAB_MAGIC) { snprintf(why, n, "magic"); return 0; }
    cnt = t[1]; bits = t[2] & 0xFF; at = t[3]; size = t[4];
    if (bits > 8 || (t[2] & ~(0xFFu | LANG_TAB_OFF16))) { snprintf(why, n, "flags 0x%X", t[2]); return 0; }
    nb = 1u << bits;
    key = t + LANG_TAB_HEAD + ((nb + 2) >> 1);
    owords = t[2] & LANG_TAB_OFF16 ? (cnt + 1) / 2 : cnt;
    if (at != 4u * (uint32_t)(key + cnt + owords - t) || size < at || (size & 3)) { snprintf(why, n, "layout"); return 0; }
    if ((t[2] & LANG_TAB_OFF16) && size > 0xFFFF) { snprintf(why, n, "u16 offsets in %u bytes", size); return 0; }
    for (i = 0; i <= nb; i++) {
        uint32_t b = half(t + LANG_TAB_HEAD, i), k = 0;
        while (k < cnt && (bits ? key[k] >> (32 - bits) : 0) < i) k++;
        if (b != (i == nb ? cnt : k)) { snprintf(why, n, "bucket %u", i); return 0; }
    }
    for (i = 0; i < cnt; i++) {
        uint32_t o = tab_off(t, i);
        const char *s = (const char *)t + o;
        if (i && key[i - 1] >= key[i]) { snprintf(why, n, "keys not ascending at %u", i); return 0; }
        if (o < at || o >= size || !memchr(s, 0, size - o)) { snprintf(why, n, "string %u", i); return 0; }
        if (lang_table_find(t, key[i]) != s) { snprintf(why, n, "find %u", i); return 0; }
    }
    return 1;
}

TEST(lang_registry)
{
    int i, j, ready = 0;
    /* save bytes: the first two are in every existing save */
    CHECK_EQ(LANG_EN, 0);
    CHECK_EQ(LANG_ES, 1);
    CHECK(LANG_COUNT >= 8 && LANG_COUNT <= LANG_SLOTS);
    CHECK_EQ(LANG_SLOTS, 10);
    CHECK_EQ(doomo_option_info[OPT_LANGUAGE].count, LANG_COUNT);
    CHECK(doomo_option_info[OPT_LANGUAGE].values == doomo_lang_names);
    CHECK(doomo_lang_tables[LANG_EN] == NULL);           /* English is the source */
    for (i = 0; i < LANG_COUNT; i++) {
        const DoomoLangInfo *li = &doomo_lang_info[i];
        CHECK_MSG(li->name && li->name[0] && li->code && li->code[0], "language %d", i);
        CHECK_STR(doomo_lang_names[i], li->name);
        CHECK_EQ(lang_find(li->code), i);
        CHECK_EQ(li->glyphs, LANG_GLYPHS_LATIN);
        CHECK(li->plural <= LANG_PLURAL_NONE);
        ready += li->ready;
        for (j = 0; j < i; j++) CHECK_MSG(strcmp(doomo_lang_info[j].code, li->code), "%s twice", li->code);
    }
    CHECK(doomo_lang_info[LANG_EN].ready && doomo_lang_info[LANG_ES].ready);
    CHECK(ready >= 2);
    CHECK_STR(doomo_lang_info[LANG_ES].name, "Espa" STR_ES_n "ol");
    CHECK_EQ(lang_find("PT-br"), LANG_PT_BR);
    CHECK_EQ(lang_find("pl"), LANG_PL);
    CHECK_EQ(lang_find("xx"), -1);
    CHECK_EQ(lang_find("e"), -1);
    CHECK_EQ(lang_find(""), -1);
}

TEST(lang_tables_valid)
{
    char why[64];
    int i, n = 0;
    for (i = 0; i < LANG_COUNT; i++) {
        const uint32_t *t = doomo_lang_tables[i], *r = doomo_lang_res_tables[i];
        uint32_t k;
        if (t) { CHECK_MSG(table_ok(t, why, sizeof(why)), "%s table: %s", doomo_lang_info[i].code, why); n++; }
        if (!r) continue;
        CHECK_MSG(table_ok(r, why, sizeof(why)), "%s resident table: %s", doomo_lang_info[i].code, why);
        CHECK(r[2] & LANG_TAB_OFF16);           /* small: u16 offsets (PS1 RAM) */
        CHECK(t);
        /* the PS1's resident strings are a subset of the full table */
        for (k = 0; k < r[1]; k++) {
            const char *full = lang_table_find(t, tab_key(r, k));
            CHECK_MSG(full && !strcmp(full, (const char *)r + tab_off(r, k)), "%s resident %u",
                      doomo_lang_info[i].code, k);
        }
    }
    CHECK(n >= 1);
    CHECK(doomo_lang_tables[LANG_ES] && doomo_lang_tables[LANG_ES][1] >= 3000);
    CHECK(lang_table_find(NULL, 1) == NULL);
}

TEST(lang_english_identity)
{
    static const char *const s[] = { "Story Mode", "Off", "Language", "Nobody translated this", "" };
    int i;
    test_reset();
    CHECK_EQ(lang_get(), LANG_EN);
    for (i = 0; i < DOOMO_COUNTOF(s); i++) CHECK(lang_tr(s[i]) == s[i]);
    CHECK(lang_tr(NULL) == NULL);
}

/* 20 strings from every Spanish TSV (main, catalogs, screens): icons,
 * accents, a long help line */
static const char *const es_en[] = {
    "Story Mode", "Free Play", "Off", "On", "OFF", "ON", "NEW BEST!", "Florist",
    "Restart level", "Textures", "RANK", "All Prizes", "Cupcake Tester",
    "A lamp by the bed", "Language", "Demo run - nothing recorded", "Mail Carrier",
    STR_ICON_SKULL " Monster Mode is ON.", "\x8B Select  \x8C Resume",
    "PS1: Punchy = heavy recoil, shake, snappy moves. Brutal adds gore, gibs and hit-stop."
};
static const char *const es_es[] = {
    "Modo historia", "Juego libre", "No", "S" STR_ES_i, "NO", "S" STR_ES_I,
    STR_ES_EXCL "NUEVO R" STR_ES_E "CORD!", "Florista", "Reiniciar nivel", "Texturas", "RANGO",
    "Todos los premios", "Catacupcakes", "Una l" STR_ES_a "mpara junto a la cama", "Idioma",
    "Prueba: no se guarda nada", "Cartero",
    STR_ICON_SKULL " " STR_ES_EXCL "Modo Monstruo ACTIVADO!", "\x8B Elegir  \x8C Volver",
    "PS1: Contundente = retroceso, temblor y movimientos " STR_ES_a "giles. Brutal a" STR_ES_n
    "ade gore y pausa de impacto."
};

TEST(lang_spanish_sentinels)
{
    char buf[256];
    int i;
    test_reset();
    set_lang(LANG_ES);
    CHECK_EQ(lang_get(), LANG_ES);
    for (i = 0; i < DOOMO_COUNTOF(es_en); i++) {
        CHECK_MSG(!strcmp(lang_tr(es_en[i]), es_es[i]), "\"%s\" -> \"%s\"", es_en[i], lang_tr(es_en[i]));
        CHECK_MSG(!strcmp(tr_uncached(es_en[i], buf, sizeof(buf)), es_es[i]), "uncached \"%s\"", es_en[i]);
    }
    CHECK(lang_tr("Nobody translated this") != NULL);
    CHECK_STR(lang_tr("Nobody translated this"), "Nobody translated this");
    set_lang(LANG_EN);
    CHECK(lang_tr(es_en[0]) == es_en[0]);
}

TEST(lang_never_twice)
{
    const char *r, *r2;
    test_reset();
    set_lang(LANG_ES);
    r = lang_tr("Off");
    CHECK_STR(r, "No");
    CHECK(lang_tr(r) == r);                 /* a translation is never looked up again */
    r2 = lang_tr("Story Mode");
    CHECK(lang_tr(lang_tr(r2)) == r2);
    /* "No" is itself an English key ("No" -> "No", another string of the
     * table): the translation of "Off" still comes back as it is */
    CHECK(lang_tr("No") != r);
    CHECK(lang_tr(r) == r);
    set_lang(LANG_EN);
}

TEST(lang_out_of_range)
{
    test_reset();
    set_lang(200);
    CHECK_EQ(lang_get(), LANG_EN);
    CHECK(lang_tr("Story Mode") != NULL);
    CHECK_STR(lang_tr("Story Mode"), "Story Mode");
    set_lang(LANG_COUNT);
    CHECK_EQ(lang_get(), LANG_EN);
    /* a language with no table yet: English text, its own id */
    set_lang(LANG_PL);
    CHECK_EQ(lang_get(), LANG_PL);
    CHECK_STR(lang_tr("Story Mode"), "Story Mode");
    CHECK_EQ(opt_get(OPT_LANGUAGE), LANG_PL);
    set_lang(LANG_EN);
}

TEST(lang_cache_matches_lookup)
{
    /* every Spanish sentinel and some untranslated text, twice (miss, then
     * hit), against the uncached lookup; 200 distinct literals overflow
     * the 64 slots, so entries are replaced too */
    static const char *const more[] = {
        "Gore", "Difficulty", "Text speed", "Sim speed", "Day length", "Autosave", "Camera",
        "Controls", "Invert look", "Look speed", "Aim assist", "Lock-on", "Crosshair",
        "Field of view", "Head bob", "Screen shake", "Damage numbers", "Hit markers",
        "HUD layout", "Shoulder", "Blood colour", "Confetti", "Demon density", "Possessions",
        "Town events", "Weather", "LCD filter", "Window skin", "Font style", "Subtitles",
        "Nobody translated this", "Nor this", "Pip", "Rivet", "Mayor Giggs"
    };
    char buf[256];
    int pass, i;
    test_reset();
    set_lang(LANG_ES);
#if defined(__linux__)
    CHECK(plat_text_is_const(es_en[0]));    /* lang_native.c: the program's read-only data */
    CHECK(!plat_text_is_const(buf));
    CHECK(!plat_text_is_const(&doomo_town));
#endif
    for (pass = 0; pass < 3; pass++) {
        for (i = 0; i < DOOMO_COUNTOF(es_en); i++)
            CHECK_STR(lang_tr(es_en[i]), tr_uncached(es_en[i], buf, sizeof(buf)));
        for (i = 0; i < DOOMO_COUNTOF(more); i++)
            CHECK_STR(lang_tr(more[i]), tr_uncached(more[i], buf, sizeof(buf)));
        for (i = 0; i < OPT_COUNT; i++)
            CHECK_STR(lang_tr(doomo_option_info[i].name), tr_uncached(doomo_option_info[i].name, buf, sizeof(buf)));
    }
    /* a language switch empties it: the same pointers answer in English */
    set_lang(LANG_EN);
    for (i = 0; i < DOOMO_COUNTOF(es_en); i++) CHECK(lang_tr(es_en[i]) == es_en[i]);
    set_lang(LANG_ES);
    CHECK_STR(lang_tr("Story Mode"), "Modo historia");
    set_lang(LANG_EN);
}

TEST(lang_context_keys)
{
    static uint32_t tab[64];
    static const char *const en[] = { LCTX("dir", "Off"), "Off", "Back", LCTX("menu", "Back") };
    static const char *const tr[] = { "Hors", "Eteint", "Retour", "Arriere" };
    static const char *const ctx_only = LCTX("x", "Nobody translated this");
    const char *r;
    test_reset();
    /* English never shows the prefix */
    CHECK_STR(lang_tr(LCTX("dir", "Off")), "Off");
    CHECK_STR(lang_tr(ctx_only), "Nobody translated this");
    CHECK_STR(lang_tr("\x1Fnever closed"), "\x1Fnever closed");
    /* a context with its own translation, one that falls back to the plain
     * text's, one with neither */
    build_table(tab, DOOMO_COUNTOF(tab), en, tr, 4);
    { char why[64]; CHECK_MSG(table_ok(tab, why, sizeof(why)), "%s", why); }
    s_fake = tab;
    s_fake_lang = LANG_FR;
    set_lang(LANG_FR);
    CHECK_STR(lang_tr(LCTX("dir", "Off")), "Hors");
    CHECK_STR(lang_tr("Off"), "Eteint");
    CHECK_STR(lang_tr(LCTX("other", "Off")), "Eteint");
    CHECK_STR(lang_tr(LCTX("menu", "Back")), "Arriere");
    CHECK_STR(lang_tr(LCTX("x", "Back")), "Retour");
    CHECK_STR(lang_tr(ctx_only), "Nobody translated this");
    r = lang_tr(LCTX("dir", "Off"));
    CHECK(lang_tr(r) == r);
    s_fake = NULL;                          /* a platform that swaps a table flushes */
    s_fake_lang = -1;
    lang_flush();
    CHECK_STR(lang_tr("Off"), "Off");       /* the real (missing) French table again */
    set_lang(LANG_EN);
    set_lang(LANG_FR);
    CHECK_STR(lang_tr("Off"), "Off");
    set_lang(LANG_EN);
}

TEST(lang_plural_rules)
{
    static const uint32_t pl_n[] = { 0, 1, 2, 4, 5, 11, 12, 14, 21, 22, 25, 102, 112, 1000 };
    static const int pl_f[] = { 2, 0, 1, 1, 2, 2, 2, 2, 2, 1, 2, 1, 2, 2 };
    int i;
    CHECK_EQ(lang_plural(LANG_EN, 0), 1);
    CHECK_EQ(lang_plural(LANG_EN, 1), 0);
    CHECK_EQ(lang_plural(LANG_EN, 2), 1);
    CHECK_EQ(lang_plural(LANG_ES, 1), 0);
    CHECK_EQ(lang_plural(LANG_FR, 0), 0);
    CHECK_EQ(lang_plural(LANG_FR, 1), 0);
    CHECK_EQ(lang_plural(LANG_FR, 2), 1);
    CHECK_EQ(lang_plural(LANG_PT_BR, 0), 0);
    for (i = 0; i < DOOMO_COUNTOF(pl_n); i++)
        CHECK_MSG(lang_plural(LANG_PL, pl_n[i]) == pl_f[i], "pl %u -> %d", pl_n[i], lang_plural(LANG_PL, pl_n[i]));
    CHECK_EQ(lang_plural(-1, 1), 0);
}

TEST(lang_offered_languages)
{
    int l, steps;
    /* lang_next visits exactly the ready languages and comes back */
    for (l = LANG_EN, steps = 0; steps < LANG_COUNT + 1; steps++) {
        l = lang_next(l, 1);
        CHECK(doomo_lang_info[l].ready);
        if (l == LANG_EN) break;
    }
    CHECK(l == LANG_EN);
    CHECK_EQ(lang_next(LANG_EN, 1), LANG_ES);
    CHECK_EQ(lang_next(LANG_ES, -1), LANG_EN);
    CHECK(doomo_lang_info[lang_next(LANG_PL, 1)].ready);
}

static void frames(int n)
{
    while (n-- > 0) { plat_begin_frame(); doomo_frame(0); plat_end_frame(); }
}
static void press(uint32_t b)
{
    plat_begin_frame(); doomo_frame(b); plat_end_frame();
    frames(1);
}

TEST(lang_options_row)
{
    int i, ready = 0;
    for (i = 0; i < LANG_COUNT; i++) ready += doomo_lang_info[i].ready;
    test_reset();
    scene_set(&SCENE_OPTIONS, OPTPAGE_GAME);
    frames(2);
    /* RIGHT on the first row (Language) walks the ready languages only */
    for (i = 0; i < ready; i++) {
        int before = lang_get();
        press(BTN_RIGHT);
        CHECK_EQ(lang_get(), lang_next(before, 1));
        CHECK(doomo_lang_info[lang_get()].ready);
    }
    CHECK_EQ(lang_get(), LANG_EN);
    press(BTN_LEFT);
    CHECK_EQ(lang_get(), lang_next(LANG_EN, -1));
    opt_set(OPT_LANGUAGE, LANG_EN);
    test_reset();
}
