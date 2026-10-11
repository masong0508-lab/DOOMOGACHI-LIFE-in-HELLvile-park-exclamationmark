# DOOMOGACHI LIFE on the Game Boy Advance

The `gba/` module is a real GBA ROM running the shared C99 core
(`core/`). It implements every function of `core/include/doomo/platform.h`
on bare metal: no devkitPro, no libgba, no BIOS calls besides
`IntrWait`/`Halt`. This document covers building, running, the hardware
mapping, the memory map, the renderers, saves, audio, the test harness and
the performance work.

Quick start:

```sh
make -C gba                 # gba/build/doomogachi.gba      the game
make -C gba engine-test     # gba/build/doomogachi_et.gba   3D showcase tour
make -C gba test            # both ROMs + headless mGBA smoke tests
mgba-qt gba/build/doomogachi.gba
```

---------------------------------------------------------------------------

## 1. Building

Requirements (Debian/Ubuntu package names):

| what | package | tested |
|------|---------|--------|
| cross compiler + newlib | `gcc-arm-none-eabi`, `libnewlib-arm-none-eabi` | GCC 13.2.1 |
| ROM tools | `python3` (stdlib only) | 3.11+ |
| test harness (optional) | host `gcc`, `libmgba-dev` | mGBA 0.10.2 |
| playing | `mgba-qt` or `mgba-sdl` | 0.10.2 |

Make targets (run from anywhere with `make -C gba ...`):

| target | output | notes |
|--------|--------|-------|
| `all` (default), `rom` | `build/doomogachi.gba` | the game |
| `engine-test` | `build/doomogachi_et.gba` | same as `make ENGINE_TEST=1` |
| `engine-test-nocore` | `build/doomogachi_et_nocore.gba` | renderer only, procedural stand-in art (`NOCORE=1`) |
| `harness` | `build/harness` | host program linked against libmgba |
| `test` | - | `test-et`, `test-save`, `test-game` (below) |
| `sizes` | - | `arm-none-eabi-size -A` of the game ELF |
| `clean` | - | removes `gba/build/` |

How it is put together:

* `core/sources.mk` lists the core (`CORE_SRCS`); every core object is
  linked directly (never through an archive), after the GBA objects, so
  the platform's strong definitions override the core's weak stubs.
* `*.c` compile as THUMB `-O2` and run from ROM; `*.iwram.c` compile as
  ARM and are copied to IWRAM by `crt0.s`; `*.s` hand-written ARM lives in
  the `.iwram` section. GBA sources build with `-Werror`; core sources do
  not (their warnings are the core's business).
* `-mcpu=arm7tdmi -ffunction-sections -fdata-sections -Wl,--gc-sections`.
* `tools/mktables.py` generates `build/gen/gba_tables.c` (1/x table for
  the raycaster, sine table, ...).
* The ELF is converted with `objcopy -O binary`, then `tools/gbafix.py`
  fills the header (Nintendo logo, title `DOOMOGACHI`, game code `BDME`,
  maker `DM`, complement check) and pads the ROM to a power of two.
  `tools/memreport.py` prints the RAM budget after every link.

---------------------------------------------------------------------------

## 2. Running

### mGBA (recommended)

```sh
mgba-qt gba/build/doomogachi.gba        # or: mgba gba/build/doomogachi.gba
```

* **Save type**: the ROM carries the `FLASH1M_V103` tag and talks to a
  128 KB flash chip. mGBA detects it automatically (the ROM switches flash
  banks during boot, which makes mGBA's auto-detection pick *Flash 1Mb*).
  If a stale `.sav` of another size exists, delete it or set
  *Tools > Game overrides > Save type: Flash 1Mb*. The save file is
  `doomogachi.sav` next to the ROM.
* **Debug log**: `plat_log()` writes to mGBA's debug-print registers
  (`0x04FFF600`, enabled via `0x04FFF780`). In mGBA-Qt open
  *Tools > View logs* and tick *Debug*/*Info* for the *GBA Debug* category.
  The engine-test ROM logs its frame rate there every 2 seconds.
* Any BIOS works; the ROM never relies on BIOS data (only `IntrWait`).

### Real hardware / flash carts

The header is valid (logo + complement), the ROM uses WAITCNT `0x4317`
(ROM 3/1 wait states, prefetch on), which every cartridge supports. The
save routines run from IWRAM and follow the Macronix / Sanyo / Panasonic /
SST / Atmel command sets; flash carts must be set to "FLASH 128K" (most
detect it from the `FLASH1M_V103` tag).

### Controls

| GBA | core button | |
|-----|-------------|---|
| D-pad | `BTN_UP/DOWN/LEFT/RIGHT` | |
| A / B | `BTN_A` / `BTN_B` | |
| L / R | `BTN_L` / `BTN_R` | |
| START / SELECT | `BTN_START` / `BTN_SELECT` | |
| SELECT + A | `BTN_X` | synthesised by the core (`DOOMO_INPUT_SYNTH_XY`) |
| SELECT + B | `BTN_Y` | synthesised by the core |
| - | `BTN_L2` / `BTN_R2` | not available on GBA |

`KEYINPUT` is read once per logic tick (`gba_read_pad()`).

Engine-test ROM: the tour runs by itself. Any key takes manual control:
D-pad walk/turn, L/R strafe, hold A + UP/DOWN to look up/down, B jump,
SELECT toggles first/third person, START resumes the tour. Holding SELECT
while the ROM boots runs the flash save self-test first.

---------------------------------------------------------------------------

## 3. Hardware usage and memory map

| region | address | size | use |
|--------|---------|------|-----|
| ROM | `0x08000000` | 762 KB game / 794 KB ET (padded to 1 MB) | THUMB code, art, tables; WS0 3/1 + prefetch |
| EWRAM | `0x02000000` | 256 KB | 16 B harness mailbox, `.data`, `.bss` (core state, `gba_big`) |
| IWRAM | `0x03000000` | 32 KB | ARM hot code + data (`.iwram`), `.iwram_bss`, stacks |
| IRQ stack | `0x03007FA0` down | 672 B | the master IRQ handler (`crt0.s`) |
| main stack | `0x03007D00` down | >= 6 KB reserved (linker `ASSERT`) | system mode |
| VRAM page 0/1 | `0x06000000` / `0x0600A000` | 2 x 37.5 KB | Mode 4 double buffer |
| VRAM gaps | `0x06009600` / `0x06013600` | 2 x 2.5 KB | the 3D view's 16 colormap rows + tint map (behind the two Mode 4 pages; byte reads as fast as IWRAM) |
| VRAM OBJ area | `0x06014000` | 16 KB | 3D texture cache: 4 textures (unused by the display: no hardware sprites) |
| palette | `0x05000000` | 512 B | BG + OBJ palettes = `doomo_palette` |
| flash | `0x0E000000` | 128 KB in 2 banks | saves |

Display: Mode 4 (240x160, 8 bpp) with two pages flipped in vblank.
Interrupts: VBlank only (frame counter + audio mixer). Timer 0 drives the
DirectSound FIFO, DMA 1 feeds it, timers 2+3 cascade into a free-running
32-bit cycle counter (`gba_cycles()`, used for profiling). DMA 3 does block
copies/fills.

The `.mailbox` section (first 16 bytes of EWRAM) holds
`{ "DMGB", logic ticks, drawn frames, fps*10 }`; the test harness reads it
to count logic ticks and frame rates.

### RAM usage (measured, `tools/memreport.py`)

Every build prints this report; the linker script `ASSERT`s the limits.
Game ROM (`build/doomogachi.gba`) with the foundation, art, roster,
portrait, life (sim / exorcism / pet) and action (map / actors / combat /
weld) modules linked:

```
ROM   1,436,940 bytes
IWRAM    24,852 / 32,768  (code+data 20,984, bss 3,868)  7.0 KB left for the stack
EWRAM   246,536 / 262,144 (.data 256, .bss 246,248)      15.2 KB free
  109,524  doomo_town      core (town.o)
   32,780  doomo_map       core (map.o, the live map)
   18,432  pool            core (portrait.o)
   17,408  gba_big         platform: renderer caches / save window (bigbuf.c)
   12,288  view_pix        core (weld_gfx.o)
    7,084  sim_s           core (sim.o)
    6,144  doomo_actors    core (actor.o)
    4,480  pcv             core (portrait_gfx.o)
    4,096  img_pix         core (pet_gfx.o)
    3,040  dq              core (ui_dialog.o)
    2,688  spr_img         core (sprites.o)
    2,560  sc_pool         core (ui_raster.o: the UI shape cache)
```

How EWRAM got here:

| state | EWRAM free | what changed |
|-------|-----------:|--------------|
| foundation + platforms | 19.4 KB | `gba_big` was the 88 KB save scratch (`DOOMO_SAVE_SCRATCH_MIN`) |
| streaming saves (perf pass) | ~90 KB | `gba_big` 88 KB -> 17 KB: saves stream through it (section 8) |
| life + action modules landed | 0.3 KB | live map 32.8 KB, weld view 14.8, sim 7.1, actors 6.8, pet 5.3, combat 4.9, portrait growth, ... |
| save codec without static buffers | 7.0 KB | `save.o` 7,022 -> 196 bytes: its LZ window / match heads now live in the scratch (ARCHITECTURE.md) |
| UI rasteriser tables `const` | 8.1 KB | corner / circle / band tables of `ui_raster.c` moved to ROM (1.1 KB) |
| colormap / tint map in ROM | 13.6 KB | `doomo_colormap` / `doomo_tintmap` are pointers to the generated `const` tables (palette.h); only the no-art fallback builds RAM copies (5.6 KB) |
| smaller UI shape cache | 15.2 KB | `sc_pool` 1,024 -> 640 rects, 48 -> 40 entries (1.7 KB); the UI gallery / dialog fps are unchanged |

The engine-test ROM (no game scenes): ROM 867,272 bytes, IWRAM 21,032
(10.7 KB for the stack), EWRAM 173,128 (86.9 KB free).

Modules landing after the perf pass (creator, studio song book, story
overworld, free play, library, minigames, concert) add about 43 KB of
`.bss` to the game ROM, which no longer fits as it stands (the link
fails with EWRAM over by ~27 KB at the time of writing). Most of that is
per-scene state that is dead while another scene runs: put it in the
shared `scene_scratch()` block (scene.h, 12 KB, ARCHITECTURE.md
"Memory and performance") or in `gba_big`, rather than in a static
each.

Measured main-stack high-water marks (the boot code paints the free IWRAM
and `gba_stack_used()` scans it): 660 bytes in the game's boot / title /
UI gallery (1,568 with the span rasteriser's row buffers), 936 - 1,024
bytes in the 3D tour and overworld. The game ROM logs
`GBA: main stack high-water N bytes` whenever the mark grows by 256 bytes.

`gba_big` (17 KB, `src/bigbuf.c`) is one shared EWRAM buffer; whoever
uses it claims it first (`gba_big_claim()`), which invalidates the
previous owner's cache (it is rebuilt in a frame):

| offset | user |
|--------|------|
| 0 - 17 KB | overworld lit-tile cache (64 tiles of 16x16) |
| 0 - 8 KB | 3D: per-column clip checkpoints for sprites |
| 8 - 12 KB | 3D: roof heights per map cell |
| 12 - 13 KB | 3D: sprite sort order |
| whole | `plat_save_scratch()`: the core's save window (2.75 KB) + its codec work area (`DOOMO_SAVE_WORK_SIZE`, 14.25 KB); less than a slot, so the core streams (section 8) |

---------------------------------------------------------------------------

## 4. The 2D layer

Mode 4 VRAM ignores byte writes (a byte store writes the byte into both
halves of the halfword), so every primitive writes halfwords or words and
does a read-modify-write for odd pixels through `volatile` pointers (GCC
would otherwise turn the read-modify-write into a byte store).

* `plat_clear`, full-width `plat_rect`: DMA 3 fills.
* `plat_rect`: ARM (`video_loops.s`): odd ends read-modify-write, then
  halfword alignment, 16-byte `stm` bursts and words. The core's UI draws
  thousands of small spans per frame, so the per-row cost is kept low.
* `plat_text`: the 5x8 core font is transposed at boot into IWRAM; each
  glyph row is written with at most two aligned 32-bit read-modify-writes.
* `plat_image`: 8bpp, index 0 transparent, integer scale 1..4, flip,
  silhouette and drop-shadow; opaque pixel pairs are written without
  reading.
* `plat_shade_rect`: through `doomo_colormap[level]` (copied to the IWRAM
  stack for areas of 256 pixels or more).
* `plat_checker`: 50% checkerboard rows (the UI's soft band seams) as
  word masks, one call instead of one `plat_rect` per pixel.
* `plat_line`: Bresenham, horizontal / vertical runs use the rect path.
* Colour index 0 is "transparent" in images; as a fill colour it is
  replaced by `PAL_BLACK`.

All primitives clip against the screen and do nothing during the skipped
catch-up ticks (`gba_nodraw`, see pacing).

### Frame pacing

The core logic runs at 30 Hz (`plat_begin_frame` / `plat_end_frame`, one
`doomo_frame()` per tick); a drawn frame is presented every second vblank
at best. When a frame takes longer, the next tick runs its logic without
drawing (`gba_nodraw` = 1) to catch up, but never two in a row, so the
display never drops below 15 fps and the game speed stays exact. After
loading or saving (`gba_pacing_reset()`) the debt is forgotten.
`plat_ticks()` counts logic ticks, `plat_entropy()` mixes the cycle
counter, vblank count, scanline and keys.

---------------------------------------------------------------------------

## 5. `plat_view3d`: the height raycaster

Files: `src/r3d.c` (THUMB setup), `src/r3d.iwram.c` (ARM column pass,
sprites), `src/r3d_loops.s` (hand-scheduled ARM: column walk + boundary
handling, flat/face/sky/sprite loops), `src/r3d_int.h`.

Semantics follow the native reference renderer (`core/native/raycast.c`,
`core/native/README.md`): field of view from `OPT_FOV` (tan 0.55 / 0.66 /
0.79), look up/down by y-shearing (`pitch * focal * 0.84 / 256` rows), eye
height and view bob, per-cell floor/ceiling heights with step faces one
light level darker, textured floors and ceilings, blocks with an explicit
top under the sky that the ray walks across (walls without a top are 256
units tall under the sky), roofed cells that close the column, lintels
over doorways (roof = tallest neighbouring block), sky panning with the
view angle, per-cell light (0 = ambient) minus one level per 128 units of
distance (minimum 1), y-side walls one level darker, animated textures
(`texture_pixels(id, time)`), doors and lifts via `map_floor_z` /
`map_ceil_z`, up to 48 cells (`R3D_MAXDIST` 40 on GBA, the maps are small)
then sky or dark fog.

How it works:

* **120 two-pixel columns.** One ray per pair of screen pixels; every
  pixel is one VRAM halfword with both bytes equal, so no read-modify-write
  is ever needed. The viewport is respected (x / width rounded to even).
* **Front to back with a clip window** `[ytop, ybot)` per column, Doom
  style. `r3d_column_asm` walks the DDA through identical cells in a tight
  loop (raw 8-byte cell compare) and handles each boundary: closes the
  floor and/or ceiling run when height/texture/light change, draws risers
  and lowered-ceiling faces, block sides, wall faces, sky. Door/lift cells
  and "roofed cell seen from under the sky" fall back to the C reference
  `boundary()` in `r3d.iwram.c`.
* **Floors and ceilings are cast down the column**: per row
  `u = u0 + K*A`, `v = v0 + K*B` with `K = proj/(m + 0.5)` from a table,
  26 fractional bits so the top 6 bits are the texel. u/v are exact every
  4th row and linear in between; the distance light is refreshed every 16
  rows; rows beyond 1400 units use the texture's average colour (no
  shimmer, and cheap).
* **Faces**: one texture column, v stepped in 16.16, anchored in world z
  so steps and blocks line up.
* **Sky**: `sky_pixels(sky, time)` panned with the angle, 88 texture rows
  over 3/4 of the view height above the horizon (sheared with pitch).
* **Sprites** (billboards) are z-sorted far to near and clipped per
  column against checkpoints the column pass recorded (window + distance),
  so they disappear correctly behind walls, steps and lintels. Feet
  anchored, `scale`, `z` lift, `BB_FLIPX`, `BB_FULLBRIGHT`,
  `BB_TRANSLUCENT` (checkerboard alternating per frame), `BB_SHADOW`
  (darkened ellipse on the floor), per-sprite light (cell light, distance
  fade, `b->light`), view tint. Billboards closer than 4 units are skipped;
  the third-person player sprite (`player_sprite`) becomes translucent
  under 40 units, so it never blocks the camera.
* **Colour**: 16 light levels of `doomo_colormap` (copied into the VRAM
  gaps behind the Mode 4 pages, as fast as IWRAM for byte reads). The
  view tint (`tint` / `tint_level`) is a pass after the sprites: level/16
  of the pixels go through `doomo_tintmap[tint]` in a 4x4 ordered dither
  (on the 2-pixel columns), like the reference. It costs about 1 ms per
  tint level at full screen (6 ms at level 6), so it suits flashes and
  light haze.

### Performance

Everything per pixel is ARM code in IWRAM. Notable tricks:

* hand-written loops with loads scheduled so no `ldrb` result is used by
  the next instruction; walk + boundary in one assembly routine;
* **VRAM texture cache**: texels from the cartridge cost 6 cycles, from
  VRAM 3. The four textures that covered the most pixels in the previous
  frame are kept in the unused OBJ VRAM (16 KB); animated ones are
  re-copied when their frame changes;
* floor/ceiling u,v interpolated over 4 rows, light every 16 rows;
* integer division (`__aeabi_idiv`, `__aeabi_uidiv`, `__aeabi_ldivmod`
  fast path) and `memcpy`/`memset` replaced by IWRAM ARM versions
  (`src/div.s`, `src/mem.s`), checked at engine-test boot
  (`ET divtest ok`);
* 64-bit multiplies in the THUMB setup code go through an ARM `smull`
  helper instead of libgcc's `__muldi3`;
* (perf pass) the wall and floor / ceiling pixel loops are unrolled 8
  times with a computed entry (no per-pixel count test), the colormap's
  16 light rows and the tint map sit in the two VRAM gaps behind the Mode
  4 pages (byte reads as fast as IWRAM; it was a 4 KB IWRAM copy), and a
  sprite clip checkpoint is read as one word. The output is bit-identical
  (frame CRCs of the whole tour compared): about 10% less time per frame.

Measured in mGBA 0.10.2 with `make -C gba test-et` (full screen
240x160, `r3d` = time inside `plat_view3d`, `fps` = drawn frames per
second including the HUD and the tour logic):

| phase | fps | r3d avg | before the perf pass | notes |
|-------|-----|---------|----------------------|-------|
| courtyard (outdoor, sky, hedges, house, tower) | 19.4 | 43.9 ms | 17.9 fps, 49.1 ms | |
| path | 19.9 - 20.2 | 38.9 - 42.9 ms | 18.5 - 19.7, 43.2 - 48.2 ms | sprites 4 - 7 ms |
| hall door / hall (stairs, pillars) | 22.6 - 27.8 | 29.3 - 34.8 ms | 20.2 - 26.1, 32.7 - 38.2 ms | |
| skylight (look up 220) | 25.1 | 34.5 ms | 22.2, 37.6 ms | |
| stairs (step faces) | 19.9 | 41.9 ms | 18.4, 47.2 ms | |
| mezzanine (look down) | 24.4 | 33.4 ms | 22.0, 36.9 ms | |
| third person | 19.2 - 20.3 | 39.0 - 44.6 ms | 17.6 - 18.7, 43.0 - 49.4 ms | player + prop sprites 9 - 10 ms |
| low corridor | 19.9 | 39.8 ms | 19.0, 43.8 ms | |
| hell room (dark, demons, red tint level 4) | 21.7 | 36.3 ms | 20.7, 40.1 ms | tint pass about 4 ms |
| hell third person | 20.2 - 29.3 | 25.4 - 42.2 ms | 17.2 - 29.1, 27.9 - 46.9 ms | |
| overworld (`plat_overworld`) | 21.7 | 36.2 ms | 21.5, 37.1 ms | |

Frames are presented at a vblank: a whole frame (render + HUD + logic)
under 33 ms shows at 30 fps, under 50 ms at 20 fps, longer at 15 fps
(the pacing never drops below that, the logic stays at 30 Hz). The
heaviest showcase scenes (courtyard, the third-person views, stairs,
corridor) now sit right at the 50 ms line: most frames show at 20 fps,
the occasional slow one at 15 (19.2 - 19.9 average); the rest run at 20 -
29 fps.
Use the harness profiler (`profile N` with `--nm`) to see where the
cycles go.

2D UI: the core's UI kit used to rasterise its shapes per pixel in THUMB
code (the boot heart's coverage mask, `ui_star`, `ui_backdrop`, rounded
windows), with divisions per pixel and thousands of tiny `plat_rect`
calls; the boot animation drew at about 3 fps and the 30 Hz logic slowed
to 6 - 10 ticks per second. The perf pass rewrote it as span rasterising
(`core/src/ui_raster.c`: integer scanline maths, runs merged across rows,
a cache of the rect lists of shapes redrawn every frame, `plat_checker`
for dithered seams) with pixel-identical output (every frame of the
native runner's boot / UI gallery / saves / playthrough scripts compared
at 240x160 and 320x240: 0 differing pixels; `plat_rect` calls per frame
2,180 -> 524 on average). Harness `fps` command on the game ROM
(`logic` = ticks per second, `drawn` = presented frames per second):

| screen | before: logic / drawn | after: logic / drawn |
|--------|----------------------:|---------------------:|
| boot heart drop | 6.3 / 3.2 | 29.3 / 29.3 |
| boot name | 5.6 / 2.8 | 29.8 / 29.4 |
| boot wipe | 4.7 / 2.8 | 28.6 / 25.9 |
| title (stub) | 10.1 / 5.5 | 30.0 / 30.0 |
| UI gallery 1 windows | 10.3 / 5.9 | 30.0 / 30.0 |
| UI gallery 2 meters | 11.5 / 6.8 | 30.0 / 30.0 |
| UI gallery 3 list | 12.6 / 8.3 | 30.0 / 30.0 |
| UI gallery 4 paint | 12.5 / 7.6 | 30.0 / 29.5 |
| UI gallery 5 options | 12.8 / 7.6 | 30.0 / 30.0 |
| UI gallery 6 font | 12.5 / 7.6 | 30.0 / 30.0 |
| UI gallery 7 modals | 14.1 / 9.0 | 30.2 / 30.2 |
| confirm modal | 10.6 / 6.4 | 29.5 / 26.8 |
| keyboard (text entry) | 7.9 / 5.1 | 29.8 / 22.3 |
| dialogue (typing) | 10.1 / 5.7 | 30.2 / 24.0 |
| dialogue choice | 11.5 / 7.3 | 30.2 / 28.6 |
| UI gallery 8 backdrops | 10.9 / 6.0 | 30.5 / 25.7 |
| UI gallery 8 hell backdrop | 11.4 / 7.0 | 30.0 / 29.2 |

---------------------------------------------------------------------------

## 6. `plat_overworld`

Files: `src/overworld.c` (THUMB), `src/ow.iwram.c` + `src/ow_loops.s`
(ARM pixel loops). Semantics follow `core/native/overworld.c`: one cell =
16x16 pixels (4 world units per pixel), camera focus at the viewport
centre, floors point-sampled from the 64x64 textures, pits darker, raised
floors lifted, solid cells as blocks with a darker front face (6..14 px
from `ceil_h - floor_h`, else 10) and a lit top lifted by that height with
a dark contact line, billboards feet-anchored and y-sorted between the
block rows with drop shadows, daylight 0..15 -> colormap level 5..15
(lamps glow at night), a dithered night tint, then the view tint.

Speed: ground tiles are cached *lit* (per texture frame, light level,
front height and tint state) in a 2-way, 32-set cache in `gba_big`, so a
frame of ground is mostly 16-pixel block copies; horizontal scrolling
moves in 2-pixel steps so every copy is halfword aligned. Cells covered by
a block top + front face skip their ground tile.

---------------------------------------------------------------------------

## 7. Audio

* `plat_tone(ch, hz, vol, wave)` uses the PSG: channels 0/1 are the
  square channels (duty 12.5 / 25 / 50 % from `WAVE_PULSE*`, `WAVE_SAW`
  maps to 25 %), channel 2 the wave channel (32-step triangle / saw /
  square tables, double-buffered wave RAM banks, volume steps
  100/75/50/25 %), channel 3 noise (`WAVE_NOISE` = 15-bit LFSR, other
  waves the 7-bit periodic mode). A channel is only restarted when its
  volume or waveform changes, so sliding pitches do not click.
* `plat_sample(sfx, vol)`: the PCM from `audio_sfx_pcm()` is mixed by
  software into DirectSound A at 10512 Hz (timer 0 reload 65536 - 1596,
  176 samples per frame). 4 voices with 20.12 fixed-point resampling from the
  sample's own rate; the oldest voice is stolen when all are busy. DMA 1
  streams a 2 x 176-byte ring into FIFO A and is restarted at its start
  every other vblank; each VBlank IRQ mixes the half that just finished
  (`irq.iwram.c`).
* `SOUNDCNT`: PSG and DirectSound at full volume on both sides, bias
  0x200.

The engine-test ROM plays DirectSound samples on ticks 1-8 and all four
PSG channels on ticks 10-18; `enginetest.txt` checks both windows with
`expect-audio`.

---------------------------------------------------------------------------

## 8. Saves (128 KB flash)

`src/save_flash.c` (logic, THUMB) and `src/flash.iwram.c` (chip access,
ARM in IWRAM, interrupts off while the chip is in command mode).

* Detection: switch bank 1 then bank 0 (makes emulators upgrade to the
  128 KB chip), enter ID mode (`AA/55/90`), read manufacturer/device:
  `09C2` Macronix MX29L010 (128 KB), `1362` Sanyo LE26FV10N1TS (128 KB),
  `1CC2` Macronix MX29L512, `1B32` Panasonic MN63F805MNP, `D4BF` SST
  39VF512, `3D1F` Atmel AT29LV512 (64 KB chips; Atmel uses 128-byte page
  programming).
* Layout: slot 0 (story, <= 40 KB) at `0x00000`, slot 1 (free play,
  <= 88 KB) at `0x0A000` (crosses the bank boundary at 64 KB; banks are
  switched with command `B0`). Each slot ends with a 12-byte trailer
  `"DMSV"`, size, ~size, written last.
* Write: erase the trailer's sector first (an interrupted write reads as
  "no save"), erase only the 4 KB sectors that are not already blank,
  program byte by byte (skipping `0xFF`), verify every sector, then write
  the trailer. Sector erase and byte program poll for completion with a
  timeout; Macronix chips get the extra reset command after a timeout.
* `plat_save_read` copies `min(size, cap)` bytes and reports the copied
  size; `plat_save_erase` erases the trailer sector (the slot reads as
  empty).
* **Streaming** (platform.h): `plat_save_scratch()` hands the core the
  17 KB `gba_big` (claiming it from the renderer caches), less than a
  slot, so no RAM copy of a save ever exists. `save_game()` sizes and
  checksums the save in a first encode pass, then streams the same bytes:
  `plat_save_stream_begin(slot, size)` erases the trailer sector,
  `plat_save_stream_write()` erases each 4 KB sector when the stream
  enters it and programs (and verifies) the bytes as they arrive, and
  `plat_save_stream_end(1)` writes the trailer - the same steps and the
  same bytes on the chip as `plat_save_write`. Loads read through
  `plat_save_read_at()` (random access, so a window refill costs only the
  flash read). `make -C gba test-coresave` saves and reloads the codec's
  worst case (1000 random custom citizens, 58.6 KB, slot 1 across the
  bank boundary) and a story island, across two harness runs.

---------------------------------------------------------------------------

## 9. Test harness (`gba/harness`)

`build/harness` is a headless mGBA (libmgba 0.10) runner:

```sh
gba/build/harness ROM.gba [--script FILE] [--shots DIR] [--save FILE]
                  [--frames N] [--scale S] [--fail-on-stub] [--quiet]
                  [--nm NMFILE]
```

* Prints the ROM's debug log (`plat_log`) to stdout.
* Script format = the core native runner's: `wait N` (logic ticks, read
  from the mailbox, so scripts behave the same however fast the ROM
  renders), `press BTN..`, `hold BTN.. N`, `shot NAME`, `log TEXT`,
  `expect-log TEXT`, `expect-scene NAME`, `type` (warning only), plus GBA
  extensions `down`/`release`, `frames N` (raw video frames), `fps
  [LABEL]`, `audio-reset` / `audio` / `expect-audio MIN`, `profile N
  [FUNC]`.
* `--save FILE` backs the 128 KB flash with FILE (created if missing),
  like a frontend's `.sav`, so persistence can be tested across runs.
* `profile N [FUNC]` steps N instructions and charges each one's emulated
  cycles (wait states, halts) to its function (needs `--nm` with
  `arm-none-eabi-nm -n` output); with FUNC it also lists that function's
  hottest addresses.
* Fails (exit 1) on mGBA fatal/illegal-instruction errors, a hang (no
  logic tick for 20 s of emulated time; 60 s at boot), failed
  expectations, and with `--fail-on-stub` on `STUB:` log lines.
* Screenshots are PNG (own encoder, `png.c`), optionally scaled.

`make -C gba test`:

| target | what |
|--------|------|
| `test-et` | engine-test ROM, `scripts/enginetest.txt`: boot, division self-test, DirectSound + PSG audio, 16 screenshots (courtyard, blocks, path/tower, hall door, stairs, skylight look-up, step faces, mezzanine look-down, third person x2, low corridor, hell room sprites, hell third person x2, overworld x2), frame-rate log lines |
| `test-save` | engine-test ROM twice with the same `--save` file and SELECT held at boot: first run "no previous data / write ok", second run "persisted ok" (both 30 KB slots verified byte for byte, slot 1 erased and rewritten) |
| `test-game` | game ROM, `scripts/smoke.txt`: boot, title, START, menu presses, screenshots in `build/shots/game` |
| `test-coresave` | game ROM, `scripts/coresave1.txt` then `coresave2.txt` with the same `--save` file: the core's streamed saves (worst-case island in slot 1, a too-big save refused in slot 0, a story island in slot 0), loads checked by town CRC, then persistence across a reboot |

---------------------------------------------------------------------------

## 10. Engine-test ROM

`make -C gba engine-test` builds `ENGINE_TEST=1`: the ROM boots straight
into `enginetest_main()` (`gba/enginetest/`): a scripted first- and
third-person tour of a 32x24 showcase map (`et_map.c`: courtyard under
the sky with hedges, a house with a roof and door, a tower, crates, a
pond, a path; a hall with stairs to a mezzanine, a 4x4 skylight, candy
pillars, a lava pit, a lift, a door; a low tech corridor down into a dark
hell room with flesh pillars and a raised pentagram dais), props and
demons as billboards, then the overworld view of the same map, then the
core's Hell Breach maps if `map_load()` provides them. It logs
`ET phase: <name>` at each phase and `ET fps=.. r3d avg=..` every 2
seconds.

`NOCORE=1` links only the core's palette/font/fixmath plus procedural
stand-ins (`enginetest/standalone/`), for renderer work while the core is
in flux.

---------------------------------------------------------------------------

## 11. Conventions for the core

* Doors: an open cell with `CELL_DOOR` and the door texture as its
  `ceil_tex`. Its ceiling comes from `map_ceil_z()` (the core animates it:
  down to the floor = closed); seen from a neighbouring cell, the face
  between the door cell's ceiling and the neighbour's ceiling (or, under
  the sky, the doorway's roof = the tallest neighbouring block) is drawn
  with that texture, so a closed door fills the doorway and an open one
  leaves a lintel.
* Lifts: `CELL_LIFT`, height from `map_floor_z`; `tag` = top height (used
  for the "tallest thing on the map" early-out under the sky).
* Blocks: `wall != 0` with `ceil_h > floor_h` are blocks of that height
  under the sky (walls otherwise).
* Keep big core tables `const` (ROM); every byte of `.bss` is EWRAM and
  EWRAM is nearly full (see the RAM table). Transient buffers belong on
  the stack or in a shared buffer; saves need none (the codec works in
  `plat_save_scratch()`, extension chunks are staged there too).
* Draw UI shapes with the `ui_*` primitives (spans, cached rect lists),
  never pixel by pixel: every `plat_rect` is a THUMB -> ARM call.
