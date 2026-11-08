# core/native — the headless native platform

The native platform runs the shared C99 core on the host with no window,
no audio device and no input device. It is what `make -C core test` and
CI use, and how every team looks at its screens. It implements every
`plat_*` function in `core/include/doomo/platform.h`:

| File | What |
|---|---|
| `plat_native.c` | 8bpp software framebuffer (any size up to 640x480; default 240x160, `--w 320 --h 240` for PS1 layouts), 2D drawing (`plat_rect`, `plat_shade_rect`, `plat_line`, `plat_text`, `plat_image` with flip, silhouette and drop shadow), file-backed saves (`<saves>/slot0.sav`, `slot1.sav`, written atomically), counted audio calls, `plat_log` (records every `STUB:` line), a fixed `plat_entropy()` so runs are reproducible |
| `panel.c` | (G7) the retained-panel cache behind `plat_panel_begin` / `plat_panel_end` (platform.h): off by default (`nat_panel_cache = 0`, every panel drawn every frame, so screenshots never depend on it); `core/tests/test_panel.c` turns it on and compares every frame with the cache off. It keeps panels the GBA's way: body = `plat_rect` / straight `plat_line` pixels, everything else must land on body, shading is redrawn by the core |
| `raycast.c` | the **reference software height raycaster** behind `plat_view3d` (semantics below) |
| `overworld.c` | the top-down `plat_overworld` (semantics below) |
| `png.c` | dependency-free PNG writer (indexed colour, stored deflate, zlib Adler-32, CRC-32) |
| `runner_main.c` | the scripted runner `$(BUILD)/runner` |

## Running

```
make -C core runner                 # -> core/build/runner (BUILD=dir to move it)
core/build/runner --script FILE --shots DIR [--w 240 --h 160]
       [--frames N] [--saves DIR] [--seed N] [--zoom N] [--gba-input]
       [--assert-no-stubs] [--quiet] [--verbose] [--budgets FILE]
```

* `--gba-input` sets `DOOMO_INPUT_SYNTH_XY` like the GBA does (SELECT+A = X,
  SELECT+B = Y).
* `--assert-no-stubs` exits with status 2 if any weak placeholder from
  `core/src/stubs.c` ran (`plat_log("STUB: ...")`). The final integration
  playthrough uses it.
* `--zoom N` writes screenshots scaled N times (nearest neighbour).
* Without a script the runner plays 300 frames (or `--frames N`).

### Script commands

One command per line; `#` starts a comment.

| Command | Meaning |
|---|---|
| `wait N` | run N frames (30 Hz ticks) with the currently held buttons |
| `press BTN [BTN..]` | hold for 1 frame, then 1 frame released |
| `hold BTN N` | hold BTN for N frames, then release it |
| `down BTN` / `up BTN` / `release` | hold, release one, release all |
| `shot NAME` | write `SHOTS/NAME.png` of the last frame |
| `type "text"` | type into the open text entry, one character per frame (`\n` = OK, `\b` = delete); fails if no keyboard is open |
| `expect-scene NAME` | fail unless the top scene is `NAME` (`TITLE`, `UI_GALLERY`, ... `SCENE_` prefix optional) |
| `scene NAME [ARG]` / `push NAME [ARG]` | `scene_set` / `scene_push` by name (e.g. `scene UI_GALLERY`) |
| `save SLOT` / `load SLOT` / `expect-save SLOT` | exercise the save system |
| `log TEXT` | print a line |
| `no-stubs` | fail now if any stub has run |
| `journey-begin NAME` / `mark NAME` | (M0) start the journey meter: from here every press (`press`, `hold`, `down`, each A of `skip-talk`/`skip-to`, the `\n` OK of `type`) counts 1 press, every frame 1 tick, every typed letter 1 typed; depth = scene pushes + modals above the start |
| `journey-end [NAME]` / `report [NAME]` | (M0) stop it and print `journey: NAME presses=P ticks=T depth=D screens=S typed=C`; with `--budgets FILE` fail when the journey's row is missing or exceeded |
| `expect-presses NAME [<=] N` | (M0) fail unless journey NAME took at most N presses |
| `problem WHO KIND [ITEM]` | (M0) raise a problem for the citizen named WHO (`hungry dirty bored sick item fight lonely` or a `PROB_*` number; ITEM by name, e.g. `Flowers`) |
| `item ITEM [N]` / `expect-item ITEM N` | (M0) add N (may be negative) of ITEM to the bag / fail unless it holds exactly N |
| `expect-problem WHO [0\|1]` | (M0) fail unless WHO has (1) / has no (0) open problem |
| `expect-rel WHO OTHER [0\|1]` | (M0) fail unless the two know each other (1) / have never met (0) |
| `expect-money MIN` | (M0) fail unless the purse holds at least MIN H$ |
| `first-boot` | (CL2) play a new console's first boot: from here the runner keeps a machine config record in memory (`PLATF_CONFIG`, as GBA / PS1 do), the record is wiped, the settings reload and `BOOT` starts, so the language picker, the Mature question and the security code come up in order (`core/tests/scripts/first_boot.txt`). Without it the runner has no record, as before |
| `expect-hero NAME` | (CL2) fail unless the protagonist (`town_res(DOOMO_PLAYER_ID)`) is called NAME |
| `expect-toast "TEXT"` / `expect-no-toast "TEXT"` | (CL1) fail unless a queued toast (or the one on screen) says TEXT / if one still does |
| `toasts [reset]` | (CL1) print the toasts shown so far per kind (`unlock hint feedback system`) and the unlock news still gathering; `reset` starts counting again |
| `expect-toasts KIND MAX` | (CL1) fail if more than MAX toasts of KIND (`unlock hint feedback system all`) were shown |
| `hud-watch on\|off` | (CL1) from `on`, check every frame on which the story overworld drew its HUD band and a toast that the toast lies under the band, off the objective pill and the clock; `off` prints the tally and fails if any frame was covered |

Buttons: `UP DOWN LEFT RIGHT A B L R START SELECT X Y L2 R2`.

`core/tests/run_scripts.sh` plays every `core/tests/scripts/*.txt` at both
240x160 and 320x240 (screenshots in `$(BUILD)/shots/<w>x<h>/<script>/`).
A script whose first line is `# assert-no-stubs` also gets that flag.
Look at the PNGs — every team verifies its screens visually.

`core/tests/run_journeys.sh` (`make -C core journeys`, part of `test`)
plays every `core/tests/journeys/*.txt` the same way with
`--budgets core/tests/journeys/budgets.tsv`, prints one table row per
journey (presses, ticks, depth, screens, typed, the budget) and fails on a
journey over budget, a press count that differs between 240x160 and
320x240, a journey without a budget row or a row without a journey, and
when the meter itself lets j1 through with its row lowered by one press.
See docs/GAME_DESIGN.md "UX budgets".

## plat_view3d: the height raycaster (reference semantics)

The GBA renderer implements the same rules in optimised form; the PS1
draws the same maps as GTE polygons.

* One ray per screen column, DDA through the cell grid front to back,
  with a per-column open window `[ytop, ybot]` that shrinks as geometry is
  drawn. `pitch` shears the horizon (`pitch * focal * 0.84 / 256` rows).
  FOV follows `OPT_FOV` (tan(fov/2) = 0.55 / 0.66 / 0.79).
* Every open cell draws its textured floor, and its ceiling unless
  `ceil_tex == DOOMO_TEX_SKY`, between the distances where the ray
  enters and leaves it (perspective-correct per pixel).
* Entering a cell with a higher floor draws a **lower step face** textured
  with the new cell's `floor_tex`; a lower ceiling draws an **upper face**
  with its `ceil_tex`. Faces are one light level darker.
* A solid cell (`wall != 0`) draws a wall face textured `wall`. Under a
  roof it closes the column. Under open sky the block is as tall as its
  `ceil_h` (if above its `floor_h`, else 256 units), and the ray continues
  across its roof (texture `floor_tex`, else `wall`), so taller buildings
  behind it stay visible. Cells outside the map are solid (256 units
  under the sky, with the sky above them: the ray ends there).
* A roofed open cell entered from under the sky draws its `ceil_tex` as a
  face from its ceiling up to its roof - the tallest `ceil_h` of its solid
  4-neighbours (if above their `floor_h`), else 256 units above the floor
  it is seen from - and the sky above the roof (as the GBA renderer).
* Light: the cell's `light` (0 = map ambient) minus 1 level per 128 world
  units of distance (minimum 1), x-side faces 1 darker, through
  `doomo_colormap`. Texture `v` is anchored to world height (walls line up),
  floors use world x/y modulo 64.
* Whatever stays open at the end shows the sky (`sky_pixels`, 256 columns
  per full turn) under open sky, or the darkest stone colour indoors.
* Billboards are sorted far to near and drawn column by column, clipped
  against the window that was open at **their depth** (so a low wall hides
  only a demon's feet). `scale` 256 = 1 texel per world unit; `z` is the
  feet height. `BB_FULLBRIGHT` ignores light, `BB_TRANSLUCENT` (and the
  third-person `player_sprite` when closer than 40 units) is drawn as a
  50% checker, `BB_SHADOW` darkens the floor under the feet. Sprites
  nearer than 4 units are skipped, so the camera never clips into the
  player's body.
* `tint` / `tint_level` push pixels through `doomo_tintmap` with a 4x4
  ordered dither (level/16 of the pixels).

## plat_overworld: the top-down town

* A cell is a 16x16 tile (4 world units per pixel); `cx, cy` is the
  viewport centre.
* Open cells sample their `floor_tex` down to 16x16. Pits (`floor_h < 0`)
  are darker; raised floors are lifted blocks.
* Solid cells are raised blocks: a darker front face (6–14 px, from
  `ceil_h - floor_h`) and a top face (`floor_tex` if set, else the wall
  texture), with a dark contact line at the base.
* Billboards are feet-anchored and drawn 1 texel = 1 pixel at scale 256
  (2x at 512 ...), lifted by `z / 4`, interleaved with the blocks by their
  feet row (y-sorted). `BB_SHADOW` draws a soft blob under the feet.
* `daylight` 0..15 maps to colormap level 5..15; cells with
  `light >= 13` glow at night; below daylight 8 a night tint is dithered
  in; then `tint` is applied like in 3D.
