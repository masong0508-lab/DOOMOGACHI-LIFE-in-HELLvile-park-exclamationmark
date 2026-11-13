# core/data/maps — ASCII map sources

Every `*.txt` here is one map in the shared 2.5D cell format (`map.h`).
`tools/mapc.py` compiles each file into its own generated C file:

```
core/data/maps/vape_cellar.txt  ->  core/gen/map_vape_cellar.c
                                    const DoomoMapDef map_def_vape_cellar
```

`core/src/maps.c` (action) registers the maps by **weak** reference to
`map_def_<name>`, so a map links in as soon as its generated file exists:

| map id | source file | owner |
|---|---|---|
| `MAPID_HAHALAND` | `hahaland.txt` | story |
| `MAPID_HAHALAND_INVADED` | `hahaland_invaded.txt` | story |
| `MAPID_HELLVILE` | `hellvile.txt` | story |
| `MAPID_VAPE_CELLAR` | `vape_cellar.txt` | action |
| `MAPID_CLOUD_NINE_SPIRE` | `cloud_nine_spire.txt` | action |
| `MAPID_ARENA` | `arena.txt` | action |
| `MAPID_PARK_UNITED` | `park_united.txt` | story |

A missing map falls back to a small built-in pastel room, so nothing
crashes while a module is still landing.

```
python3 tools/mapc.py              # compile every map (also part of make -C core gen)
python3 tools/mapc.py --check -v   # validate only, print a summary
make -C core check-gen             # CI: generated files are up to date
```

## File format

One statement per line, `#` starts a comment line. Keywords:

```
map vape_cellar            C name -> map_def_vape_cellar (default: file name)
title "The Vape Cellar"    shown in menus / level intro (<= 31 chars)
kind dungeon               overworld | dungeon | arena
sky HELL                   SKY_* (prefix optional): PASTEL_DAY SUNSET NIGHT_STARS HELL VAPE_STORM
music HELL_BREACH          SONG_* (prefix optional)
ambient 10                 light 0..15 used by cells whose light is 0

cell C key=value ...       legend: what the grid character C means
grid ... end               the map, one character per cell (rows = y, columns = x)
floorh ... end             optional overlay: floor heights per cell
ceilh ... end              optional overlay: ceiling heights per cell
light ... end              optional overlay: light per cell
thing X Y TYPE [key=value ...]   place a thing (also possible via the legend)
```

Coordinates: x grows east (columns), y grows south (rows). A cell is
`DOOMO_CELL_UNITS` = 64 world units square. Heights are in steps of
`DOOMO_HEIGHT_STEP` = 8 world units: `fh=2` is 16 units, `ch=16` is 128.
Maps are at most 64 x 64 cells; every row must have the same width.

### Cell legend keys

| key | meaning | default |
|---|---|---|
| `wall=TEX` | solid cell with this wall texture (`TEX_*`, prefix optional). `none`/`0` = open | open |
| `floor=TEX` | floor texture (also the overworld tile) | required for open cells |
| `ceil=TEX` / `ceil=sky` | ceiling texture, or open sky | required for open cells |
| `fh=N` | floor height, -128..127 steps | 0 |
| `ch=N` | ceiling height (roofed cells) / block top (walls under the sky) | 16 |
| `light=N` | 0..15, 0 = map `ambient` | 0 |
| `tag=N` | 0..255, meaning depends on the flags (below) | 0 |
| `flags=a,b` | `door lift lava water secret exit portal trigger` | none |
| `like=C` | start from the definition of legend char `C`, then override | |
| `thing=TYPE` | also place a thing in every cell with this char (`angle=` `arg=` `tflags=` apply to it) | |

### What the renderers do with it

* An **open cell** draws its floor, and its ceiling unless `ceil=sky`.
* Neighbouring open cells of different heights get **step faces**: a
  higher floor shows a lower face textured with that cell's floor, a
  lower ceiling shows an upper face textured with that cell's ceiling.
  Use this everywhere: stairs, ledges, pits, beams, soffits, crawlspaces.
* A **wall** (`wall=TEX`) under a roofed neighbour closes the view. Next to
  open sky it is a block as tall as its `ch` (if above its `fh`, else 256
  units) and you can see over it to taller things behind — towers, fences,
  hedges, houses. On the overworld a wall is a raised block with a front
  face; `floor=` on a wall cell is its roof texture.
* A **roofed cell seen from under the sky** (a doorway, a gallery, a
  tunnel mouth) is drawn as a block from its ceiling up to its *roof*:
  the tallest `ch` of the wall cells next to it, else 256 units above the
  floor it is seen from, with the sky above. So frame roofed areas that
  open onto the sky with walls (door posts, pillars) whose `ch` is the
  roof height you want, or the edge reads as a tall slab. The map's outer
  border needs no care: beyond it the sky shows.
* The overworld shows `floor_tex` as the tile, pits (`fh < 0`) darker,
  raised floors as lifted blocks, cells with `light >= 13` glow at night.

### Movement rules (the shooter and the overworld)

* The player is 56 units tall (32 crouched), eye at 40. A floor up to
  `DOOMO_STEP_UP` = 24 units (3 steps) higher can be walked onto; a jump
  plus a mantle reaches about 53 units (6 steps). Drops of any height are
  fine (falling takes no damage, lava does).
* A cell is passable if the gap between its ceiling and the floor you
  stand on is at least 56 units, or 32 if you crouch (crawlspaces
  auto-crouch). Flying demons ignore floors but not walls and ceilings.

### Flags and tags

| flag | runtime behaviour |
|---|---|
| `door` | `fh` = sill, `ch` = the OPEN height, `ceil=` is the door's face texture (use a wall texture for a **secret door**). Doors start closed. `tag 0`: opens when bumped, stays open. `tag 1`/`2`: needs the red / blue key. `tag 3..15`: opens when bumped, closes again after ~4 s. `tag >= 16`: remote, opened only by an event with that tag (trigger cells, cleared demon groups, bosses) |
| `lift` | `fh` = rest height, `tag` = the other height (signed, -128..127 steps). Standing on it sends it there; it waits ~3 s, then returns. |
| `trigger` | entering fires event `tag` once (opens remote doors with that tag, wakes `dormant` things with `arg = tag`) and calls `story_trigger(tag)` |
| `secret` | entering counts a secret (once per cell, "A secret!" toast) |
| `exit` | entering finishes the level |
| `lava` | hurts every 16 ticks (CUTE gore: it is hot cocoa, it still hurts) |
| `water` | slows you down, animated |
| `portal` | animated vape portal: the arena and invasion encounters spawn demons here |

Tags 16..63 are free for map events; story uses 64..255 for
`story_trigger()` (they still work as event tags).

### Overlays

`floorh`, `ceilh` and `light` blocks have the same size as the grid.
Each character overrides one cell; `.` or a space keeps the legend value
and rows (or the whole block) may be shorter than the grid (the rest is kept). Height digits
are base 36: `0-9 a-z` = 0..35, `A-Z` = -1..-26. Light digits are `0-9 a-f`.
They keep the legend small: one character per *material*, heights in the
overlay (stairs are just `ABCDEF` in a row).

### Things

`thing X Y TYPE [angle=DEG] [arg=N] [tflags=...]`, or `thing=TYPE` in a
legend entry. `TYPE` is a `THING_*` name (prefix optional): `PLAYER_START`,
`NPC` (`arg` = roster id low byte, used by story), demons `IMPLING
VAPE_WRAITH PUFF_DEMON HELLHOUND_PUP BONE_JESTER SLIME_BELCHER GARGOYLE
CYBER_PIGGY BRUTE CLOUD_SPIDER LORD_VAPULA`, pickups `HEALTH_SMALL
HEALTH_BIG ARMOR AMMO_BULLETS AMMO_SHELLS AMMO_CELLS AMMO_ROCKETS KEY_RED
KEY_BLUE SCRAP SOUL_ORB CANDY`, props `PROP_TREE PROP_LAMP PROP_BENCH
PROP_FOUNTAIN PROP_BARREL PROP_SKULLS PROP_TORCH PROP_SIGN PROP_FLOWERS
PROP_MAILBOX VAPE_PORTAL`, and `SPAWNER` (arena / invasion spawn mark).
`angle` is in degrees, 0 = east, 90 = south (y grows downward), 180 = west,
270 = north.

`tflags` (comma list):

| tflag | meaning |
|---|---|
| `ambush` | the demon waits until it sees you |
| `not_easy` | absent on the Plushie difficulty |
| `hard` | only on Nightmare and Ultra-Hellvile |
| `dormant` | appears in a vape puff when event `arg` fires (trigger cell) |
| `boss` | boss / mini-boss: more HP and a boss health bar |
| `kill_event` | when every demon with this `arg` is dead, fire event `arg` (`arg + 1` if also `dormant`) |
| `secret` | the pickup counts as a secret |

### Validation (mapc and `core/tests/test_maps.c`)

mapc refuses: unknown keys / textures / things, rows of different widths,
grid characters without a legend entry, open cells without floor or
ceiling texture, roofed cells whose ceiling is not above the floor, doors
without a door texture, lifts whose other height equals the rest height or
that would hit their ceiling, things inside walls, more than 255 things,
and (except for overworld maps) anything but exactly one `PLAYER_START`.
`test_maps` additionally checks that the exit of every dungeon is
reachable from the start (walking with step-up, lifts, doors and keys).
Format v2 statements and checks: see "Format v2" below.

## Format v2 (zones, paths, swap sets, texbank, header keys; LV1)

The 8-byte cell, the cell flags and the thing flags are full, so nothing new
goes into them. A map opts in with a `format 2` line (or by using any
statement below). mapc then also writes `const DoomoMapExt map_ext_<name>`
(map.h) next to `map_def_<name>`: ROM side tables, no resident bytes. A map
without v2 statements is a v1 map: its generated file is byte for byte what
it was, and `make -C core check-gen` proves it for all 12 maps. Accessors:
`core/src/levels_mapx.c` (`map_ext_of`, `map_zone_at`, `map_path_point`,
`map_swap_apply`, ...); a new level adds `&map_ext_<name>` to
`doomo_map_exts[]` there.

Header keys (each at most once):

| key | stored as |
|---|---|
| `format 2` | marks a v2 map |
| `fog N` | fog distance in cells, 1..40 |
| `view N` | view distance in cells, 4..40 |
| `gravity N` | percent of normal gravity, 10..250 |
| `dark yes` | a dark level: needs `note` |
| `note TEXTID` | content note text id, 1..65535 |
| `gba yes` / `gba no` | `yes`: mapc enforces the GBA budgets (below) |
| `detail a,b,c` | what the frame-rate governor drops first: `floors sky props fx far light` (1..3 names) |
| `hero LOOK LOADOUT` | hero override: two numbers 0..255 |

`gba yes` budgets (plan 7.2): at most 40 x 40 cells, at least 75% of the open
cells roofed, no open-sky patch wider than 8 x 8, `view` 12..16, at most 120
things, and in every room of 16+ cells the 4 most used textures cover 75% of
the surfaces.

Blocks and statements:

```
zone X0 Y0 X1 Y1 KIND [arg=N] [tag=N]   up to 32 rectangles (inclusive corners)
path ID [kind=KIND] x,y x,y ...          up to 8 paths of 2..32 points
swap NAME [event=N] [toggle] ... end     up to 16 named sets, 256 cell overrides in all
texbank N TEXNAME                        level-local texture N (0..7) = id 56 + N
tflags2=a,b                              on a thing, a legend entry or `thing` line
```

* Zone KINDs: `fog tide gravity dark noise face_a face_b ward chase safe
  bounce web`. `gravity` needs `arg=1..250` (percent), `bounce` needs
  `arg=1..127` (rise in height steps). Zones of one kind may not overlap;
  `face_a`/`face_b` and `safe`/`chase` may not overlap each other.
* Path KINDs: `route` (default, nobody rides it), `zip` (first point to last,
  one way), `ride` (board and leave at any point), `raft` (a point to any
  later point), `tube` (first <-> last), `arc` (a bounce arc, first to
  last), `chase`. Points must be on cells that are open or that a swap opens.
* `swap` is a sparse grid block like `floorh`: each character is a legend
  entry that replaces that cell while the set is applied (`.` or a space
  keeps the cell). Without `event=N` only the level script (LV2) applies it;
  `event=N` makes it a `once` set applied when map event N fires (a trigger
  cell, a cleared `kill_event` group); `event=N toggle` flips it each time
  (a lever). Refused: doors and lifts, a cell in two sets, an override equal
  to the cell, a wall in the PLAYER_START, an opened cell on the map border,
  and an override on a wall that stays a wall that no open cell touches.
  Stored as XOR deltas, so apply and revert are the same operation.
* `texbank` names must be lowercase identifiers of at most 11 characters and
  not a shared `TEX_*` name; use them as `wall=`, `floor=`, `ceil=` in legend
  entries.
* `tflags2` flags: `escort collect sleeper splitter frozen usable`.
* `on EVENT do ACTION` (level scripts) is refused until the script VM (LV2).

mapc also refuses: zones or paths outside the map, overlapping exclusive
zones, a `dark yes` map without `note`, a `gba yes` map that breaks the
budgets, `format` other than 2, and every malformed statement.

Checks (`make -C core test` runs all of them):

```
make -C core mapc-check    tools/tests/test_mapc.py: a test per statement and per refusal
make -C core levelcheck    tools/levelcheck.py: on every map, mapc's rules, the ROM size,
                           and the reachability search (exit, pickups, soft locks)
python3 tools/mapc.py --export DIR FILE.txt    the compiled map as plain numbers (.mapx)
```

The reachability search is `core/tools/map_bfs.h`, one implementation used by
`core/tests/test_maps.c` (on the registered maps and on the BFS test maps in
`core/tests/maps/t2_*.txt`, compiled into the test binary only) and by
`core/tools/mapreach.c` (the tool levelcheck runs). Its edges: walking (step
up, lifts, doors, keys, remote doors open), zip / arc / tube / ride / raft
paths, swap sets applied by trigger cells and kill events, levers flipping
toggle sets, gravity zones and `gravity` scaling the step-up, bounce zones.
It reports every cell reachable in some state, whether an exit is reachable,
and soft locks (a reachable state from which no exit is reachable). Mechanic
kits (stamp doors, laid bricks, named rooms: LV3-LV5) add their own edges
through the `step` and `extra` hooks. There is one test map per edge kind.

## Example

```
map tiny_room
title "Tiny Room"
kind dungeon
sky PASTEL_DAY
music HAHALAND_DAY
ambient 12

cell # wall=PASTEL_BRICK
cell . floor=WOOD_FLOOR ceil=CLOUD_PUFF fh=0 ch=14 light=13
cell s like=.                         # stairs: heights come from floorh
cell D floor=WOOD_FLOOR ceil=DOOR_CUTE fh=0 ch=12 flags=door
cell L floor=PENTAGRAM_FLOOR ceil=CLOUD_PUFF fh=0 ch=14 flags=lift tag=6
cell x floor=TILE_FLOOR ceil=sky fh=0 flags=exit
cell p like=. thing=PLAYER_START angle=0

grid
########
#p.sssL#
#......#
###D####
#xxxxxx#
########
end

floorh
.
...123
end

thing 5 2 IMPLING angle=180
```
