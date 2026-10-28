/* lang.h - the language setting and the UI string lookup (owner: foundation).
 *
 * OPT_LANGUAGE (town.h) picks the language: LANG_EN or LANG_ES. Text the
 * game draws is looked up through lang_tr(): the string the code passes is
 * always the ENGLISH literal; in Spanish mode the lookup swaps it for its
 * translation when core/lang/es.tsv has one, and returns the very same
 * pointer when not (so an untranslated string simply stays English). The
 * hooks sit in the UI text layer (ui.c: ui_text, ui_text_width, ui_textf,
 * ui_text_wrap, ui_text_big, font_text_width) and in each platform's
 * plat_text, so no call site changes.
 *
 * Two kinds of Spanish text:
 *   - UI strings   core/lang/es.tsv (UTF-8) -> tools/langpack.py ->
 *                  core/gen/lang_ui_es.c (a sorted table of 32-bit string
 *                  hashes -> Spanish, no English keys stored). The PS1
 *                  reads the same table from LANGUI.BIN into spare RAM
 *                  (ps1/src/txtload.c) instead of linking it.
 *   - story text   core/lang/story/<pack>.tsv -> core/tools/textpack.py:
 *                  same packs, same ids, strings swapped (text_es_*.c on
 *                  ROM platforms, the second half of TEXT.BIN on the PS1).
 * Spanish letters are the font bytes 0xA0..0xAF (font.h STR_ES_*).
 *
 * The language is saved with the island (the option byte), but it is a
 * session choice: once chosen, loading a save keeps it (lang_loaded).
 */
#ifndef DOOMO_LANG_H
#define DOOMO_LANG_H

#include "types.h"

enum { LANG_EN, LANG_ES, LANG_COUNT };

/* The current language (LANG_EN / LANG_ES). */
int lang_get(void);

/* English literal -> the current language's text. Never NULL for non-NULL
 * input; the result is read-only and lives as long as the language does. */
const char *lang_tr(const char *s);

/* opt_set(OPT_LANGUAGE, ..) calls this: remember the session's choice and
 * let the platform load its tables (PS1: LANGUI.BIN). */
void lang_changed(void);
/* save.c calls this after a save replaced the options: keep this session's
 * language (or adopt the save's if none was chosen yet). */
void lang_loaded(void);

/* Platform hooks (weak defaults in lang.c). */
/* The UI table (format: tools/langpack.py), or NULL. */
const uint32_t *plat_lang_ui(void);
/* The language changed: the platform may (re)load tables. Not called from
 * the draw phase. */
void plat_lang_changed(int lang);

/* The table format (tools/langpack.py): u32 magic, u32 n | (blob offset << 16),
 * u32 key[n] (FNV-1a 32 of the English bytes, ascending), u16 off[n], pad to 4,
 * then the NUL-terminated Spanish strings at blob offset + off[i]. */
#define LANG_UI_MAGIC 0x31474E4Cu   /* "LNG1" */
uint32_t lang_hash(const char *s);

#endif
