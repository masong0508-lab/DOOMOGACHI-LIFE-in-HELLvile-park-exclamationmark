# Script locks: what a rewrite must not break

**Package P0 (1 November 2026).** The companion of
[`SCRIPT_STYLE.md`](SCRIPT_STYLE.md). It lists everything in the game's
text that saves, C code, tests or other packs depend on, and the two lock
files that check most of it mechanically
(`core/data/story/save.lock`, `core/data/story/flow.lock`, written and
checked by `core/tools/story_opslock.py`; `make -C core story-check` runs
the check, and `make -C core test` runs `story-check`).

Every count below was measured on the base of the rewrite
(`claude/nice-allen-gr6czm` @ 959acc8).

## 1. The save lock (hard): never regenerate it to make a rewrite pass

`core/data/story/save.lock` holds what a save file or the C code keeps by
number. Old saves would silently point at the wrong thing if any of it
moved. A rewrite package must leave it untouched, and `--check` must say
`SAVE LOCK: ok`.

| Record | Count | Why it is locked | How to stay inside it |
|---|---|---|---|
| `pack NAME = id=N` | 59 | Script ids are `pack << 9 \| index`; the pack id is the pack's place in name order (`textpack.py` `build()`) | A new pack needs a name that sorts **after** every existing pack, or its id shifts every later pack's scripts |
| `later SCRIPT = id=N` | 109 | `STRY.later[4]` keeps up to four pending "Later" callbacks **by script id** (`story.h` `DoomoStryLater`, `scriptvm.c` `op_later`) | Never delete, rename or move a `later` target. Add new scripts at the **end** of a pack (an earlier insert shifts every later index) |
| `mission S1xx = live\|retired` | 109 | `STRY.side2` keeps 2 bits per mission row S100-S217, by row number | Never delete a mission row or turn a live one into `kind=none`; titles, hints and trophies are W5's to reword |
| `possessor row=N = host=H` | 2 | The `POSSESS` op carries the row as the true-name id; `exorcism.c`'s default rows mirror it | Keep `row=` and `host=` |
| `c-ref NAME = defined` | 587 | `SCR_*`, `BSCR_*`, `TXT_*` and `PACK_*` names used by `core/src`, `core/native`, `core/tests`, `gba/src` or `ps1/src` | Keep the script, beat script, `string NAME` or pack name. The text and the ops inside are free |

Not in the lock file but just as hard (checked by tests or by reading):

* **`STRY` itself** (`story.h`, 96 B, version 1, no spare byte). A new
  saved field means `STRY` v2 and a migration; that is engine work, not a
  rewrite.
* **The flag ids** (`SF_*`, `story.h`). `.dsc` files use them by name; a
  rewrite may set or test different flags only as a deliberate restructure
  (flow lock), and may never invent one.
* **The Say It pool** (`ep46`, Ep 4.6). `STRY.sayit_said` keeps 96 bits,
  one per pool line and cast line said (`story_summit.c`, `SAYIT_POOL`).
  Reword freely; keep the number and order of entries.
* **The legacy beat ids** (`BEAT_C*`, 28 of them). Old saves alias onto
  episodes through them (`story_beat_episode`); they are never reordered.

If a change truly has to move one of these (a STRY v2, a new pack in the
middle), it is engine work: bump the save version, migrate, run
`python3 core/tools/story_opslock.py --write-save`, and explain it in the
commit.

## 2. The flow lock (soft): restructure on purpose, with a reason

`core/data/story/flow.lock` holds the shape of every scene with the words
taken out: each script's ops except `say` lines, with text ids masked in
choices, stashes and captions (the number of options stays), jump targets
as positions in that stream (labels may be renamed), the beat scripts
with their captions masked, the `@mission`, `@armed` and `@possessor`
attributes with their text masked, every pack's named-string NAMEs, and
every pack's kind, chapter and minutes.

* **A pure text rewrite never changes it.** Rewording, adding or cutting
  `say` lines, changing who says them or with which expression: all pass.
* **A restructure changes it, and that is allowed** (DECISIONS: restructure
  scenes where it makes jokes land). Run
  `python3 core/tools/story_opslock.py --write` in the same commit and add
  one line per restructured scene to the commit message:

  ```
  Flow lock regenerated:
    e11_mayor: cut the second [R21] and the sfx; the hard cut lands on "Recently!"
    s117_offer: the choice gets a third option ("Ask the carrot")
  ```
* `--check` prints the first op that differs in each changed script, for
  example `FLOW LOCK: script e22_hattie: first difference at op 0: lock
  'WAIT 8 0', now 'WAIT 13 0'`.
* Tests that measure structure still apply: `silence_budget` (20-25% of a
  chapter muted), `story_style_rations` (`{d}`, "Ten years I", `[NO
  BUTTON]`), `story_length_budget`, the side-table rules. Retune one only
  with the reason in the commit, never just to make a rewrite pass.
* **A merge or rebase conflict in `flow.lock`** (two packages restructured
  in parallel): never pick a side and never re-run `--write` blind, which
  would silently accept the other branch's restructures without review.
  Resolve the `.dsc` files first. Then take the other branch's
  `flow.lock`, run `--check`, and confirm that every script it names is
  one of **your** restructures (each listed in your commit message) and
  that the other branch's commits list theirs. Only then run `--write`.
  A script you did not restructure that `--check` still names is a
  mis-merged `.dsc`: fix the merge, not the lock. `save.lock` never
  conflicts in a rewrite; if it does, stop (§1).

## 3. Refrains, callbacks and true names

These run across packs: a setup in one pack, a payoff in another. Reword
one end and the other end breaks silently. **Keep the wording** of the
locked part, except where the new canon requires a change (marked
**canon**). Counts are lines in the English packs today.

| Refrain | Lines | Packs | Rule |
|---|---|---|---|
| "We don't talk about the other side." | 14 | ch1, ep11 (2), ep14, ep34, ep36, ep42, ep46, ep47, sa_nana, sa_solo2, sa_tower (2), sb_kids | Keep the sentence; variations change one word |
| "Hee. Hee." (the Mayor not looking) | 20 | ep11 (3), sa_mayor (6) and 11 more | Keep it exactly; it is a library take in the show |
| "Don't make it weird" (Rivet) | 13 | sa_rivet (6), ep26 (2), ep15, ep36, int3, s139, sa_solo1 | Keep; at most 3 more uses in the rewrite |
| Mr. Huggins the Nth (the numbering) | 19 | sa_fuzz (9), ep33 (3), ep24 (2), sb_solo1 (2), ch1, ep43, sa_solo1 | The numbers are a running count: keep each number where it is |
| Grape (the smell of the breath) | 28 | ch2 (3), ep21-ep22 (9), s139 (3), sb_vex (2) and 9 more | Keep the word; C2 says someone reacts to it before Ep 2.2 |
| "beep." / "bep?" (the pet) | 24 | 21 packs | The pet's only words. Never anything else |
| "...Mostly." (Nana's slip) | 7 | ep12, ep33, int3, q49a (2), sb_bloom (2) | Keep |
| "Open late" (Grumbleguts' sign-off) | 3 | ep27, sb_cloud (2) | Keep; it is his catchphrase |
| "I don't know that one." (Steamy's ducks) | 1 | ep14 | Keep; Ep 3.1's "...either" pays it off |
| "Ten years I ..." | 4 | ep41, s139, s179, s187 | Exactly these four (lint) |
| "THE POWER OF CUDDLES COMPELS YOU!" | 1 | ep25 | Keep |
| "Nothing is unhuggable." | 1 | ep24 | Keep |
| "My eyes are doing a rain." (Pip) | 2 | ep13, sb_tock | Keep |
| Fluffy / "I had a cloud once." / "Hi, Fluffy." | 7 | ch1, ep15, ep46 (2), sb_solo1, sb_vex, int3 | The Mayor's lost cloud: the game's own story. Keep |
| "That was a PERSON." (the Bottle) | 1 | ep25 | Keep |
| "I never asked where it went." (the midpoint) | 3 | ep34, int3, sa_mayor | Keep |
| Mingle (the old festival) | 11 | d1 (2), sb_solo2 (2) and 7 more | Keep the name |
| "Clack!" (Bonejangles) | 6 | ep14 (2), ep15, ep22, q49a, sa_fuzz | Keep; W15 adds the animatronic layer around it |
| "the Factory", "the stone" | 4, 3 | ep33, sa_pip (2), sb_solo1; ch1, int3, sa_mayor | Keep the nouns; C2 applies to their first use |
| "Needs death" (Clinker's rating) | 1 | sb_tock | **canon**: becomes "Needs salt" (C1 owns the S164 scoring code) |
| "Needs salt" | 1 | sb_gate | Keep; it is now the ratings catchphrase |
| The Mayor as a cherub, "wings buzzing" | — | ch1/common.dsc ("a pair of wings stops flapping"), any line about his halo or cherub look | **canon**: a Mad-Hatter turkey; wings flap |
| Sunny (Grampy's name for the player) | 3 | ep14 (2), ep34 | Keep; Sunny lives in the game |

**True names** (exorcism answers; the player picks one, so the spelling is
the mechanic): TALLYWORN, WHEEZLE, HAZE, SOOTKIN, FLUFFY, GIGGLESWORTH,
VAPULA, DIRGELET, MOPSWORTH, CLINKER, PLODDY, SHACKLES, DRUDGE, CINDERS,
NIMBUS (the `@possessor` blocks in `ch2/common.dsc` and the default rows in
`core/src/exorcism.c`). Never respell one; the decoys may be reworded only
together with `exorcism.c`.

## 4. Strings the C code checks

| String | Where it must stay | Checked by |
|---|---|---|
| "deceased" | `story_grumble.c` `obit_line` (Grumbleguts' dating-profile obituary) | `test_side_b.c` |
| "full" | `fp_common.c` the ghost rite's blocker line ("He went out full. ...") | `test_side_b.c` |
| The S164 poem's death words | `story_mg_b.c` `poem_death[]` marks which word of each slot is a death; the verdicts ("Six. Needs death.", "Seven. The death is in the right place.", "Four. Too much death.") are what `test_side_b.c` scores | `test_side_b.c` (`play_mg(SMG_B_POEM)` returns 1, 2, 3); C1 moves the verdict text to "Needs salt" together with the test comments |
| "EXORCISM UNFAILED x1" | `sa_sprk` (Chain I, Sprinklepop's soul) | `test_side_a.c`, `test_exorcism_authored.c` (comments name it; the state is checked) |
| "POPULATION: 1000" / "999. I can live with 999." | `sa_solo1` (S143) | `side_sample_a.txt` pins |
| `[NO GAG]` on S206 and S211, no `[PUNCH]`, `{d}`, `{r}`, `SHK_` or `[GURGLE]` in them | `sb_*` | `test_side_b.c` `no_comic_beat` |
| `[NO BUTTON]` / `[NO GAG]` never twice in a row in a chain; Grumbleguts' LOVE lines never in a child's conversation; exactly 4 ticker Laters | side packs | `test_side_table.c` |
| The Herald ticker | `story_grumble.c`, at most 63 characters | C |
| `OBJ_C1_MAYOR`, `OBJ_C1_KEY` (`ch1`), `VEX_PA1` (`ep27`): the decoder's known lines, compared whole | `core/tests/test_textpack.c` (`textpack_decodes_known_lines`, `textpack_stash_keeps_lines_for_a_pushed_scene`); `OBJ_C1_MAYOR` must start with "Say hi " (the truncation check) | `test_textpack.c`; W1 updated the two `ch1` lines |

## 5. Runner pins (74)

`skip-to "TEXT"` presses A until the dialogue contains TEXT, and
`expect-text "TEXT"` fails unless it does. Rewording a pinned line breaks
the script. Update the pin in the same commit (SCRIPT_STYLE M5): pick a
substring of the new line that is unique in that conversation. The table
says which pack holds each pinned line today.

#### `core/tests/scripts/side_sample_a.txt` (31)

| line | command | pinned text | where it lives |
|---|---|---|---|
| 21 | `expect-text` | twenty balloons | sa_sprk |
| 23 | `skip-to` | Pop the grey ones | sa_sprk |
| 30 | `skip-to` | only feelings | sa_sprk |
| 41 | `expect-text` | Mr. Huggins the 3rd | sa_fuzz |
| 42 | `skip-to` | to a carrot | sa_fuzz |
| 46 | `skip-to` | Compliment the carrot | sa_fuzz |
| 49 | `skip-to` | threads a needle | sa_fuzz |
| 53 | `skip-to` | head falls off | sa_fuzz |
| 56 | `skip-to` | They all do that | sa_fuzz |
| 59 | `skip-to` | all their names | sa_fuzz |
| 70 | `expect-text` | switched OFF | sa_solo1 |
| 71 | `skip-to` | do NOT bonk | sa_solo1 |
| 78 | `skip-to` | STAFF | sa_solo1 |
| 85 | `skip-to` | voice. | ep36, ep45, sa_pip, sa_solo1, sa_tower, sb_cloud |
| 88 | `skip-to` | plug GO | sa_solo1 |
| 92 | `skip-to` | very good machine | sa_solo1 |
| 105 | `expect-text` | little ghosts | ep42, sa_sprk |
| 106 | `skip-to` | Tuesday | ep26, sa_sprk, sb_solo3, sb_wisp |
| 109 | `skip-to` | Also Thursday | sa_sprk |
| 112 | `skip-to` | long week | sa_sprk |
| 115 | `skip-to` | against the law | sa_sprk |
| 126 | `expect-text` | hopscotch | ep41, sa_solo2 |
| 127 | `skip-to` | Ten lava squares | sa_solo2 |
| 134 | `skip-to` | hmph | sa_solo2 |
| 145 | `expect-text` | The town sign still says | sa_solo1 |
| 146 | `skip-to` | Stencilla, over the gate | sa_solo1 |
| 150 | `skip-to` | Don't let | ep25, sa_solo1 |
| 154 | `skip-to` | That's the point | sa_solo1 |
| 158 | `skip-to` | Ten years ago | ep34, ep43, sa_solo1 |
| 210 | `skip-to` | POPULATION: 1000 | sa_solo1 |
| 213 | `skip-to` | 999 | ep46, sa_solo1 |

#### `core/tests/scripts/side_chains.txt` (32)

| line | command | pinned text | where it lives |
|---|---|---|---|
| 20 | `expect-text` | Employee of the Month | s139, sb_vex |
| 22 | `skip-to` | Take the photo? | sb_vex |
| 35 | `expect-text` | Psst. | sb_vex |
| 37 | `skip-to` | Let Vex stay? | sb_vex |
| 51 | `expect-text` | reference letter | s139 |
| 53 | `skip-to` | Air out Cloud Nine? | s139 |
| 68 | `expect-text` | vape rig | sb_vex |
| 70 | `skip-to` | Build a bubble machine? | sb_vex |
| 83 | `expect-text` | housekeeper | sb_wisp |
| 85 | `skip-to` | Look into it? | sb_wisp |
| 97 | `expect-text` | red sky | sb_wisp |
| 99 | `skip-to` | Make Wisp a window? | sb_wisp |
| 112 | `expect-text` | measure me | sb_wisp |
| 114 | `skip-to` | Measure Wisp? | sb_wisp |
| 123 | `expect-text` | housewarming | ch1, ep44, sb_wisp |
| 125 | `skip-to` | Have a housewarming? | sb_wisp |
| 141 | `expect-text` | The candle in | sb_wisp |
| 143 | `skip-to` | Help with the candle? | sb_wisp |
| 156 | `expect-text` | hot stuff | sb_cloud |
| 158 | `skip-to` | Fetch the book? | sb_cloud |
| 170 | `expect-text` | dating ticker | sb_cloud |
| 172 | `skip-to` | Take his photo? | sb_cloud |
| 181 | `expect-text` | Taking bets | sb_cloud |
| 183 | `skip-to` | Go to the group? | sb_cloud |
| 197 | `expect-text` | ordered the menu | sb_gate |
| 199 | `skip-to` | Help in the kitchen? | sb_gate |
| 214 | `expect-text` | He wants to cook | sb_gate |
| 216 | `skip-to` | Run the duet? | sb_gate |
| 225 | `expect-text` | made dessert | sb_gate |
| 227 | `skip-to` | Find a dessert date? | sb_gate |
| 236 | `expect-text` | gate's fixed | sb_gate |
| 238 | `skip-to` | Decorate the gatehouse? | sb_gate |

#### `core/tests/scripts/side_sample_b.txt` (11)

| line | command | pinned text | where it lives |
|---|---|---|---|
| 193 | `expect-text` | My tally wall is full | sb_tock |
| 195 | `skip-to` | Get him chalk? | sb_tock |
| 202 | `expect-text` | Chalk for a poem | sb_tock |
| 230 | `skip-to` | Seven. | sb_tock |
| 233 | `skip-to` | Take the chalk | sb_tock |
| 247 | `expect-text` | The candle in | sb_wisp |
| 249 | `skip-to` | Help with the candle? | sb_wisp |
| 253 | `skip-to` | Apartment 13 | sb_wisp |
| 256 | `skip-to` | Wisp blows | sb_wisp |
| 259 | `skip-to` | SEE out | sb_wisp |
| 262 | `skip-to` | Very small | s187, sb_kids, sb_wisp |

## 6. Spanish while the English is rewritten

DECISIONS: Spanish (and every new language) is redone **after** the
rewrite. The rewrite packages do not touch `core/lang/story/*.tsv`. A
rewritten line's old row goes **stale** (its English no longer exists) and
the new line is **missing** a translation; in Spanish mode the game shows
the new English in the same box. `make -C core story-check` runs
`textpack.py --check-es --allow-missing --allow-stale` (the rewrite phase:
reported, not fatal); `make -C core story-check STORY_ES=strict` is the
gate the L8 Spanish batches finish on. Box overflow and a Spanish pack too
big to load fail in both modes. When the Spanish is redone,
`textpack.py --sync-es` rewrites each TSV in pack order, keeps every
translation, moves stale rows into a `# STALE` block for reuse and adds
the new lines as `TODO` rows.
