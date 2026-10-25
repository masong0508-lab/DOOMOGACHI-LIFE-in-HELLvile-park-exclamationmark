# The story engine

Package S2 of [`docs/design/story/SPEC.md`](design/story/SPEC.md) (with its
binding §12) turned Story Mode from C tables into data: one resident
bytecode VM runs every conversation, episode, side mission and "Later"
callback; the words live in compressed text packs; the story's own save
chunk `STRY` keeps what the old flags could not. This page is the
reference for writers (the `.dsc` language) and for engine work (the VM,
the packs, the save, the verbs, the minigames, the PS1 window).

The timing half (beats, tweens, camera effects, dialogue codes) is
[`docs/TIMING.md`](TIMING.md) (package S1).

## The pieces

| File | What | PS1 |
|---|---|---|
| `core/src/scriptvm.c` | the VM (`story_vm.h`), the line formatter, `story_rng`, `story_fingerprint` | resident |
| `core/src/txt.c` | the pack reader and the canonical-Huffman decoder (`txt.h`), the caption stash | resident |
| `core/src/stry_save.c` | the `STRY` save chunk (96 B), episode bits, side rows, migration of old saves | resident |
| `core/tools/textpack.py` | the `.dsc` compiler and all of its lints | host |
| `core/gen/text_*.c`, `txt_dict.c`, `side_table.c`, `possess_table.c`, `story_ids.h` | generated from `core/data/story/**/*.dsc` (`make -C core gen`) | `text_*.c` are not linked on the PS1 (`TEXT.BIN` instead) |
| `ps1/src/txtload.c` | the PS1 text window: `TEXT.BIN` sectors into resident slots | resident |
| `core/src/story.c` | the VM's host: `VM_EXT` / `VM_PUSHED` ops, conversations, callbacks, re-arming possessions | story group |
| `core/src/story_verbs.c` | the side-mission verbs | story group |
| `core/src/story_mg.c` | `SCENE_STORY_MG`, the side-minigame shell | `storymg` group |
| `core/src/story_timing.c` | `SCENE_STORY_TIMING`, the comic-timing press minigame | `storymg` group |
| `core/src/story_side.c` | the 49 old quests and the walker over the new S100-S217 rows | story group |
| `core/src/story_script.c` | the beat logic for Chapters 3-4 (the legacy port; Ch1-2 delegate to `story_ch12.c`) | story group |
| `core/src/story_ch12.c`, `story_ch12.h` | Chapters 1-2: the glue between the legacy beat spine and the `ch1/`, `ch2/` packs (cast, doors, talks, objectives, the `CB_C12` actions, the wrongness rows, idle lines) | story group |
| `core/src/cutscene_ch12.c` | the Ch1-2 beat stages: P0, the Bottle, F1 and the `CUTV_*` vignettes (fog, rite, gate) | cutscene group |
| `core/src/mml_mingle.c`, `mml_heehee_jingle.c` | `SONG_MINGLE`, `SONG_HEEHEE_JINGLE` | resident (`audio_songs`) |
| `core/src/dream.c` | `SCENE_DREAM`'s scripted mode (the story's own dreams, D1 / D2) | dream group |

Names follow SPEC R4: resident files never match an overlay group's regex
(`scriptvm.c`, `txt.c`, `stry_save.c`), overlay-only story code keeps the
`story_` prefix.

## The VM (`story_vm.h`, `scriptvm.c`)

A script is an array of 4-byte ops `{u8 op, u8 a, u16 b}` in a pack. Script,
text and beat ids are all `pack:7 | index:9` (R6). The VM state is ~84 B of
`.bss`: the script, pc, a 4-deep call stack, 16 vars (`v0..v15`), the open
choice's labels and the last host op.

`vm_tick()` runs ops until one waits, and returns:

| Result | Meaning | The host |
|---|---|---|
| `VM_IDLE` | no script | — |
| `VM_RUN` | running or waiting (a box, a WAIT, a beat) | call again next tick |
| `VM_EXT` | a host-kind op is in `vm_ext_op()` | handle it in scene code, call `vm_tick()` again (same tick) |
| `VM_PUSHED` | a `PUSH` / `HOME DEFEND` / `HOME VISIT` | push the scene; after the pop, `vm_resume(result)` (var 0) |

**Resident code never calls overlay code (SPEC C6).** The VM runs text,
flags, vars, jumps, calls, waits, audio, camfx, inventory, the house's
resident API (`house_api.h`), `TIME`, `LATER`, `RNG` and `STASH` itself.
Everything that needs scene code comes back as `VM_EXT`: `CAST`, `NPC`,
`CAM`, `TELEPORT`, `BEAT`, `INSERT`, `CUT`, `FREEZE_CAP`, `TINT`, `REWARD`,
`BEFRIEND`, `SET_BEAT`, `POSSESS`, `UNPOSSESS`, `NATIVE`. The overworld
(`story.c host_ext`) is the host today; a cutscene, the house or combat
host the same way.

**Dialogue.** `SAY` decodes the line, formats it and copies it into the
dialogue queue entry at once (nothing resident points into a pack), so a
script's lines queue in order. Every op that is not a line, a choice, a
flag, a var, a jump, a call, `LATER`, `RNG`, `GIVE`/`TAKE`, `QUEST` or
`STASH` waits until the box has closed: a sting or a push lands after the
reader dismissed the line before it.

**Text codes** in `SAY` / `CHOICE` lines: `@P` the player, `@E` the pet,
`@T` the conversation's subject, `@D` its demon, `@V` Vex's flavour of the
day, `@0..@9` a VM var, `@@`. The typewriter codes are S1's (`{.} {..}
{...} {d} {!} {big}…{/} {w} {r} {punch} {p n} {cut n}`, `/` a line break,
`//` a page break); `textpack.py` rejects any other `{code}`.

**Determinism (C18).** `story_rng(ctx)` is an xorshift of `doomo_town.seed ^
day ^ beat ^ ctx`; every caller passes its own documented ctx (`RNG` ops:
`0x524E0000 | pool`, the timing game's drift: `0x54494D00 | ...`). No wall
clock. `story_fingerprint()` hashes `STRY`, the story flags, chapter and
beat (the I9 parity checks; the runner's `story-fp`).

## The `.dsc` language (`core/data/story/<package>/*.dsc`)

```
@pack ep16 kind=episode chapter=1 minutes=30
@mission S135 giver=69 side=hill ch=2-3 verbs=find,timing chain=J step=2 requires=S134
        later=10m offer=s135_offer active=s135_again title="Three Nights" trophy="Nobody Knows Who"
@possessor name="TALLYWORN" host=steamy name_res=712 offer="A|B|C|D" clue="a|b|c|d" skin=EXS_STANDARD
@armed steamy row=0 from=C2_SCREAM to=C2_EXORCISE unless=SF_STEAMY_SAVED|SF_STEAMY_LOST
string NAME "a named line (TXT_NAME in story_ids.h)"

script s135_offer
  say fizz SURPRISE "It beeps at everything." [HEART]
  choice nar "Three nights?" { "Sure." -> yes ; "Later." -> no }
yes:
  quest S135 active
  push timing pattern=three skin=thump
  jlt v0 2 meh
  reward 50 furn=vapor_detector
  later 10m s135_later
no:
end
```

* **Pack kinds and caps** (bytes, packed): `episode` 12 KB, `common` 6 KB,
  `side` 8 KB, `feast` 10 KB, `legacy` 12 KB. At most 128 packs of 512
  strings / scripts each. A pack may span files (`@pack` again with the
  same kind).
* **Ops** (one per line; `WHO` is `nar`, `me`, `target`, a cast name or a
  resident id; `vN` is a var): `say WHO [EXPR] "line" [HEART|PUNCH|PILL|SHOUT|DEADPAN|GURGLE]`,
  `choice WHO "q" { "a" -> L ; "b" }`, `player_line`, `setf/clrf FLAG`,
  `jif/jifn FLAG L`, `jlt/jge vN VALUE L`, `jmp L`, `setv/addv vN N`,
  `beat BEAT_*`, `give/take ITEM [n]`, `reward H$ [ITEM [n]] [unlock=ID] [furn=KEY]`,
  `befriend WHO n`, `possess WHO ROW`, `unpossess WHO`,
  `push KIND [ARG] [captions=NAME,NAME]` (kinds: `combat exorcism exo_practice
  cutscene pet workshop creator credits home cooking concert dream timing
  minigame other`; every ARG must be listed in `core/data/story/push_args.txt`,
  C12; a side pack's `push combat` needs `captions=`),
  `push timing pattern=three|knock|prayer|rimshot skin=thump|laugh|bell|heart [drift]`,
  `stash SLOT "line"|NAME`, `verb escort|find|photo|deliver [WHO] [cell=x,y] [fragile] then=SCRIPT`,
  `native CB [arg]` (legacy glue only), `wait/hold/waita N`, `sfx SFX_*`,
  `music SONG_*`, `mute [n]`, `duck gain n`, `tapestop [n]`, `resume_on_beat`,
  `shake SHK_*`, `punch dx dy`, `flash PAL_* level frames`, `fade level`,
  `pal PAL_* level`, `tint TINT_* level`, `iris [heart] r`, `cut shot`,
  `insert id 3|4`, `freeze_cap style "caption"`, `cast WHO x y [arg]`,
  `npc WHO NACT_* [arg]`, `cam CAM_* [arg]`, `teleport MAPID_* x y`,
  `home HOME_* [arg]`, `quest S1xx offered|active|done`, `time day|night|advance n`,
  `rng vN pool`, `later Nm SCRIPT`, `call SCRIPT`, `ret`, `runbeat BEATSCRIPT`, `end`.
* **Bracket codes** (the closed list, C14; anything else fails the build):
  `[R2]..[R26]` (a wait on the ladder), `[HOLD n]`, `[MUTE n]`, `[DUCK g n]`,
  `[TAPESTOP]`, `[SCRATCH]` (tape-stop 5 + `SHK_BRAKE` + mute 14),
  `[SILENCE]` (mute until the next `music`), `[WAIT A]`, `[FLASH Wa,b]`,
  `[SHK_*]`, `[DAY n]`, `[NIGHT]`, `[NO BUTTON]`, `[NO GAG]` (lint markers),
  `[INSERT n]`, `[EXPR x]` and the expression names (`[SAD]`, `[SHOCK]`, ...:
  the next line's expression), `[SFX x]` and the SFX names (`[TINK]`, ...).
* **Beat scripts** (`beatscript NAME ... end`): `LABEL:` lines and
  `+FRAMES op args ; op args` (S1's `beat.h` events; `L_PUNCH` and
  `L_BUTTON` are label ids 30 and 31).
* **The compiler rejects** a page over 3 x 28 at 240x160 (legacy packs
  page automatically and are exempt), more than one `{d}` in a script, a
  jump to an undefined label, an unknown name, a pack over its cap, a
  mission without `later=`, "Ten years I" outside Ep 4.1 / S139 / S179 /
  S187, an option over 27 characters, an unlisted push arg.

`make -C core gen` runs `textpack.py`; `make -C core check-gen` regenerates
everything byte-identical. Fix a failing lint in the `.dsc`, never in
`core/gen/`.

## Packs (`txt.h`, `txt.c`)

Each pack: a 40-byte header (`'TX'`, version, id, counts, flags = legacy |
kind << 4), then the string offsets, the scripts (`{first_op, n_ops,
n_labels, pack}`), the ops, the beat spans, the beat events (8 B) and the
Huffman streams. The codec (SF §16.3): symbols 0..255 are bytes, 256 + k is
one of 256 dictionary words and phrases chosen over all text; one global
canonical Huffman code (lengths ≤ 16) in `core/gen/txt_dict.c`. Decoding
is per string into the caller's buffer (~15 cycles a bit).

`txt_plat_pack(p)` says where a live pack's bytes are: `core/gen/text_dir.c`
points into ROM on the GBA and native; on the PS1 `ps1/src/txtload.c`
points into its window and `txt_plat_load(p)` reads the pack from
`TEXT.BIN` (a build product, never committed: sector 0 = `{'TEXT', n, n x
{sector, bytes}}`, then every pack sector-aligned; CMake runs `textpack.py
--bin` and `iso.xml` puts it on the disc).

**The caption stash (C5).** `txt_stash(slot, id)` decodes a line into one
of 4 resident 64-byte slots; `stash` ops and `push ... captions=` fill it
before a push, so a scene in another overlay group (combat's HUD, the
scripted dream, the text-entry game) reads its lines without the story's
packs.

## `STRY` (`stry_save.c`, story.h `DoomoStry`)

Exactly 96 B, version 1, written after the town in the save stream
(`save_ext.h`, `doomo_save_ext_story2`): the house phase, 38 episode bits,
branches, the 118 side rows x 2 bits (0 none, 1 offered, 2 active, 3 done),
knocks, nests, clues, the Feast state, Grumbleguts' story-only counters,
the spawner cooldowns and four pending `LATER` callbacks `{script, due
game-minute}`. There is no spare byte: the next field needs `STRY` v2.

**Old saves (C19).** A save without `STRY` resets the chunk after the town
body is read; `reset()` tells a new town (quest 0, `SF_STARTED` clear) from
a migration and derives the episode bits and the house phase from
`doomo_town.story.quest`: the 28 legacy `BEAT_C*` ids alias onto episodes
(`story_beat_episode`) and are never reordered.

**Authored possessions across a reload.** exorcism.c's armed possessor
slots (S3) are not in the save. The story re-arms them: on every resume
(`SCENE_STORY` continue, a debug jump, a new game) `story_rearm_possessors()`
clears the slots and arms every host that is still possessed inside one of
its `@armed` windows (the beats in which that row is the story's
possession, unless a flag says it is over). Any other possession is the
sim's own pick: `exorcism_authored_row()` no longer guesses (the
`INTEGRATION_TODO` S3.M1 risk; tests `possessor_survives_a_reload_in_story_mode`,
`story_save_round_trip_mid_story`).

## Side missions S100-S217

`@mission` headers become `core/gen/side_table.c` (`StorySideRow`, 26 B,
indexed by mission number - 100; unwritten rows are kind 0). The walker
(`story_side.c`) offers a mission when its giver is fine and
`story_side2_gate()` says so: inside its chapter window, after `requires=`,
part k of a chain after part k - 1, at most one part of a chain per
chapter, and parts 3+ of a chain that crosses the Wall only from Ep 4.1.
Talking to the giver runs `offer=` (state 0 / 1) or `active=` (state 2);
the scripts set the state (`quest S1xx active|done`). Hillvile givers stand
in the invaded town in Ch3 (`story_side2_giver_map`). The 49 old quests
keep their table and their bits in the story flags.

**S4 additions.** `@mission` takes `at=X,Y` (where the giver stands on
their map: `StorySideRow.spot`, the row's padding byte, into
`doomo_side_spots`; in Ch3 the Hillvile givers shelter around the church
instead), `after=BEAT_*` (an episode that must be done first: the `tag`
byte) and `hint="..."` (the quest log's line, stored right after the
title: hint id = title id + 1). Every `@mission` header lives in the one
index pack `quests` (`core/data/story/quests49/index.dsc`; reopen it with
`@pack quests kind=side` above each header), with the 49 old quests'
titles and hints: the PS1 keeps one side pack resident, and a quest log
reading titles from many packs would re-read the disc every frame. The
missions' scripts live in their own side packs, loaded when a script
starts; `story_text_prefetch` loads only the index. The old quests'
offers and thank-yous are re-timed scripts (`q49a`, `q49b`): the offer ends
on the accept choice, the thank-you is followed by the table's reward.
`native CB_SIDE_HAVE ITEM_X` (and `CB_SIDE_STATE`, `_NIGHT`, `_STATUS`,
`_QUEST`, `_MONEY`, `_DAY`, story_int.h) put an answer the VM cannot get
by itself into var 0. The overworld keeps a running verb's resident (the
escort's follower, the photo's subject, the delivery's recipient) on the
map through cast refreshes, and Ch3-5 idle lines let a side mission's
business go first (`story_ch34.c idle()`). The verb key is SF sec.14's
(`fetch deliver find photo combat exorcism weld pet minigame fishing
cooking concert housing creator escort timing`; talk is implicit).

**Trophies (R8)** are not unlock ids: a done row with a `trophy=` name is
listed in the quest log's Trophies tab (`story_trophies()`).

## Verbs (`story_verbs.c`)

`verb KIND [WHO] [cell=x,y] [fragile] then=SCRIPT` starts one overworld verb
(one at a time); the VM stays free, so the plot can still talk, and SCRIPT
runs with `v1` = the result when it ends. The HUD objective shows the verb.

| Verb | The player | Result |
|---|---|---|
| `escort WHO cell=x,y` | walks WHO to the cell; WHO waits when you get 3 cells ahead and is lost after 5 s more than 9 cells behind | 1 arrived, 0 lost |
| `find cell=x,y` | walks onto the hidden cell; toasts say cold, getting warmer (9 cells), warmer (5), hot (2) | 1 |
| `photo WHO` | presses A facing WHO, 2-6 cells away, inside a ~53° frame | 2 centred, 1 in the frame (a miss lets you retry) |
| `deliver WHO [fragile]` | walks slower (200/256; fragile 160/256 and no running) and talks to WHO | 2 perfect, 1 with spills |

The rules are pure functions (`story_escort_judge`, `story_photo_judge`,
`story_find_heat`) with their frames and distances in one table.

## The story minigames

**`SCENE_STORY_TIMING`** (`push timing`; arg = `TPAT_* | (TSKIN_* |
TIMING_DRIFT) << 8`). A metronome clicks at 15 frames a beat; a ball hops
along the round's pattern and the last dot is the punchline: A within 2
frames is PERFECT, within 5 GOOD. Three rounds per pattern (the rule of
three 3-1, 3-1-2, 3-1-2-1; the knocks; Hattie's prayer; the rimshot). Skins
change the click, the punch and the crowd (thump, a laugh track, bell,
heartbeat). `drift` (S155) moves each beat by up to 2 frames, from
`story_rng`, so the player has to listen. Result: the rounds landed, 0..3.

**`SCENE_STORY_MG`** (`push minigame ID`, `story_mg.c`) is the frame around
S4's and S5's side minigames (`story_mg_a.c`, `story_mg_b.c`, read through
the weak registry pointers): a title card, the game's `enter/update/draw`
(update returns 1 + its result), a result card. On the PS1 the shell, the
timing game and the registry's games are the `storymg` overlay group (a
scene edge from the story, like the cutscenes). Built in:
`STORY_MG_TEXT_ENTRY` types the stashed answer (slot 1) to the stashed
prompt (slot 0), case and spaces forgiven (Sprinklepop's EXORCISM
UNFAILED).

**Scripted dreams** (`push dream 0x4000|n`, `DREAM_ARG_SCRIPTED`): you
dream, the Peek is forced (no tiptoeing away), the vignettes are the row's
(D1 "Teeth Like Muffins", D2 "The Ninth Cloud") and the captions are the
stashed lines (slots 0-2, the wake-up line in 3), arriving as floating
letters. No stats change and the dream journal is not written.

## Other S2 hooks

* **Go inside**: the cottage's door menu has "Go inside" once you have the
  key and have seen it (`SF_HOME_SEEN`): `SCENE_HOME` with
  `HOME_ARG_OWN(HOME_COTTAGE)`.
* **Unlocks 677-710** (`unlocks_story.c`, R7): the 12 new cutscenes
  (gallery, granted when the story pushes them), the 9 new songs (the
  jukebox, granted the first time a script plays them in Story Mode) and 13
  misc entries (3 title variants, 6 Breach variants, 2 arena modes, 2
  creator part sets) that S3 / S5 content grants.
* **The HUD objective** is a resident copy that the story group refreshes
  every tick (`story_objective_refresh`), so combat's HUD and the quest log
  read it without pinning the plot's code resident on the PS1.
* **Debug jumps** to the 38 new ids set up the legacy beat in progress at
  that point of the story order, with every episode before it done
  (`STORY_ARG_DEBUG_BEAT + id` works for them too).
* **Runner commands** (`core/native/runner_main.c`): `seed N` (the town seed
  and `plat_entropy()` from now), `expect-text "..."` (the dialogue line
  contains it), `expect-flag F [V]`, `story-fp [HEX]`. The GBA / PS1
  harnesses get the same commands from H2 / H3 (SPEC C2).

## Chapters 1-2 (S2 content)

P0, Ep 1.1-1.6, Interlude I, Ep 2.1-2.7, F1, D1 and the Bottle
(story_final.md §9-11) are VM content in `core/data/story/ch1/` and `ch2/`:
one episode pack each (`ep11` .. `ep27`, ≤ 2 KB), `p0`, `int1`, `f1`,
`d1`, and a chapter-common pack per chapter (`ch1`, `ch2`: the HUD
objectives, the cast's idle lines, the W1-W15 wrongness captions and
scripts, the `@possessor` / `@armed` rows of TALLYWORN and WHEEZLE). About
22 KB packed in all.

**The spine.** The 18 legacy beats `BEAT_C1_ARRIVE` .. `BEAT_C2_CELLAR`
stay the saved spine (old saves and `test_story.c` keep working): each
holds an episode or two, and `quest_step` is the stage inside the beat.
`story_set_beat` marks every episode that comes before the beat's episode
done (`STRY` episode bits), so the debug jumps, the gallery and the
`expect-flag` checks agree. `story_script.c` hands every Ch1-2 beat to
`story_ch12.c` at the top of each function (`ch12_beat(b)`), which answers
from tables: who stands where (`cast_rows`), whose talk runs which script
(`npc_rows`: a flavour line lets a side quest's business go first; a
door is its keeper's plot line), what the HUD says, where its arrow
points, what starts by itself on an idle overworld (`script_auto` from
`story_vm_pump`: P0 before the first day, the hatching, the thumping, the
interlude, the Ch2 mornings). Scripts change the state with VM ops and the
`native CB_C12 C12A_*` actions (`story_ch12.h`).

**The beat stages.** `story_push_cutscene(CUT_P0 | CUT_BOTTLE | CUT_F1 |
CUTV_FOG | CUTV_RITE_FAIL | CUTV_GATE_CRACKS)` (the `CUTV_*` args
0x120.. are in `push_args.txt`) opens `SCENE_CUTSCENE`, whose S2 hunk hands
them to `cutscene_ch12.c`: the beat script comes out of the episode's pack
(`txt_beat`), the captions are text ids, the host kinds SHOT, CAPTION,
POP (an object and its `TWT_*` table), INSERT (3-4 frames drawn instead of
the stage) and IRIS are handled there, and the stage draws from the shot,
the shot's age, the caption count, the pops and `beat_age(L_PUNCH)`. The
scenery is tables of prims in stage units (s px, s = 1 or 2) around an
origin, because on the PS1 the file sits in the cutscene overlay and that
group is the slot's size class. Grumbleguts is drawn by `grumbleguts_gfx`
as built (cartoon-modest; his lines are the director's notes').

**The PS1 text window** has a third resident slot for the chapter-common
pack (2 KB, `txtload.c`): objectives, idle lines and wrongness captions are
read while an episode runs and must not evict it. A common pack too big
for it falls back to the episode slot. An episode that `call`s into
another pack (D1, F1) evicts the episode for that span; the VM reloads it
on return (`bind`, one ~0.1 s read).

## The legacy port

The old beats of Chapters 3-4 still run as C glue in `story_script.c`
around the `legacy` pack (`legacy_ch34.dsc`, which S3 replaces); the 49 old
side quests' text was the `legside` pack until S4 re-timed it into
`quests49/` (the `quests`, `q49a` and `q49b` packs). `legacy_ch12.dsc` is
gone: Ch1-2 are the packs above.

## Tests

`test_story_vm.c` (every op, the stash, verbs, the minigames, the walker,
`STRY` and its migration, `story_vm_scripts_valid`), `test_textpack.c`
(the codec round trip, the box), `test_story_lint.c` (`beat_script_timing`,
`silence_budget`, `story_style_rations`, `story_length_budget`; the chapter
gates bite once a chapter has episode packs with declared minutes),
`test_story.c` (the playthrough, debug jumps to all 66 ids, `STRY` at every
episode boundary, Go inside, the reload), `test_exorcism_authored.c` (the
re-arm), `test_story_ch12.c` (every beat stage plays to its end and START
skips it, every Ch1-2 beat has its pack, objective and target, the
wrongness rows and idle lines decode, the two songs are real). Runner:
`story_engine.txt` (the timing game, the minigame shell, the scripted
dream, the Trophies tab, the story checks; both resolutions, no stubs),
`story_p0.txt` (the prologue, shot by shot), `story_ch1.txt` (P0 skipped,
the card, the arrival, Ep 1.1, the quest log, the fog), `story_int1.txt`
(the interlude to the Ch2 card), `story_ch2.txt` (Ep 2.1-2.2 and every Ch2
stage). GBA: `gba/harness/scripts/story_ch1.txt` (`make -C gba
test-story`, part of `test`) measures the stages' fps: P0 23-30, the fog
30, the gate 24 (gate: ≥ 15).

## Memory

Measured on clean builds against 4ba4e96 (the commit before S2's engine).

| | GBA | PS1 |
|---|---|---|
| S2 `.bss` (VM 88 B, `STRY` 96 B, the stash 256 B, the HUD objective 96 B, the verb, minigame and walker state) | ~1 KB of EWRAM: 8,160 -> 7,104 B free | resident |
| `scriptvm.c`, `txt.c`, `stry_save.c`, `txt_dict.c`, `txtload.c` | ROM | ~12 KB resident |
| the text window | ROM pointers | 20 KB resident (episode slot 12 KB, side slot 8 KB) + a 256 B directory |
| the legacy port's packs | ROM (+13 KB of ROM in all: 3,076,700 B) | `TEXT.BIN`, 22 KB on the disc |

**The PS1 resident ledger (SPEC C5).** The story's text and the plot code
it pinned left resident memory (resident text + rodata 1,140 -> 1,086 KB),
the VM, the decoder, the window and the stash came in (`.bss` 526 -> 548
KB): **108 KB below the stack top** (was 75 KB; gates: >= 72 KB, >= 64 KB
for `elf2exe`). The overlay slot stays 116 KB (combat). The side slot moves
into the overlay slot's tail once H3's `ovl_tail()` lands (C5: -8 KB).

**Groups.** The story group is 99 KB (47 KB before; C4's cap is 75 KB): it
now holds the plot code that the escaping strings used to pin resident. It
fits the slot and C5's tail rule (<= slot - 14 KB = 102 KB). The minigame
shells are their own `storymg` group (6 KB). Levers, in order: Ch1-2
content replaces `story_script.c`'s legacy glue (~12 KB) and moves
`story_talk.c`'s chatter into packs (~11 KB); `credits.c` as its own group
only frees 3 KB (most of it is shared with the overworld).

**Chapters 1-2 content (S2), measured against 9fd4327.** GBA: ROM +38 KB
(3,115,480 B), EWRAM 7,104 -> 6,712 B free (the HUD objective 76 B, the
stage state 164 B, the idle line 112 B; IWRAM unchanged, 6,320 B of
stack). PS1: 108 -> 103 KB below the stack top (the common slot 2 KB, the
two songs' MML, the stage and idle buffers); the cutscene group 98,912 ->
118,444 B (the stages: 20 KB; still under combat's 118,896 B, so the slot
stays 116 KB); the story group 101,748 -> 104,640 B (`story_ch12.c` 7 KB in,
`story_script.c` 3 KB out), about 200 B over C5's tail line (slot - 14 KB),
which only matters once H3's `ovl_tail()` lands.
