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
24 KB (GBA thumb) / 37 KB (PS1 -Os); the five maps are 103 KB of ROM
data (cells LZ-packed on the PS1).
