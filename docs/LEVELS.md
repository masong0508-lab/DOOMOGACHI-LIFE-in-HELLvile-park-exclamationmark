# Hell Breach levels + arena modes (levels expansion)

Five new first- / third-person levels on top of the action module's
shooter, a level select with ranks, three play modes for the levels and
three arena modes. Everything runs on the shared core (no new art: the
mini-bosses are existing demons scaled, tinted and glowing through
`DoomoBillboard.scale` / `light` / `BB_TRANSLUCENT`).

## The levels

| # | Level | Map | Par / gold | Secrets | Mini-boss (base demon) | Mechanic |
|---|-------|-----|-----------|---------|------------------------|----------|
| 1 | The Vape Cellar (story) | `vape_cellar` | 2:30 / 1:35 | 2 | The Cellar Wraith (vape wraith) | ghostly |
| 2 | Cloud Nine Spire (story) | `cloud_nine_spire` | 5:00 / 3:20 | 1 | Lord Vapula | his own phases |
| 3 | Sugar Rush Sewers | `sugar_rush_sewers` | 3:30 / 2:20 | 3 | Gumdrop Gutterking (slime belcher, x2.2) | summons implings, throbs |
| 4 | Plushie Factory | `plushie_factory` | 3:40 / 2:30 | 3 | Foreman Fluffnuts (cyber piggy, x1.9) | calls hellhound pups |
| 5 | Graveyard Shift | `graveyard_shift` | 4:20 / 2:55 | 2 | The Weeping Widow (vape wraith, x2.4) | translucent, blinks between graves |
| 6 | Bone Cathedral | `bone_cathedral` | 4:40 / 3:10 | 3 | Bishop Bonebottom (bone jester, x2.3) | the organ pipes shoot fire |
| 7 | Hellvile Underbelly | `hellvile_underbelly` | 5:00 / 3:20 | 3 | Gatekeeper Grumbleguts (brute, x1.7) | imps pour out of the portals |

Each new map starts cute and turns hellish (the HUD and music follow
`combat_s.hell`), and uses floor / ceiling heights, stairs, ledges, pits,
lifts, doors (keys, switches, the boss's kill event), secrets, lava /
water and open-sky areas (the map headers in `core/data/maps/*.txt`
describe the routes). The Plushie Factory's conveyor belts are plain cells
tagged 240..243; the levels runtime pushes the player and pickups along.

## Scenes

* `SCENE_LEVEL_SELECT` (arg = `LVM_*` mode | (level + 1) << 4): a live 3D
  preview of the map (orbiting camera, its demons and props as
  billboards), rank pips, par / gold time, the mini-boss, a padlock for
  locked levels. L / R switch the mode, SELECT plays an autopilot demo.
* `SCENE_BREACH` (arg = level | mode << 8 | `LVF_DEMO`): intro card, the
  combat session + levels runtime, pause (weapon, camera, restart, quit),
  results tally with the rank stamp, death screen with retry.
* `SCENE_ARENA_MODE` (arg = `AM_*`): Boss Rush, Horde, Monster Mode Mayhem
  in the Demon Arena.
* **Control cards (clarity C10, CL6).** Every Breach level (every mode,
  Brutal Only included), the Demon Arena (`SCENE_ARENA`, free play) and
  the arena modes open on a card (`core/src/combat_card.c`): the goal,
  the mode's rule, the controls of the scheme picked in Options (Modern,
  Classic, Dual stick) with the pad's own button names (GBA "SEL+B", PS1
  "L1" / "Triangle", Dreamcast "START+Y") and the HUD legend (heart =
  health, shield = armour, skull = demons down, the top-right "beaten /
  all", or the arena's star = score). The first card
  of a visit waits for A (the Arena's B walks back out); a restart, a
  retry, "Again!", a demo and the autopilot start by themselves as before.
  The HUD's counters keep their icons (the legend names them: spelling
  "Demons 0/17" out cost the GBA combat frame 1-2 ms of outlined text),
  health is a heart in every mode, the ammo panel's "Bullets" line sits inside it,
  the pause row says "Music volume 7", the arena results say "You earned"
  (no arrows: it is not a choice). The story's fights (`SCENE_COMBAT`
  pushed by `story_push_combat` with `COMBAT_ARG_CARD`, the nests and the
  scripted fights) open on the same card on the first fight after
  power-on, frozen until A; the fights after it start at once as before
  (the nests come four in a row), and a bare `push COMBAT N` (the
  harnesses, debug, the GBA speed tour) never shows it. Tests:
  `clarity_c10_*` in `core/tests/test_clarity_cl6.c`; script
  `clarity_shooter.txt`.

## Modes

* **Hell Breach** - play for a rank: time vs par (40), kills (35) and
  secrets (25) -> S >= 90, A >= 70, B >= 45, else C. Ranks (3 bits a
  level) and the best Horde wave share the saved `PROG_BREACH_RANKS`.
* **Time Trial** (after any clear) - the big clock; beat the developers'
  time for a gold medal.
* **Cute-Only / Brutal-Only Run** (after 3 clears) - weapons of the other
  style are put away (Rivet lends one if you have none), gore follows the
  style.
* **Boss Rush** (after Lord Vapula) - all seven bosses back to back with a
  heal between rounds.
* **Horde** (Demon Arena unlocked) - endless escalating waves, a mini-boss
  every 5th wave and a SHOP BREAK every 3rd wave (candy heart, plush
  armor, ammo, Guardian Angel, Holy Fury) paid with the loot you picked up.
* **Monster Mode Mayhem** (turn Monster Mode on) - three minutes of tiny,
  big and huge demons, endless ammo and the berserk MONSTER METER.

All are activities (`doomo_activities_combat`): the Arena lists every
mode, Hellvile has the Underbelly gate and (at night) Graveyard Shift.
Unlocks are ids 864..890 (`core/src/unlocks_levels.c`): golds, mode
gates, feats, mini-boss trophies and three creator-part alternate routes.

## Map format v2 (LV1, 2026-11-13)

Zones, paths, swap sets, level-local textures, `tflags2` and the level header
keys, for the 24 new levels (plan section 8.1). All of it is ROM side tables
in a `DoomoMapExt` (`map.h`) that a v2 map's generated file defines next to
its `DoomoMapDef`; the 12 existing maps are v1 and their generated files are
unchanged (`make -C core check-gen`). Statements, limits and refusals:
`core/data/maps/README.md` "Format v2". Code: `tools/mapc.py` (parse, refuse,
emit, `--export`), `core/src/levels_mapx.c` (accessors; the `doomo_map_exts[]`
registry, empty until the first new level), `core/tools/map_bfs.h` (the
reachability search), `core/tools/mapreach.c`, `tools/levelcheck.py`.

* **Memory:** no resident byte on any platform. Nothing in `levels_mapx.c` is
  referenced by game code yet, so `--gc-sections` drops it; the registry is a
  4-byte ROM table. GBA `make -C gba sizes` is identical to before the package.
* **BFS:** `test_maps` (the `bfs_*`, `mapx_*` tests) runs the search on one test
  map per edge kind (`core/tests/maps/t2_zip, arc, tube, ride, raft, bounce,
  gravity, swap_once, swap_toggle, swap_stuck, hook, tables`); each proves the
  exit is reached with the map's tables and not without them. `t2_swap_stuck`
  is a deliberate soft lock the search must find.
* **Tools:** `make -C core levelcheck` and `mapc-check` are part of
  `make -C core test`.
* **Not yet:** the runtime that applies zones and
  swaps in play (LV3-LV5: the accessors exist and are unit tested), `texbank`
  art (LV7).

## The level script VM (LV2, 2026-11-13)

`on EVENT [repeat] do ACTION ; ACTION` lines in a map file compile to bytecode in
a ROM table (`DoomoMapExt.script`, `script_len`); `core/src/lv_script.c` runs it.
Bytecode, events, actions and limits: `core/include/doomo/lv_script.h`; the map
syntax: `core/data/maps/README.md`. The state is a caller-owned `LvVm` (about 84
bytes of data plus 4 pointers; variables 32 bytes), not part of `LvRun`, so no RAM is
spent until a level starts a VM.

* **Events:** start, tag, kill group cleared, timer (once or every N ticks), zone
  entered (by zone tag), usable used, variable reached a value, boss phase.
* **Actions:** open / close a remote tag, apply / revert a swap set (the VM
  changes the live map itself through `map_swap_apply` / `map_swap_revert`), set a
  zone parameter, spawn a dormant group, caption, bark, music cue, tint pulse,
  camera sweep (path, ticks, done tag; `lv_vm_frozen()` says input is frozen, the host
  gets the progress 0..256 each tick), set / add a variable (0..255, clamped), give /
  take a weapon, checkpoint, offer a choice (`lv_vm_choose()` answers: sets the variable and
  fires a tag), set a story flag, end the level, post a tag (chains handlers).
  Everything that leaves the VM goes out through one host function
  `void host(void *ctx, int op, int a, int b)`; the game wires it when a level uses a script.
* **Deterministic:** no clock, no rng, no pointers in the state hash; the same events give
  the same state and the same host calls (`lv_vm_hash`, tested).
* **Budgets (asserted):** 64 handlers, 2048 script bytes, 255 bytes per handler, 32 variables,
  16 actions per tick (a longer handler resumes next tick), 16 queued handlers (the extra is
  dropped and `err` gets `LVVE_QUEUE`), one open choice. `lv_script_check()` validates a script
  (structure, opcodes, variables, swap sets, zones, camera paths) before a run; mapc refuses the same.
* **Tests:** `core/tests/test_lv_script.c` (a test per action, per event, the budgets, the
  checker, and the compiled `core/tests/maps/t2_script.txt` run twice with equal hashes);
  `tools/tests/test_mapc.py` (`TestScripts`).
* **Memory:** nothing in the game calls the VM yet, so `--gc-sections` drops it on the GBA,
  PS1 and Dreamcast: GBA `make -C gba sizes` is unchanged, the PS1 EXE is identical. `lv_script`
  is in the PS1 combat overlay group (`ps1/tools/ovl_plan.py`), so a level using it later adds
  no resident byte there either.
* **Not yet (follow-up, the first package that needs it):** calling the VM from
  `lv_runtime_pre` / `lv_runtime_post` and the host function (needs `LvVm` somewhere: the
  mechanic union of LV3-LV5, or a block of the combat overlay), a `LVE_USE` source (usables:
  LV3), a `LVE_PHASE` source (boss phases), `levelcheck` counting the script bytes.

## Files

`core/src/levels.c` (tables, ranks, runtime, activities),
`levels_select.c`, `levels_play.c`, `levels_draw.c`, `arena_modes.c`,
`levels_int.h`, `unlocks_levels.c`, `core/data/maps/` + `core/gen/map_*.c`
for the five maps, `core/tests/test_levels.c`,
`core/tests/scripts/levels_*.txt`.

## Testing

`make -C core test` runs `test_levels.c` (map features, the autopilot
reaching every exit with the runtime on, FP / TP renders at 240x160 and
320x240 without holes, ranks, variants, conveyors, the scenes and every
arena mode) and the runner scripts `levels_select`, `levels_breach`,
`levels_modes`, `levels_arena` at both sizes. `test_maps.c` validates
every map (BFS from the start to the exit honouring step-up, lifts, doors
and keys). On the PS1 the same scripts run on the game disc:
`python3 ps1/harness/run.py --exe ps1/build/doomogachi.exe --cue
ps1/build/doomogachi.cue --script core/tests/scripts/levels_arena.txt --out DIR`.

Budgets: .bss 345 bytes, no .data, 7.7 KB const tables + strings, code
25.6 KB (GBA thumb -O2) / 39 KB (PS1 -Os, in the combat overlay group);
the five maps are 103 KB of ROM data (cells LZ-packed on the PS1).

## LV0: the registry, the records and the select pages (levels plan 2.5, 8.5, 9)

The level ids are fixed (saves use them): 0..6 the seven built levels, 7..15 Hell Breach 8-16, 16..30
New Game+ Notices 1-15 (`LV_COUNT` 31, `levels_int.h`). Nothing past id 6 has a map yet: each is registered
with `map = LV_NO_MAP` ("not built"), so it is not shown in the select and cannot be started. A level package
(LV12 and later) fills in its row and nothing else has to change.

* **`levels2.c`** holds the level table (`lv_info`, all 31 rows, the old seven moved out of `levels.c`) and the
  registry `lv_reg[]`: the page (Breach 1-7 / Hell Breach 8-16 / Arrears), the platform manifest flag
  (`LVG_GBA`: the seven old levels plus HB8, HB9, HB13, HB15, N1, N7, N14), a dark flag, the unlock rule and
  the text of the locked reason. `lv_listed()` says whether a build has a level, `lv_built()` whether it also
  has a map.
* **Unlock rules** (`lv_rule_eval`, `levels.c`): always, after a level, a story chapter, a story episode done
  (`LVK_BEAT`: the episode's done bit, or a later chapter so an older save that is further on is not locked
  out), a story flag, New Game+. A level of the Arrears page also needs New Game+ (or a finished arc). A
  level that was cleared once stays open. A Notice that follows one that is not on this platform follows that
  one's own rule, so the GBA's Notice 7 opens by the chain and not by a level it does not have. The rows name
  the plan's episodes where it names them; LV21 and LV22 re-point a row when the story lands a better beat.
* **`BRCH`** (`levels_rec.c`, save tag, slot 14, declared 160 B, `SAVE_EXT_SLOTS` 14): per new level 4 B (rank
  3 bits, gold, feats 3 bits, cleared; secrets; 16 collectible bits) and 48 B of arc state (act, 16 flag
  bits, the buy-back mask, two choice bytes, the fourth player line, up to 16 held resident ids, the previous
  run's last ending). A header carries the version and a 24-bit mask of the records that follow, so a save
  made on one platform loads on another (a record of a level the build does not list is skipped). Note: such a record is not kept as raw bytes, so a save moved to a platform that lists fewer levels and saved there loses the records of the levels that platform does not list. 148 B with
  all 24 levels, 80 B on the GBA. A fresh island writes the empty chunk; a save without the chunk, an empty
  or a bad one loads as "nothing recorded". The old seven keep `PROG_BREACH_RANKS` and their gold unlock ids
  (no migration). `lv_rank`, `lv_set_rank` and `lv_gold` answer for all 31.
* **Select pages** (`levels_select.c`): the level select shows only levels the build lists and has a map
  for, in pages. LEFT / RIGHT walk the page (they wrap), UP / DOWN turn the page (to the first level on it
  that still needs you), the title is the page's name and the pips are the page's levels (15 fit at 240 wide).
  With one page (today) the screen is pixel-identical to before. `SCENE_LEVEL_SELECT` takes the level to focus
  in bits 4..9 of its argument.
* **PS1.** Until the PS1-RAM package releases the LEVELS share of the resident memory (PARALLEL_WAVES.md section
  3) the PS1 build keeps the old seven only (`LV_TABLE_N`, `LV_HAS_NEW` 0 in `levels_int.h`): no new rows, no
  `BRCH` chunk, no page code. Flip those two macros when the share is released; the host and GBA builds run
  the full registry already. Measured: the PS1 EXE ends 112 B lower than before (the room it frees is paid
  for by the slot table and the packed size classes).
* **Tests**: `core/tests/test_levels_rec.c` (ids, pages, the GBA manifest, records that only improve,
  the arc, the `BRCH` round trip through a real save, the GBA's smaller chunk both ways, bad chunks, the joint
  budget, every unlock rule kind and the GBA chain, the select pages at both sizes) plus `test_save.c` (a
  save without `BRCH` leaves nothing). On the host `lv_list_req = LVG_GBA` makes the build list what the GBA
  lists, and `lv_info` is writable so a test can re-point a not-built level at an existing map.

## LV11: content notes and the dark-level presentation (levels plan 6.3, 6.4)

Nine levels are dark (`LVG_DARK` in the registry: HB9, HB11, HB12, N1, N8, N9, N10, N11, N13). Each has a
content note, and nothing in the game starts a dark level without showing it. No dark level has a map yet,
so none of this shows on a real run today; the host tests re-point a dark level at the Vape Cellar to play it.

* **Text** (`levels2.c`, `lv_notes[]`; `lv_note()`, `lv_note_short()`): the plan's nine notes, and a short
  line for each ("Note: guilt, failure, lost souls") for the strip and the later visits. The table's order is
  the order of the first-visit bit: only ever append. A test keeps the table and the `LVG_DARK` flags the
  same set. The plan says "at most three lines at 240x160": that holds for four of the nine; HB9, HB12, N1
  and N9 take four and N13 takes five at 240 wide. The page has room for five (86 px), so the plan's text is
  kept word for word.
* **The card's note page** (`levels_play.c`, `BR_NOTE`; `combat_card.c`, `ccard_note_page`): the first visit
  to a dark level opens on a window with the full note over the dimmed level, under the banner. A (after 12
  ticks) goes on to the control card and marks the note seen; B goes back to the select and marks nothing.
  A demo and the autopilot skip the page and mark nothing. A restart and a retry do not show it again.
* **Later visits**: the control card has the short note as one line above the goal (`ccard_draw_above_note`),
  unless Content notes is "First time only". A mode rule line (Cute-Only, Brutal-Only, Time Trial) takes the
  room the line needs at 240x160, so the line is left out then; the first visit's page showed the note.
* **The first-visit flag** (`levels_rec.c`, `lv_note_seen()` / `lv_note_set_seen()`): one bit per dark level in
  the 'BRCH' arc's spare bytes 42..43. The chunk size does not change; a chunk from before LV11 reads "nothing
  seen". A level with no note reads "seen". The GBA keeps the bits of the dark levels it does not list.
* **Save growth**: the world's OPT2 chunk is 2 bytes longer on every platform that has the two ids (not the
  PS1); an older shorter chunk still loads. `test_save.c` allows one byte more for the extra LZ length byte.
* **The select** (`levels_select.c`): a small moon badge next to the tag pill, and the short note as a strip
  on the preview's bottom edge (where "Clear it once to race the clock" goes, which wins). The strip and badge
  also show on a locked dark level (it is a content warning; the owner may override).
* **Options** (`options_store.c`, add-only ids at the end of OPT2): `OPT_NOTES` "Content notes": Every time
  (default) / First time only; `OPT_MEMORIAL` "Memorial names": On (default) / Off. Both are on the Game page,
  machine-wide (the config record's byte 16: bit 0 first time only, bit 1 memorial Off, so 0 is both defaults
  and an older record reads right; without a record the 'OPT2' chunk carries them), and both are bits in the
  byte the Stick option already uses (no EWRAM). The memorial row only shows once Notice 13 is built and open.
  A preset does not touch them. `lv_memorial_label(name)` is what the Vault's memorial wall prints for each
  name: the name, or "Someone we lost." when Off (the Vault's package, LV41, calls it).
* **Later-visit line and mode rules**: with a mode rule line on the card (Cute-Only, Brutal-Only, Time Trial)
  the one-line note is not drawn (the card would be 71 px against a 66 px budget at 240x160), whatever
  Content notes says. The first visit's page always shows.
* **The note page holds still** (round 3): after the dim is in (12 ticks) the page uses S2-1's modal hold like
  the control card does, so only the banner's band is repainted and the held 3D view is not redrawn each frame.
  Not measured on a GBA: no dark level has a map yet, so no harness script can reach the page (the first
  dark level's package, LV3 and later, measures it with `fps` on the page). The first-visit bit is saved with
  the next island save.
* **PS1**: nothing. The PS1 lists no dark level, so `town.h` has no `OPT_NOTES` / `OPT_MEMORIAL` there, the
  card and play code is behind `LV_HAS_NEW`. Measured 2026-11-15 on the tip (`249ae551`) and the tip + LV11
  (clean PS1 builds with the boot FMV): `ps1/build/doomogachi.exe` and `OVL.BIN` are byte-identical, `_end`
  `0x801df9f0` both, **132,608 B below the stack top** both (delta 0 B; LEVELS' PS1 resident share used by this
  package: 0 B). `game_smoke` PASS. When the LEVELS share of PS1 RAM is used for the dark levels, add the
  ids and flip `LV_HAS_NEW`.
* **GBA** (measured 2026-11-15 on the tip and the tip + LV11, `make -C gba sizes`): `.iwram` 11,272 B,
  `.iwram_bss` 4,088 B, `.data` 816 B, `.bss` 254,836 B, every overlay group the same before and after
  (net zero); ROM `.text` 1,500,088 -> 1,501,472 B (+1,384 B), `.rodata` 7,693,800 -> 7,695,628 B (+1,828 B).
  Vape Cellar autopilot (combat, reference only), before -> after, fps drawn start room / middle / late:
  8.8 / 8.4 / 16.6 -> 8.8 / 8.3 / 16.4 (the notes code is not on the combat path; the small differences are
  the autopilot's timing after the code moved). `make -C gba test` passes.
* **Visible change on the other platforms**: the Options Game page has one more row, "Content notes", on
  the GBA, Dreamcast and native builds (not the PS1). It sits below the first six rows, so the page as it
  opens (240x160 and 320x240) is pixel for pixel the same; the row shows when the list scrolls. Nothing else
  changes in any existing level's card or select: the note, the badge and the strip need a dark level.
* **Tests**: `test_levels_rec.c` (`lv11_*`: the notes and the dark flags are the same set, every line fits at
  both sizes, the flag in a real save and on the GBA's list, the first visit's flow with B / A / a later
  visit / a demo, the pages and the badge at both sizes (screenshots), the memorial label, the options
  rows), `test_options_store.c` (`lv11_*`: round trip through the chunk and the record, an older record and
  chunk), `test_tips.c` (the Game page has one more row). Not done here: the Vault wires
  `lv_memorial_label` (LV41); the `note TEXTID` map header key (a note per map) is LV2's; Spanish text.

## Level test harness (LV10)

One list makes every level's scripts: `LEVELS` in `tools/breach_scripts.py` (a `make -C core gen`
generator, so `check-gen` catches a stale file; it refuses to run when the list and `LV_OLD_COUNT` (the built levels) differ).
Per level it writes `core/tests/scripts/levels2_SLUG.txt` (the runner: intro card, control card,
first and third person, then the autopilot in two rooms; `make -C core test` plays it at 240x160 and
320x240), `gba/harness/scripts/breach_SLUG.txt` (`push BREACH level|LVF_DEMO`, `sync on`, three fps
windows of 130 ticks in the start room, the middle and the late part, a `vhash-seq` (the hash is printed so two runs can be compared by hand; nothing gates it),
shots) and the PS1 and Dreamcast versions of it (`tools/speed2.py` turns the windows into `fps` /
`fpsend` for `ps1/harness/run.py` and `log` / `mark-ticks` / `expect-ticks` for `dcrun`). Windows are
labelled `breach SLUG: WHAT`.

- GBA: `make -C gba test-breach2 [BREACH2="sewers ..."] [BREACH2_GATES=1]` (about 1 minute for all 7). The
  `expect-fps` floors are 85% of the first measurement and only fail with `BREACH2_GATES=1`.
- PS1: `python3 tools/breach_scripts.py` writes `ps1/harness/scripts/breach_SLUG.txt`; run one with
  `python3 ps1/harness/run.py --exe ps1/build/doomogachi.exe --cue ps1/build/doomogachi.cue --script
  ps1/harness/scripts/breach_sewers.txt --out DIR` (the table: `tools/breach_fps.py ps1=DIR/tty.log`).
- Dreamcast: `dc/harness/scripts/breach_SLUG.txt` for `dcrun` (the table: `tools/breach_fps.py dc=LOG`).
- `tools/breach_fps.py gba=LOG ps1=LOG dc=LOG` prints the platform x level x window table (fps, logic
  ticks/s, frames by vblanks, worst frame, verdict).
- `core/tests/test_levels2.c`: the autopilot finishes every level at Plushie in Hell Breach and Time
  Trial mode (asserted). `LEVELS2_SWEEP=1 core/build/tests ... levels2_sweep` prints the whole plan matrix
  (4 modes x Plushie / Normal / Ultra-Hellvile). Measured 2026-11-13: the autopilot finishes 44 of 84
  cells; the challenge modes (no weapon choice) and the Normal / Ultra cells of some levels stall. That
  gap is the autopilot's (LV6), not the levels'; move a cell into the asserted set when it closes.

GBA fps of the autopilot, 2026-11-13, drawn fps per window (start room / middle / late): Vape Cellar
8.7 / 8.4 / 16.7, Cloud Nine 4.0 / 4.0 / 5.2, Sewers 6.3 / 5.7 / 10.8, Factory 5.5 / 4.9 / 6.9,
Graveyard 5.3 / 10.7 / 5.2, Cathedral 4.9 / 7.9 / 10.9, Underbelly 6.4 / 6.5 / 9.0. Far below 30:
combat is the one scene the 30 fps rule excepts, and the speed stream owns it. Each level's `vhash-seq`
was the same in two runs. No resident byte changed (tools, tests and scripts only).


## LV7a: the shared textures and skies (art kit, first landing)

Dreamcast RAM note: `s_tslot` / `s_tstamp` in `dc/src/view3d.c` and `overworld.c` are sized by `TEX_COUNT`, which is 55 now (was 48): about 7 x (light levels x 4 B) more static RAM, small against 16 MB.

Plan section 8.6, package LV7. This landing is the art that every level can use at once; the demon
sprite sets, the Duchess portrait, the per-level banks and all the audio are the follow-up (status
file, "LV7b").

- **7 shared textures**, ids 48..54 (`textures.h`, generated by `core/art/tex_lv.c`): `TEX_STEAM_TILE`,
  `TEX_PIPE_STEEL`, `TEX_OFFICE_CARPET`, `TEX_CUBICLE_FABRIC`, `TEX_FELT`, `TEX_LEDGER_PAGE`,
  `TEX_VAULT_METAL`. 64 x 64, seamless, one frame each. The plan said 49..55, but the old art ends at id 47, so they follow it without a hole and id 55 stays
  free. Ids 56..63 stay free for the level slots (a map's `texbank`, map format v2); `TEX_COUNT_ALL` is 55
  (a test pins it).
- **3 skies**, ids 5..7 (`skies.c`): `SKY_OVERCAST` (the dark levels), `SKY_INK` (the Ledger),
  `SKY_DUSK_LANTERNS` (the Mingle). `SKY_COUNT_ALL` 8. They have names and notes in the Library's
  gallery room (it counts them in its percentage).
- Frame limits grew: `DOOMO_TEX_MAX_FRAMES` 72 (67 used), `DOOMO_SKY_MAX_FRAMES` 8 (8 used).
- mapc and levelcheck accept the new ids (`TEX_COUNT_ALL` / `SKY_COUNT_ALL`).

**The PS1 does not carry them.** `TEX_COUNT` and `SKY_COUNT` are macros: the old counts (48 and 5) when
`DOOMO_PLATFORM_PS1` is defined, `TEX_COUNT_ALL` / `SKY_COUNT_ALL` everywhere else. Every table and
every bound check of the core therefore keeps its old size there, and an id of the new art reads as
"no texture" (the PS1 builds the old seven levels only, `LV_HAS_NEW` 0). `art_data.c` puts the new
frames and table rows last, behind `#ifndef DOOMO_PLATFORM_PS1`, and `ps1/tools/packart.py` stops its
LZ pack at that line (`tools/tests/test_packart.py` runs it and compiles the PS1 view). The new art
reaches the PS1 with the level packs (LV8), when PS1-RAM has landed. Dreamcast and host take all of it.

**GBA.** The 7 textures and 3 skies are `const` ROM data (+28 KB and +72 KB of `.rodata`, see the
numbers in the status file); the GBA is a 32 MB cartridge. The overworld's per-frame texture table
(`OwLit` in `gba/src/overworld.c`) grows by 7 pointers; its separate `frame_tex_ok` flag array is
gone (a NULL pointer means "not looked up yet this frame"), which pays for them: net RAM change is
negative. Existing levels use no new id, so every shot is unchanged.

**Measured (2026-11-15, base `fe410024`, same commands before and after, both trees built clean).** GBA
(`make -C gba sizes`): `.rodata` 8,246,748 -> 8,349,684 B (**+102,936 B ROM**: 7 x 4,096 + 3 x 24,576 of pixels
+ 512 of tables and text; the cartridge is 32 MB), `.text` +64 B, `.bss` 254,828 -> 254,812 B (**-16 B
EWRAM**), `.iwram` 11,272, `.iwram_bss` 4,088, `.data` 816 and the four overlay slots unchanged (GBA ledger
row 60). PS1 (`cmake --build --preset ps1`, boot FMV): `doomogachi.exe` and `OVL.BIN` are **byte-identical**
to the base's (`cmp`), 130,000 B below the stack top before and after (**delta 0 B**; none of the 8 KB LEVELS
share used). Core tests 1,386 passed, 0 failed.

**Review sheet.** `core/build/artgen --sheet DIR --zoom 1|2` writes the contact sheets (`textures.png`,
`tiles_a/b.png` with the 3 x 3 tiling and four light levels, `skies.png` with a 1.5 x wrap to show the
seam). Cuts: nothing here is adult-only; no gore; no faces.

**Review sheets (2026-11-15).** `core/build/artgen --review DIR` writes pages of exactly 240 x 160 (GBA) and
320 x 240 (PS1, Dreamcast) at 1:1: `review_tex_WxH_N.png` (the 7 new textures first, marked `+`, then the
old ones they sit beside: metal floor, tech panel, carpet, tile floor, wood floor, rust metal, heart
wallpaper, grass) and `review_sky_WxH_N.png` (the 3 new skies first, then night, hell, vape storm and
sunset, the lower band of each as it shows at that size). All pages were looked at: every texture reads as
what its name says at 1:1 (steam tile green with a grid, pipe steel a stack of pipes, office carpet dark
speckled blue-grey, cubicle fabric flat orange with seams, felt green speckle, ledger page cream with
ruled lines and a red margin, vault metal riveted plates), none is a copy of an old one, and the skies wrap
without a visible seam. Nothing was redrawn.

## The texture bank (LV7a, plan 8.6 and 8.8)

A level has up to 8 textures of its own on the ids 56..63 (`texbank N NAME` in its map, mapc already checks
the names). This landing adds the bytes behind those names: the format, a generator, the reader and a
sample. No level uses a bank yet.

* **Format** (`core/include/doomo/texbank.h`, version 1, little endian): `LVTB`, version, slot count 1..8,
  total length; then 16 bytes per slot (name of at most 11 lowercase characters, frames 1..4, speed, mode
  loop or pingpong, average colour); then 4,096 bytes of palette indices per frame, the same pixels as the
  shared textures. At most 32 frames. 8 static textures are 32,908 B (the plan allows 300 KB of art per
  GBA level, 96 KB per pack on the disc).
* **Source and generator.** `core/data/texbanks/NAME.bank` is a small text file (`tex SLOT [frames=N]
  [speed=T] [mode=...] KIND args`; kinds fill, checker, bricks, stripes, noise; colours `RAMP.shade`).
  `tools/texbank.py` (a `make gen` generator) writes `core/gen/tb_NAME.c`, and `python3 tools/texbank.py
  --bin NAME FILE` or `import texbank; texbank.build_file(path)` gives the raw blob: these are the
  **pack-ready bytes** that `tools/packbreach.py` (LV8) copies into the level pack unchanged. Every refusal
  names its line (`tools/tests/test_texbank.py`, run by `make -C core mapc-check`).
* **Reader.** `texbank_open` checks the blob (magic, version, count, length, names, frames, modes) and fills
  a small view; the reader keeps no state. `texbank_activate(&bank)` makes it the level's: then
  `texture_pixels`, `texture_image`, `texture_frames`, `texture_is_animated`, `texture_avg` and
  `texture_name` answer the ids 56..63 from it (a fresh image uid per activation, so a renderer that caches
  by uid never shows the bank before). An id the bank lacks, or no bank, is the old fallback (grass). The
  shared ids do not change.
* **Where it is compiled.** The host and the Dreamcast (`DOOMO_TEXBANK_ACTIVE`). The GBA and the PS1 compile
  nothing of it (the hook, the reader's state and `tb_sample.c` are behind the macro; GBA ROM and EWRAM and
  the PS1 EXE are unchanged, shown above). Their level packs read the same blob with their own code (LV8,
  when PS1-RAM's room is released; the PS1's 8 KB resident share is not touched here).
* **Test.** `core/tests/test_texbank.c`: the sample opens and answers by name, frame and tick; every error code (a bad
  magic, version, count, length, name, frame count, mode) is refused and leave the view untouched; the hook answers 56..63 and nothing else; a room
  whose walls and floor are bank textures is drawn by the raycaster at 240 x 160 and at 320 x 240 (no
  unwritten pixel, the wall's pixels are only the bank's colours, the same picture twice, a different
  picture for another slot or no bank). Pictures: `texbank_240x160.png`, `texbank_320x240.png` in the
  round's evidence.
* **Not here (LV7b).** The 4 new demon sprite sets, the Duchess portrait, the prop / usable / collectible
  banks (they need the same pack format with sprites), the 6 songs and the SFX (47,793 of 49,152 B used:
  an owner decision), and the GBA and PS1 readers of the texture bank.

