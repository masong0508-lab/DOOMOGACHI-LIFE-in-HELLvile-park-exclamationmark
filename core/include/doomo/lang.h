/* lang.h - the languages and the UI string lookup (owner: foundation; L1).
 *
 * LANGUAGES. core/lang/languages.tsv is the registry: one row per language
 * (code, saved byte, name, glyph mode, glyph page, plural rule, ready).
 * tools/langpack.py turns it into the LANG_* enum (core/gen/lang_ids.h)
 * and doomo_lang_info[] (core/gen/lang_registry.c). Nothing in the code
 * lists the languages: add one by appending a row (and its strings).
 * Today: en es pt-BR fr de it nl pl (8; ja and zh later, see the end).
 * OPT_LANGUAGE (town.h) holds the language: its value is the LANG_* id,
 * which is the byte saved with the island, so ids are append-only.
 *
 * LOOKUP. Code always passes the ENGLISH literal. lang_tr() returns the
 * current language's translation when its table has one, and the very
 * same pointer when not (an untranslated string simply stays English).
 * The hooks sit in the UI text layer (ui.c: ui_text, ui_text_width,
 * ui_textf, ui_text_wrap, ui_text_big; font_text_width) and in the
 * platforms' plat_text, so no call site changes. lang_tr() of a string it
 * returned is that string again (never translated twice), so layers that
 * both translate cost one lookup.
 *
 * TABLES. core/lang/<code>/ui_*.tsv (UTF-8, English<TAB>translation) ->
 * tools/langpack.py -> core/gen/lang_tab_<id>.c: ONE table per language
 * (format LNG2 below), linked on the ROM platforms (GBA, native,
 * Dreamcast). The PS1 links none of them (RAM): it links
 * core/gen/lang_res_<id>.c instead, each language's short strings of
 * ui_main.tsv, and returns those from plat_lang_table() (ps1/src/
 * lang_ps1.c); L6 replaces that with tables read from the disc. Story
 * text is separate: core/lang/story/<pack>.tsv -> core/tools/textpack.py.
 *
 * SPEED. English is a compare and a return. Another language hashes the
 * string (FNV-1a 32) and binary-searches one bucket of its table (about
 * 4 probes for 3,000 strings); a string the platform reports as constant
 * (plat_text_is_const(): ROM, read-only data) is remembered in a 64-entry
 * cache keyed by its address, so text drawn every frame is looked up once.
 * The cache is flushed when the language changes and by lang_flush().
 *
 * The language is saved with the island, but it is a session choice: once
 * chosen, loading a save keeps it (lang_loaded).
 *
 * LATER: CJK (Japanese, Chinese; not implemented). Save bytes 8 and 9 are
 * reserved for them (languages.tsv), LANG_SLOTS (10) leaves them room in
 * every per-language directory, and the glyph mode LANG_GLYPHS_CJK2 is
 * the planned path: text stays NUL-terminated bytes (lookup, hashing,
 * tables and the TSV pipeline need no change), a lead byte in 0xE0..0xFF
 * plus one trail byte (never 0x00 or 0x1F) names one of 8,192 glyphs, and
 * the text layer (font_text_width, plat_text, wrapping) measures and
 * draws such a pair as one wider glyph from a glyph sheet the platform
 * streams (GBA ROM, PS1 VRAM cache). Until that exists langpack refuses
 * cjk2 rows.
 */
#ifndef DOOMO_LANG_H
#define DOOMO_LANG_H

#include "types.h"
#include "../../gen/lang_ids.h"     /* LANG_EN, LANG_ES, ..., LANG_COUNT, LANG_SLOTS */

/* How a language's text is encoded (DoomoLangInfo.glyphs). */
enum {
    LANG_GLYPHS_LATIN,   /* one byte per glyph: ASCII, icons 0x80..0x9F, letters 0xA0..0xFF of the page */
    LANG_GLYPHS_CJK2     /* reserved (see above), not implemented */
};
/* Plural rules (DoomoLangInfo.plural, lang_plural()). */
enum {
    LANG_PLURAL_ONE,       /* 2 forms: n == 1 | other           (en es de it nl) */
    LANG_PLURAL_ZERO_ONE,  /* 2 forms: n == 0 or 1 | other      (fr pt-BR)       */
    LANG_PLURAL_PL,        /* 3 forms: one | few | many         (pl)             */
    LANG_PLURAL_NONE       /* 1 form                            (ja zh, later)   */
};

typedef struct DoomoLangInfo {
    const char *name;     /* the language's name in itself, font bytes ("Espa\xA5ol") */
    const char *code;     /* BCP 47 tag: "en", "es", "pt-BR" */
    uint8_t     glyphs;   /* LANG_GLYPHS_* */
    uint8_t     page;     /* glyph page of 0xA0..0xFF (0: the Spanish letters, font.h) */
    uint8_t     plural;   /* LANG_PLURAL_* */
    uint8_t     ready;    /* 1: offered on the Options row and the title screen */
} DoomoLangInfo;

/* core/gen/lang_registry.c */
extern const DoomoLangInfo doomo_lang_info[LANG_COUNT];
extern const char *const doomo_lang_names[LANG_COUNT];     /* = doomo_lang_info[].name */
extern const uint32_t *const doomo_lang_tables[LANG_COUNT];     /* NULL: none linked */
extern const uint32_t *const doomo_lang_res_tables[LANG_COUNT]; /* PS1 resident tables */

/* The current language: the option byte, or LANG_EN if it is out of range. */
int lang_get(void);

/* English literal -> the current language's text. Never NULL for non-NULL
 * input; the result is read-only and lives as long as the language does. */
const char *lang_tr(const char *s);

/* Context: the same English can need two translations ("Off" as a switch
 * and as a direction). LCTX("dir", "Off") is a key of its own in the TSVs
 * (\x1Fdir\x1FOff); lang_tr() uses its translation if there is one, else
 * the translation of the plain text, and English never shows the prefix.
 * Only pass such strings through lang_tr() (the ui_* text calls). */
#define LANG_CTX_MARK 0x1F
#define LCTX(ctx, s) "\x1F" ctx "\x1F" s

/* The language id of a code ("es", "pt-BR"; any case), or -1. */
int lang_find(const char *code);
/* The next language offered to players (ready) after `lang` in direction
 * dir (+1 / -1), wrapping; `lang` itself if it is the only one. */
int lang_next(int lang, int dir);
/* Which plural form (0..forms-1) of language `lang` a count n takes. */
int lang_plural(int lang, uint32_t n);

/* Forget the table and the remembered lookups: the next lang_tr() asks
 * plat_lang_table() again (a platform that moves, loads or swaps a table
 * of the current language, e.g. per overlay group, calls this). */
void lang_flush(void);

/* opt_set(OPT_LANGUAGE, ..) calls this: remember the session's choice and
 * let the platform load its tables. */
void lang_changed(void);
/* save.c calls this after a save replaced the options: keep this session's
 * language (or adopt the save's if none was chosen yet). */
void lang_loaded(void);

/* ---- platform hooks (weak defaults in lang.c) ---------------------------- */
/* The UI table of language `lang` (LNG2), or NULL. Default:
 * doomo_lang_tables[lang]. */
const uint32_t *plat_lang_table(int lang);
/* The language changed: the platform may (re)load tables. Not called from
 * the draw phase. */
void plat_lang_changed(int lang);
/* 1 if the bytes at p can never change while the game runs (ROM, read-only
 * data), so lang_tr() may remember its answer by address. Default 0 (no
 * cache). Never 1 for RAM a scene overlay, a buffer or the stack reuses. */
int plat_text_is_const(const void *p);

/* ---- the table format LNG2 (tools/langpack.py), u32 little-endian words --
 *   [0] magic "LNG2"   [1] n strings
 *   [2] bucket bits b (0..8) | LANG_TAB_OFF16 if the offsets are u16
 *   [3] byte offset of the first string   [4] table size in bytes
 *   then u16 bucket[(1 << b) + 1], padded to a word: bucket[i] is the
 *        first key whose top b bits are >= i, bucket[1 << b] = n
 *   then u32 key[n]: FNV-1a 32 of the English bytes, ascending
 *   then off[n]: byte offset of key[i]'s string from the table start, u32,
 *        or u16 (padded to a word) in a table under 64 KB (the PS1's)
 *   then the NUL-terminated strings (font bytes), padded to a word.
 * u16 values are read as halves of words (low half first), never through
 * a 16-bit pointer.
 */
#define LANG_TAB_MAGIC 0x32474E4Cu   /* "LNG2" */
#define LANG_TAB_HEAD  5             /* header words */
#define LANG_TAB_OFF16 0x100u        /* [2]: the string offsets are u16 */
uint32_t lang_hash(const char *s);
/* Look h up in table t (LNG2): the string, or NULL. No cache. */
const char *lang_table_find(const uint32_t *t, uint32_t h);

#endif
