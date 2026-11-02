# Languages (L11)

How the game's text is translated, what the tools check, and how to add a
language. The contract lives in `core/include/doomo/lang.h`; this page is
the how-to. Eight languages are registered today: `en es pt-BR fr de it nl pl`.
Only English and Spanish are offered to players (`ready = 1`); the rest are
known to the engine and wait for their translation (L8, L10).

## Two kinds of text

| Kind | Where it lives | Compiled by | Offered languages |
|---|---|---|---|
| **UI text**: menus, hints, labels, anything the code draws from an English literal | `core/lang/<code>/ui_*.tsv` | `tools/langpack.py` -> `core/gen/lang_tab_<id>.c` | one `LNG2` table per language |
| **Story text**: dialogue and quests from the `.dsc` scripts | `core/lang/story/<pack>.tsv` | `core/tools/textpack.py` | Spanish only until L5 |

Code always passes the **English** literal. `lang_tr()` returns the current
language's translation, or the same pointer when there is none, so an
untranslated string simply stays English. Every text call of the core goes
through it once (`ui_text`, `ui_textf`, wrapped and centred text, list rows);
the platforms' `plat_text` draw bytes as given and never translate.

## The files

- `core/lang/languages.tsv`: the registry. Columns: `code`, `save`, `name`,
  `glyphs`, `page`, `plural`, `ready`. The `save` byte is what the island
  saves in `OPT_LANGUAGE`, so rows are **append-only**: never reorder or
  reuse a byte. Bytes 8 and 9 are reserved for ja and zh. Retire a language
  with `ready = 0`, never by deleting its row. At most 10 languages.
- `core/lang/<code>/ui_*.tsv`: `English<TAB>translation[<TAB># where]`. `#`
  starts a comment line. Escapes: `\n` newline, `\\` backslash, `\xHH` a font
  byte (button icons). An empty translation means "not done yet". Keep
  `{codes}`, the ` / ` line breaks and the `//` page breaks of the English.
- `core/lang/<code>/ui_main.tsv` is special: its strings of at most 48 bytes
  also make the PS1's resident table (`lang_res_<id>.c`), because the PS1
  links no full table (RAM). L6 will read tables from the disc instead.
- `core/gen/lang_ids.h`, `lang_registry.c`, `lang_tab_*.c`, `lang_res_*.c`
  are generated and committed. Do not edit them by hand.

## Rules the generator enforces

`langpack.py` fails on: two different English keys with the same hash, one
English key with two different translations, a character the font cannot
draw, a translation over 250 bytes, and a registry that breaks its rules
(save bytes dense and in order, more than 10 languages, glyph mode `cjk2`).

**Letters.** Glyph page 0 is the only page today: `á é í ó ú ñ ü` plus the
icons. Other accents (`ç ã ê ö ł ...`) are drawn without their accent until
the glyph pages (L4) land, and a ready language's own name must be
drawable as it is.

**Context.** The same English can need two translations ("Off" as a switch
and as a direction). Write `LCTX("dir", "Off")` in the C source; the TSV key
is `\x1Fdir\x1FOff`. The lookup uses the context's translation if there is
one, else the plain text's, and English never shows the prefix.

**Plurals.** `lang_plural(lang, n)` returns the form index from the
language's rule: `one` (en es de it nl), `zero_one` (fr pt-BR), `pl` (3
forms), `none` (ja zh, later).

## Commands

```sh
python3 tools/langpack.py                 # regenerate core/gen/lang_*
python3 tools/langpack.py --check         # exit 1 if a generated file is stale or left over
python3 tools/langextract.py --lang es out.tsv core/src/foo.c
                                          # worksheet of the English literals of foo.c
                                          # that the language does not cover yet
python3 tools/langstale.py --lang es      # report-only: rows whose English no longer
                                          # matches any C literal (a reworded literal
                                          # silently drops back to English)
python3 core/tools/textpack.py --check-es # story TSVs against the packs
python3 core/tools/textpack.py --sync-es  # re-sync the story TSVs (adds TODO rows)
make -C core test                         # includes check-gen and langpack --check
make -C gba test-lang                     # the Options screen in Spanish on the real ROM
```

## Add a language

1. Append a row to `core/lang/languages.tsv` with the next free save byte
   and `ready = 0`.
2. Create `core/lang/<code>/` and add `ui_main.tsv` (copy the English keys of
   `core/lang/es/ui_main.tsv`, leave the translations empty or fill them).
   Run `langextract.py --lang <code>` for the strings the other files add.
3. `python3 tools/langpack.py`, then commit the regenerated `core/gen` files.
4. Translate. Run `langstale.py --lang <code>` after any rewording of the
   English; run `make -C core test`.
5. When it is done and its letters are drawable, set `ready = 1` and run
   `langpack.py` again: the language now shows on the Options row and the
   title screen.

## What is not here yet

Story text for more than one extra language (L5), PS1 tables on disc (L6),
grammar-aware generators (L7), the Spanish and other translation batches
(L8, L10), accented glyph pages (L4), and Chinese and Japanese (new font
path, `lang.h` "LATER: CJK").
