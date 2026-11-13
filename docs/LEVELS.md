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
* **Not yet:** `on EVENT do ACTION` (LV2), the runtime that applies zones and
  swaps in play (LV3-LV5: the accessors exist and are unit tested), `texbank`
  art (LV7).

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

## Level test harness (LV10)

One list makes every level's scripts: `LEVELS` in `tools/breach_scripts.py` (a `make -C core gen`
generator, so `check-gen` catches a stale file; it refuses to run when the list and `LV_COUNT` differ).
Per level it writes `core/tests/scripts/levels2_SLUG.txt` (the runner: intro card, control card,
first and third person, then the autopilot in two rooms; `make -C core test` plays it at 240x160 and
320x240), `gba/harness/scripts/breach_SLUG.txt` (`push BREACH level|LVF_DEMO`, `sync on`, three fps
windows of 130 ticks in the start room, the middle and the late part, a `vhash-seq` for determinism,
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

