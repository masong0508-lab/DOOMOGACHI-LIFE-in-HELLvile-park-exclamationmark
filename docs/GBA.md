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
| `test` | - | `test-et`, `test-save`, `test-game`, ..., `test-ow` (section 9) |
| `rom TILES=0` | `build/doomogachi.gba` | the overworld in Mode 4 only (no tile mode, G5): the reference ROM for a tile-mode frame parity check |
| `sizes` | - | `arm-none-eabi-size -A` of the game ELF |
| `clean` | - | removes `gba/build/` |

How it is put together:

* `core/sources.mk` lists the core (`CORE_SRCS`); every core object is
  linked directly (never through an archive), after the GBA objects, so
  the platform's strong definitions override the core's weak stubs.
* `*.c` compile as THUMB `-O2` and run from ROM; `*.iwram.c` compile as
  ARM and are copied to IWRAM by `crt0.s`; `*.s` hand-written ARM lives in
  the `.iwram` section. Exception: the raycaster's ARM code
  (`r3d.iwram.c`, `r3d_loops.s` in section `.iwram_3d`) is a group of the
  IWRAM code overlay (section 3), loaded by `plat_view3d()`, not at boot. GBA sources build with `-Werror`; core sources do
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

Debug cutscene viewer (game ROM): hold SELECT while powering on and a list
of every story cutscene (including "What an Ass") replaces the normal boot.
UP/DOWN pick, L/R jump 5, A plays, START inside a cutscene skips back to
the list. Code: `gba/src/cutview.c`; `make CUTVIEW=0` compiles it out. The
PS1 build has the same viewer (`ps1/src/cutview.c`, hold SELECT at boot).

---------------------------------------------------------------------------

## 3. Hardware usage and memory map

| region | address | size | use |
|--------|---------|------|-----|
| ROM | `0x08000000` | 4,201,300 B game after H9 (padded to 8 MB; the furniture's iso sprites are 878 KB of it) / about 1.1 MB ET | THUMB code, art, tables; WS0 3/1 + prefetch |
| EWRAM | `0x02000000` | 256 KB | 80 B harness mailbox, `.data`, `.bss` (core state, the arena `gba_arena`) |
| IWRAM | `0x03000000` | 32 KB | ARM hot code + data (`.iwram`), the code overlay slot (G4: 3D or 2D group), `.iwram_bss`, stacks |
| IRQ stack | `0x03007FA0` down | 672 B | the master IRQ handler (`crt0.s`) |
| main stack | `0x03007D00` down | >= 6 KB reserved (linker `ASSERT`) | system mode |
| VRAM page 0/1 | `0x06000000` / `0x0600A000` | 2 x 37.5 KB | Mode 4 double buffer |
| VRAM gaps | `0x06009600` / `0x06013600` | 2 x 2.5 KB | the 3D view's 16 colormap rows + tint map (behind the two Mode 4 pages; byte reads as fast as IWRAM) |
| VRAM OBJ area | `0x06014000` | 16 KB | 3D texture cache: 4 textures; squash and title-figure OBJs (`obj.c`); in a tile frame, the overworld billboards as OBJs (G6, `ow_obj.c`, section 6c) |
| VRAM in a tile frame (G5) | `0x06000000` | 37.5 KB | canvas map (SB 0), world map (SB 1), 36 cell composites at `0x1000`, the 392-tile canvas pool at `0x3400`; the dense build canvas is page 1's bytes (`0x0600A000`, 600 tiles, the last 216 in OBJ VRAM below `0x06013600`). The 3D gaps are never touched (section 6b) |
| palette | `0x05000000` | 512 B | BG + OBJ palettes = `doomo_palette` |
| flash | `0x0E000000` | 128 KB in 2 banks | saves |

Display: Mode 4 (240x160, 8 bpp) with two pages flipped in vblank; an
overworld frame may instead show Mode 0 (a world BG and a canvas BG,
section 6b).
Interrupts: VBlank only (frame counter + audio mixer). Timer 0 drives the
DirectSound FIFO, DMA 1 feeds it, timers 2+3 cascade into a free-running
32-bit cycle counter (`gba_cycles()`, used for profiling). DMA 3 does block
copies/fills.

The `.mailbox` section (the first 80 bytes of EWRAM, `GbaMailbox` in
`gba_plat.h`) holds `{ "DMGB", logic ticks, drawn frames, fps*10 }`, the
harness command words (cmd, arg, result, cycles), the work counters of
`fps`, the 20-byte name argument, and (G1) `hold_at` and `draw_all` for
the harness's `sync on` and `vhash-seq` (section 9); the test harness reads
it to count logic ticks and frame rates and posts its commands there.

### RAM usage (measured, `tools/memreport.py`)

Every build prints this report; the linker script `ASSERT`s the limits.
Game ROM (`build/doomogachi.gba`) with every core module linked, after
H10 (the autopilot's BFS in the arena, below; measured 2026-11-01 on
`091aa2b` + H10, after G1). The shared ledger at the end of this section
has the figure every later package must start from.

```
ROM   3,669,044 bytes
IWRAM    24,832 / 32,768  (code+data 20,964, bss 3,868)  7,168 bytes left for the main stack
  since G4: resident code+data 10,580 + overlay slot 10,384 (3D group 10,384, 2D group 0)
  flash routines: 1,092 bytes, copied onto the stack while saving
EWRAM   248,528 / 262,144 (.data 544, .bss 247,904)     13,616 bytes free
  109,524  doomo_town      core (town.o)
   32,780  doomo_map       core (map.o, the live map)
   26,992  gba_arena       platform + core: the arena (bigbuf.c, below)
   18,432  pool            core (portrait.o)
    7,084  sim_s           core (sim.o)
    6,144  doomo_actors    core (actor.o)
    5,352  doomo_song_book core (song_book.o, saved)
    4,480  pcv             core (portrait_gfx.o)
    2,560  sc_pool         core (ui_raster.o: the UI shape cache)
    2,048  ul_by_id        core (unlocks.o)
    1,984  dq              core (ui_dialog.o)
    1,856  studio          core (studio.o)
```

| | before G0 (`9d4d915`) | after G0 |
|---|---:|---:|
| EWRAM free | 4,872 B | **9,952 B** (+5,080) |
| main stack (IWRAM) | 6,320 B | **7,412 B** (+1,092) |
| main stack high-water (harness) | 1,944 B | 1,944 B; 2,520 B during a save |
| fps (every scene of the G0 tour, below) | | unchanged |

**The arena** (`src/bigbuf.c`, `gba_plat.h`). Until G0 the core's scene
scratch (12 KB, `scene.h`), the platform's `gba_big` (17 KB: the overworld
tile cache, the raycaster's buffers, the house, the save window) and the
core's per-frame billboard list (2.4 KB) were three blocks, 32,096 B. They
are never all full at once, so on the GBA they are ONE block of 26,992 B
(`scene_scratch.c` hands out its bottom; the size is set by a save beside
the largest claim: 12,288 + the codec's 14,592 + a 112 B window):

| arena bytes | user |
|---|---|
| `[0, claim)` | the scene scratch: `scene_scratch(owner, size)`, size <= 12,288 |
| `[24,592 - region, 24,592)` | the platform owner that claimed it last (`gba_big_claim()`): the raycaster's 11,840 B (clip checkpoints 6,720, roof table 4,096, sprite order 1,024; it fits beside the largest claim, combat's 12 KB of weapon views; in an update the bottom 4,096 B of the clip checkpoints are the autopilot's BFS, H10 below), the overworld's tile cache 17,152 B (beside the story overworld's 6,472 B claim; beside a claim over 7,440 B it uses only its upper 16 sets), the house's 17,200 B (H8: +328 B for the lamp pools' table; beside its own 2,828 B, a claim up to 7,392 B) |
| `[24,592, 26,992)` | `doomo_frame_bb`: filled and read inside a draw |
| `[claim, 26,992)` | `plat_save_scratch()` (saves run in an update, never during a draw): 14,704 B beside the largest claim, 1 KB more than the codec needs beside the island's 11,312 B |

A platform owner's region is a cache: it is rebuilt when another owner or
a save took the memory, and (overworld, house) after a new scratch claim
(`scene_scratch_gen()`): the raycaster's region never overlaps a claim, so
a claim costs it nothing. The regions have symbols of their own
(`gba_r3d`, `gba_ow`, `gba_room`): with `gba_arena + offset` GCC kept the
arena base in a register and could not fold the offsets into THUMB loads
(the house's sort was a third slower), and the overworld's per-call state
is one struct (`OwLit`), so the half-cache flag costs no extra load.
Measured with the G0 fps tour (libmgba harness, 13 scenes + the house
script): every scene's work per frame is identical to `9d4d915` within
0.1 ms (combat walk 71.5 -> 71.6, house plain 9.9 -> 10.0 ms), screenshots
identical.

IWRAM: the flash chip routines (`flash_ram.c`, section 8) moved from IWRAM
to a copy on the stack while saving: +1,092 B for the main stack.

### IWRAM code overlay (G4, `src/iwram_ovl.c`)

IWRAM is code + data + `.iwram_bss` + a main stack of at least 6 KB
(`gba.ld` `ASSERT`), which left about 1 KB for new ARM loops. The
raycaster's ARM code is 10.4 KB but runs only inside `plat_view3d()`; the
tile-mode 2D loops (G5's overlay canvas) will run only in tile mode. So
they share ONE slot:

```
0x03000000  resident .iwram   10,580 B  crt0's IRQ handler, div.s, mem.s,
                                        video loops, ow loops, portrait,
                                        irq.iwram.c (mixer), video.iwram.c
0x03002954  overlay slot      10,384 B  ONE of:
              .ovl_3d  10,384 B  r3d.iwram.c code + const data, r3d_loops.s
              .ovl_2d   8,592 B  section .iwram_2d (G5's canvas loops, G6's
                                 shadow rows); its tail (1,792 B) holds
                                 ow_obj.c's tables while it is in
              .ovl_room 3,648 B  (H8) section .iwram_room: the dream
                                 house's lamp-pool rows and LUT copies,
                                 loaded by plat_room() (H9: 3,248 B)
0x030051e4  .iwram_bss         3,868 B  (r3d's .bss stays resident)
0x03006100  main stack         7,168 B  (>= 6,144 B with the largest group in)
```

Since then (ledger below): resident code+data 10,928 B after G5 (+228 B
routing) and G2 (+120 B `ow_spr_rows`), `.iwram_bss` 4,080 B, the 2D group
8,224 B: 6,608 B for the main stack with the 3D group in.

* `gba.ld`: an `OVERLAY ... NOCROSSREFS` of `.ovl_3d` and `.ovl_2d` right
  after `.iwram`: both linked at the same IWRAM address, stored one after
  the other in ROM (the 3D group at `__iwram_ovl_3d_lma`), the slot as big
  as the larger group. `crt0.s` copies only `.iwram`, so neither group
  is in IWRAM after boot. `NOCROSSREFS` makes any reference from one group
  into the other a link error. `.data` / `.bss` never go in a group
  (a swap would lose them): `r3f`, `r3d_texc`, the fade table and the other
  raycaster state stay resident in `.iwram_bss`.
* `iwram_ovl_use(OVL_3D | OVL_2D)` (`iwram_ovl.h`): a compare when the
  group is already in, else one DMA 3 copy from ROM: the 3D group takes
  13,121 cycles (0.78 ms), once per mode change. `plat_view3d()` calls it
  at its top (after the no-draw early return); G5's tile-mode entry calls
  it with `OVL_2D`. Rules: call it from each mode's entry point every time
  (so a 3D frame inside a tile-mode scene reloads 3D, and the next tile
  frame reloads 2D), never from an IRQ handler (it halts with a log line
  if it finds itself on the IRQ stack) and never from code inside the
  slot; IRQ handlers never call slot code (`irq.iwram.c` is resident).
  Its state is one byte of `.bss` (it fits the old `.bss` padding: EWRAM
  unchanged).
* `IWRAM_2D_CODE` (`iwram_ovl.h`) puts a single ARM function in the 2D
  group (like `IWRAM_CODE` for resident code); hand-written loops use
  `.section .iwram_2d, "ax", %progbits`.
* `tools/memreport.py` prints the slot, both groups and their free bytes,
  and fails the build if the main stack is under 6,144 B with the largest
  group in (`gba.ld` asserts the same).
* The harness profiler (`profile N` with `--nm`) maps a PC to the nearest
  symbol below it: once the 2D group has code, both groups' symbols share
  the slot's addresses, so read slot samples knowing which mode ran.
* Room for the 2D group: the whole slot (10,384 B) plus the 1,024 B the
  stack has over 6 KB = 11,408 B before the 6 KB rule breaks. A 2D group
  bigger than the 3D group grows the slot and takes the difference from
  the stack.
* Test: the engine-test ROM stands its overworld view in for a tile-mode
  scene. Each of its frames calls `iwram_ovl_use(OVL_2D)` and runs a 12 B
  probe from the 2D group (`ET ovl: 2D group ok`), and the next map
  (`core_vape_cellar`) loads the 3D group back (`ET ovl: 3D group back
  after 2D`; `test-et` expects both lines and shoots
  `et17_core_after_2d_group`). In that ROM a load fills the rest of the
  slot with undefined instructions, so a call into a group that is not
  loaded traps instead of running stale code.

Measured (libmgba, `7dcb273` against `7dcb273` + G4): ROM +196 B, EWRAM
13,616 B free (unchanged), main stack 7,168 B (unchanged). `make test-speed
SPEED_BASE=` with the base's log: all 15 scenes frame-exact (30 frames
each), every fps and work figure the same (combat first person 84.7 ->
84.7 ms, third person 84.3 -> 84.4 ms). Engine test: the 16 tour
screenshots byte-identical to the base, and `vhash-seq 400` from tick 992
(overworld with the 2D group in, then the core map with the 3D group
reloaded at tick 1023) gives the same 400 frames as the base (sequence
`02521a25`). Per-phase `r3d` times move by 1-2 us (the ROM code after
`iwram_ovl.c` shifted; the 3D group's IWRAM timing is unchanged).

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
| story, house, side missions, music (S1-S5, H1-H4, MU1) | 4.8 KB | `9d4d915` |
| G0: the arena | 9.7 KB | scene scratch + `gba_big` + billboards in one 26,992 B block (-5,104 B) |
| measured at `959acc8` (2026-11-01) | 9,536 B | modules landed since G0 (-416 B) |
| G1: speed tour | 9,520 B | harness mailbox 64 -> 80 B (`hold_at`, `draw_all`; section 9) |
| H10: the autopilot's BFS in the arena | **13,616 B** | `combat_bot.c`'s 4,096 B work area is the raycaster's clip checkpoints during the update (below) |

The engine-test ROM (no game scenes; measured with the game ROM above):
ROM 1,152,172 bytes, IWRAM 19,636 (12.1 KB for the stack), EWRAM 178,840
(81.4 KB free).

**H10: the autopilot's BFS borrows the clip checkpoints.** The debug
autopilot (`core/src/combat_bot.c`: demo mode, "Watch demo" on the level
select, and the core tests) kept a 4,096 B BFS work area in `.bss`: a
direction nibble and a target bit per map cell (64 x 64) and a 768-cell
queue. It holds data only inside one `plan()` call, which runs in an
update (`combat_tick`). The raycaster's clip checkpoints (6,720 B at the
bottom of its arena region) hold data only inside `plat_view3d()`, a
draw. So on the GBA the BFS works in `gba_bot_work`, the clip
checkpoints' first 4,096 B (`GBA_BIG_BOT`, `gba_plat.h`; a symbol of its
own like `gba_r3d`, so the BFS addresses it exactly as it did its
statics), and `plan()` first calls `gba_bot_claim()` (`bigbuf.c`), which
claims the region as the raycaster: a no-op in combat, where the
raycaster already owns it; any other owner rebuilds its cache, as after
any claim. `bigbuf.c` asserts that the work area stays below the roof
table, which is a cache that must survive; `combat_bot.c` asserts that
its three arrays add up to exactly 4,096 B and `bigbuf.c` pins
`GBA_BIG_BOT` to the same size. The raycaster's region clears the
largest scene scratch claim (asserted), so the BFS never touches the
weapon views. Native and PS1 keep the three statics: the PS1's
`ovl_plan.py` BORROW lends their sections to the house group by name,
and both compile to the same code as before (identical disassembly; the
PS1 image ends at the same address).
Measured (libmgba, `959acc8` against `959acc8` + H10, `plat_entropy`
pinned with `seed 7`): EWRAM 9,536 -> 13,632 B free, ROM +80 B, IWRAM
unchanged. The scout fps tour (title, 8 overworld scenes, combat first /
third person, concert, house, options, pet): every scene's fps and work
per drawn tick identical to 0.1 ms and every screenshot byte-identical;
600 ticks of the demo: the same fps (6.4), work per drawn tick 122.5 ->
121.9 ms (which ticks get drawn shifts), screenshot byte-identical. A
whole demo of the Vape Cellar (6,420 ticks: 17/17 demons, the exit, the
results screen): the results screen (time 3:22, score 55, rank B)
byte-identical in all 14 frames shot, kills, HP and ammo the same at
every checkpoint, work per drawn tick within 2 ms per segment (which
ticks get drawn shifts; 3 of 10 mid-run shots differ in the weapon bob
or the haze phase only), main-stack high-water 2,056 B both. `smoke.txt`
(`make test-game`) now plays that demo to the end: all 17 demons down,
back at the level select. Rebased onto G1 (`091aa2b`, whose mailbox is
16 B bigger): EWRAM 9,520 -> 13,616 B free, ROM +80 B, IWRAM unchanged,
and `make test-speed SPEED_BASE=` (G1's tour, `091aa2b` as the base):
all 15 scenes frame-exact (30 frames each) with the same fps and work.

Levers left for the >= 24 KB goal (task #77), none of them fps-free or
G0's: the customisation package's P0 shrinks the resident record 96 -> 76
B (-20,020 B, docs/design/customization/SPEC.md section 2.1, CF-1); the
portrait cache 48 -> 36 blocks (-5,040 B) costs fps (G0 tour: dialogue
10.6 -> 8.4 fps; 42 blocks: breach third person 100.7 -> 103.0 ms), so it
stays at 48. If the HOME state needs more room later: keep the two saved
blueprint rooms (500 B) and the photo recipes (128 B) out of `HomeState`
on the GBA and stream them from flash when their menus open, or shrink
`sc_pool` (fps cost, section 4).

### EWRAM ledger (shared; every package that changes GBA RAM adds a row)

Packages that land on the shared branch in parallel each assumed the
headroom the others use. This ledger is the one place that adds them up:
when a package changes EWRAM (or IWRAM) it appends a row in merge order
with the `memreport` lines of the merged tree, and gates are checked
against the LAST row, not against a figure from an older base.

| # | package (merge order) | base | EWRAM free before | after | change | IWRAM stack | notes |
|---|---|---|---:|---:|---:|---:|---|
| 0 | baseline | `959acc8` | | 9,536 B | | 7,168 B | `docs/GBA.md` still said 9,952 (G0) |
| 1 | G1: speed tour | `959acc8` | 9,536 B | 9,520 B | -16 B | 7,168 B | harness mailbox 64 -> 80 B |
| 2 | H10: autopilot BFS in the arena | `091aa2b` | 9,520 B | 13,616 B | +4,096 B | 7,168 B | ROM +80 B; speed tour frame-exact, fps unchanged (above) |
| 3 | H6: furniture animation frames, prop art | `ffbc039` | 13,616 B | 13,616 B | 0 | 7,168 B | ROM 3,669,044 -> 3,675,732 B (iso sprites 447,309 -> 453,094 B); room parity and house fps unchanged |
| 4 | G4: IWRAM code overlay | `7dcb273` | 13,616 B | 13,616 B | 0 | 7,168 B | ROM +196 B; IWRAM resident 10,580 B + overlay slot 10,384 B (3D group; 2D group 0 B, up to 11,408 B fits the 6 KB rule); speed tour frame-exact |
| 5 | H1: room-shell parity frames | `738261b` | 13,616 B | 13,616 B | 0 | 7,168 B | no RAM (the frames build on the stack and in the arena's harness tail, as before); ROM +996 B (`room.o` text 17,786 -> 18,782 B: the shared frame table `core/tests/room_shells.h` and its harness hook); house fps unchanged |
| 6 | L1: N-language lookup (state + 64-entry cache, below) | `883cf3b` | 13,616 B | 13,088 B | -528 B | 7,168 B | ROM 3,681,980 -> 3,689,948 B (+7,968: one LNG2 table per language, Spanish 111,516 B, was four tables of 104,476 B); Options in Spanish 28.2 -> 25.8 ms per drawn tick |
| 7 | G5: tile-mode overworld (Mode 0 world + canvas) | `f010771` | 13,088 B | **13,088 B** | 0 | 6,728 B | ROM 3,689,948 -> 3,710,784 B (+20,836); IWRAM resident +228 B (routing in the 2D entry points), `.iwram_bss` +212 B (`cv`, the canvas state); 2D group 8,224 B in the slot (3D group still the larger); the tile metadata lives in the lower half of the overworld's arena region; speed tour frame-exact (16 scenes) |
| 8 | HART: roofs, redrawn furniture | `3706744` | 13,088 B | 13,088 B | 0 | 6,728 B | no EWRAM / IWRAM: the roof's tables (up to 5 KB) live in the two 2.5 KB VRAM gaps of the mode 4 pages, which the raycaster refills (`r3d_invalidate()`); the house region stays 16,872 B. ROM 3,710,784 -> 3,721,784 B (+11,000: iso sprites 453,094 -> 455,050 B, roof code in `room_ref.c` / `room.c`); fps: modal 20.0 -> 18.8 (gate 18), rotate 25.4 -> 23.2 (20), steady 29.5, cellar 26.2, defend 12.2 (floor 11) |
| 9 | C1: show canon (turkey Mayor, animatronic Bonejangles, "Needs salt") | `27de086` | 13,088 B | **13,048 B** | -40 B | 6,728 B | `portrait_turkey.c`'s layer-group state (32 B `.bss`); ROM ROMFIG (the turkey renderer, the animatronic parts, 6 creator presets); IWRAM unchanged |
| 10 | H4: themed door / window fittings, the designed Towers 7B and Pits flat | `H4` | 13,048 B | **13,048 B** | 0 | 6,728 B | ROM -> 3,774,396 B (about +40.8 KB: 19 pieces in the iso sprites); the Walls panel's skin state is 4 B of `HomeScn` (scene scratch); room parity (the Living Room) and house fps unchanged |
| 11 | G2: overworld quick wins (Mode 4), the tile window retuned | `d5181cb` | 13,048 B | 13,008 B | -40 B | 6,608 B | `owf` pixel classes + 2 tile-cache counters; IWRAM resident +120 B (`ow_spr_rows` in, the per-pixel C blit out), so the overlay slot's largest group may now grow by 464 B (to 10,848 B) within the 6 KB stack rule; ROM 3,774,396 -> 3,779,208 B (+4,812, ~3 KB of it the `test-ow` self-test); main stack high-water over the tour 3,376 B (2 KB table cache on `plat_overworld`'s stack; G5's canvas budget must leave room for it); `CV_TILES_WIN` 250 -> 64 (section 6b); speed tour frame-exact (16 scenes) |
| 12 | G6: overworld billboards as OBJs | `65209c0` | 13,008 B | **12,952 B** | -56 B | 6,608 B | ROM 3,779,208 -> 3,787,548 B (+8,340); the block table and the frame's billboards (1,700 B) live in the IWRAM overlay slot's tail past the 2D group (asserted >= 1,712 B in `gba.ld`), the OAM entries in a 896 B buffer on `video_flip`'s stack (main-stack high-water 3,704 B with tile frames forced); 2D group 8,224 -> 8,592 B (still below the 3D group); speed tour frame-exact (16 scenes) |
| 13 | H5: Garden / Park Plot lips, door backdrop, front-edge fences | `54f2983` | 12,952 B | **12,952 B** | 0 | 6,608 B | no EWRAM / IWRAM: `RG` (the frame geometry, on the stack) +26 B for the backdrop and lip colours; ROM 3,779,208 -> 3,782,252 B on G2 (+3,044: lip / backdrop rows, front-edge items, the shared helpers, the frame table), 3,787,548 -> 3,790,592 B after G6 (measured); room parity 46..50 required, house fps unchanged (home_plain 30.0, steady 29.5 drawn) |
| 14 | G7: retained panels, "3D: Faster / Better" | `17d385b` | 12,952 B | **12,408 B** | -544 B | 6,308 B | the panel table and the 80-rect pool (`panel.c`, 480 B), `ui_dialog.c`'s page-length cache, the panel / pacing state; IWRAM resident +300 B (`pn_span`, `r3d_up2`, the `plat_rect` panel test); ROM 3,790,592 -> 3,798,164 B (+7,572); speed tour and the 132-scene story sweep frame-exact (section 4b) |
| 15 | H8: lamp pools, glow pixels, night windows | `1e8a97b` | 12,408 B | **12,408 B** | 0 | 6,308 B | no EWRAM / IWRAM resident: the house's arena region 16,872 -> 17,200 B (the lamp-pool table `RoomLight`, 316 B, and the walls' 16-colour palette in `RBig`, which may now be 15,808 B), so a claim beside the house may be 7,392 B (the house scene's is 2,828 B); a third IWRAM overlay group, the room group (3,648 B of ARM, the slot stays the 3D group's 10,384 B); ROM 3,790,592 -> 3,811,936 B on H5 (+21,344: iso sprites 489,859 -> 503,183 B with the glow / pane class lists, `room.o` text +6,494 B, `room_ref.o` +2,208 B), 3,798,164 -> 3,819,508 B after G7 (measured, the same +21,344); room parity 4, 34, 36, 40 and 53..57 required on every path; house fps: plain 30.0, steady 29.2, modal 18.3-18.8 (gate 18), rotate 24.1, cellar 26.2 with 4 lamp pools (gate 24), defend 12.1 (floor 11) |
| 16 | H9: back facings for every piece | `226179f` | 12,408 B | **12,408 B** | 0 | 6,308 B | no EWRAM / IWRAM resident; the room overlay group 3,648 -> 3,248 B (the mirrored run copies are gone: nothing is drawn flipped). ROM 3,819,508 -> 4,201,300 B (+381,792: iso sprites 503,183 -> 878,496 B, 1,744 facing records instead of 872; 209 facings share an earlier one's bytes), so the padded ROM file is 8 MB instead of 4 MB (the cartridge map is 32 MB; the user approved about +450 KB, DECISIONS). Room parity: golden lines 1-3, 6-8, 10-12, 22, 24, 25, 27, 28, 30-35, 39-41, 45-53, 55-57 regenerated (turned views, fenced front edges), every required line passes on every path; house fps: plain 30.0, steady 29.5 (29.2), modal 19.0 (18.8), rotate 24.7 (24.1), cellar 26.3 (26.2), defend 12.6 (12.1); speed tour frame-exact against `226179f` (16 scenes) |

Known claimants still to land (their own packages measure and add rows):

| package | EWRAM | gate it must meet (against the last row) |
|---|---:|---|
| H11: `HOME` `.bss` on the GBA (`HOME_PERSIST`) | +3,432 B | free >= 12,352 B before it (SPEC C9) and >= 8,192 B after it; after G1, H10, L1, C1, G2, G6 and G7: 12,408 -> 8,976 B |
| L4: Latin glyph pages | 0 (`const` rows in ROM) | |
| G3: the island's run layer | 0 `.bss` (from the arena) | free >= 9,000 B |
| H2: room.c shell patterns | record EWRAM / IWRAM | |

**L1: the language lookup** (`core/src/lang.c`, `doomo/lang.h`). Every
UI text call goes through `lang_tr()`. Its state and a 64-entry cache (16
sets of 4, 512 B) are one 528 B block in EWRAM: a string in the cartridge
ROM (`plat_text_is_const()`, `src/lang_gba.c`) is looked up once per
language and then found by its address, and a translation passed in again
(`ui_text_shadow` and friends translate twice) is recognised by its address
inside the table; text built in RAM is hashed and searched every time (one
bucket of one table). Measured with the scout's island (seed 7, libmgba
harness, work per drawn tick, `6714f43` -> L1):

| screen | English | Spanish | `lang_tr` + search (profile) |
|---|---:|---:|---:|
| Options, Game page | 25.7 -> 25.4 ms | 28.2 -> **25.8 ms** | Spanish 6.7% -> 0.8% |
| Unlocks | 63.2 -> 63.1 ms | 66.8 -> 65.4 ms | |
| island | 94.0 -> 93.9 ms | 96.4 -> 95.6 ms | |

The Options screen also passes its value names to `ui_value_row()` as the
constant strings they are (they were copied into a buffer first, so each
was hashed and searched twice per frame). A direct-mapped cache of the
same size missed 742 of 3,536 lookups on that screen (string literals sit
4-byte aligned, and pairs of them shared a slot every frame); the 4-way
sets miss only the 20 first sightings. What is left in Spanish is mostly
the longer text itself (`vid_text`) and, on the Unlocks screen and the
island, text formatted at run time (L2a / L2b translate it once).

Measured main-stack high-water marks (the boot code paints the free IWRAM
and `gba_stack_used()` scans it): 660 bytes in the game's boot / title /
UI gallery (1,568 with the span rasteriser's row buffers), 936 - 1,944
bytes in the 3D tour, overworld and house, 2,520 bytes while saving (the
flash routines' copy). The game ROM logs `GBA: main stack high-water N
bytes` whenever the mark grows by 256 bytes.

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

### Speed pass 2: measured (libmgba, `harness fps` / `profile`)

The game ROM's title screen, which the user reported as very slow,
measured with `gba/build/harness --script` (`fps` prints logic ticks/s,
drawn frames/s and the CPU work per drawn tick):

| scene | drawn fps | work / tick | hot spots (profile) |
|-------|-----------|-------------|---------------------|
| title, logo drop | 19-21 | 39 ms | |
| title, idle | 13-14 | 55-57 ms | `vid_blit` 11%, chibi re-render (`capsule_plain`, `pc_finish_lines`, `sh_span`) 22%, `vid_rect_narrow` 9%, `vid_text` 8% (outline text = 5 passes), `gba_udivmod` 4% |
| title, menu open | 11-12 | 62-66 ms | + `roster_find` 6% (now cached per parade turn) |

The GBA title is not the shape-cache overflow the PS1 had (its scale-3
letters fit the 640-rect pool, which is kept at that size for EWRAM's
sake); it is bound by the two animated chibis (one sliced render step
per tick, ~10 ms), the figure blits and ~1,000 small rects per frame.
Candidates, by payoff: a run-list blit for cached figures (skip
transparent runs instead of testing pixels), a `plat_text` outline pass
drawn once from a dilated glyph mask, caching the static sky / hills
layer and DMA-ing it, and replacing the `%` / `/` in the per-frame title
maths with shifts and tables. The harness now also parses the core's
`scene: set / push NAME` log lines, so `expect-scene` works in game-ROM
scripts.

### Speed pass 3 (libmgba, `harness fps`, game ROM, every scene of the tour)

Drawn frames per second and CPU work per drawn tick (the pacing shows
a frame every second vblank at best, so a tick over 33 ms halves the
rate): the HEAD before the pass against after it. Pixel for pixel the
same (the UI gallery pages, options, news, library and creator shots are
byte-identical; the native raster tests check the new arithmetic).

| scene | before | after | notes |
|-------|-------:|------:|-------|
| title, logo drop | 20.1 / 38.8 ms | 21.8 / 34.4 ms | |
| title, idle | 13.5 / 58.0 ms | 14.5 / 44-51 ms | hills cached, outline text in one pass, blit fast path (the pair / theme vary between runs) |
| title, menu open | 12.3 / 59.7 ms | 13.9 / 52.6 ms | |
| apartments | 24.0 / 35.6 ms | **30** / 33.4 ms | |
| new island | 21.0 / 37.9 ms | 23.4 / 35.8 ms | |
| studio / play | 24.7 / 35.0 ms | 27.8 / 34.1 ms | |
| library | 16.0 / 56.1 ms | 17.3 / 53.8 ms | |
| creator | 13.5 / 58.2 ms | 13.4 / 54.5 ms | `unlock_find` per field (creator module) |
| shop, roster, resident | 21-27 | 21-27 | 35-41 ms |
| mg whack / gallery | 13.9 / 12.0 | 14.2 / 12.0 | 65 / 75 ms: thousands of 1-pixel rects (minigame module) |
| island hub / town hall menu | 7.5 / 5.5 | 7.5 / 5.6 | 96 / 139 ms: ~2,100 rects + building blits |
| pet, exorcism, concert, dialogue | 6.3-8.9 | 6.6-8.9 | 90-120 ms: the modules' own drawing + chibi re-renders |
| 3D (combat, breaches, arena, overworld) | 8.8-12.7 | 8.7-12.9 | 70-96 ms, raycaster bound |

What changed on the GBA (all exact; IWRAM keeps its 6 KB main stack with
176 bytes to spare, EWRAM is within 200 bytes of before, 8.3 KB free):

* `plat_text_outline` (platform.h; `ui_text_outline` calls it): the
  outline is one pass of 7x11 dilated glyph masks (dilated per glyph
  drawn from the transposed font, ROM code since IWRAM is full) plus the
  text, instead of ten `plat_text` passes.
* `vid_blit`: a scale-1 unflipped path reads source pairs straight from
  the row (no column table), the common case of every cached chibi,
  building sprite and crowd silhouette; `plat_rect`: a one-pixel fast
  path (dots, sparkles, stars) with no stack frame.
* The DMA fills' source word (`plat_clear`, full-width rects) lives in
  IWRAM: DMA re-reads a fixed source for every word, and an EWRAM read
  costs 6 cycles against 1 (a 38 KB clear was 2.5 ms slower).
* Core: ellipses and flat hearts without a square root or division per
  row, `fx_dist` with the 32-bit root, the title's hill heights computed
  once (120 software divisions a frame), a memo in front of the portrait
  cache scan.
* Harness: `expect-scene` mirrors the core's scene stack, so it is right
  after a `pop` (it reported the popped scene before); `make -C gba test`
  rebuilds `build/harness`, a plain `make` does not.

The GBA's slow screens are the scene modules' own drawing: the island
hub issues ~2,100 small rects a frame (land spans, roads, decorations,
walkers) and the minigames thousands of 1-pixel rects, the dialogue /
exorcism / concert / pet redraw their whole stage each tick and
re-render a chibi phase in slices (one group per tick, ~12 ms), and the
3D scenes are raycaster bound. The next steps there are structural:
a static land layer for the island drawn once (needs ~31 KB that EWRAM
and VRAM do not have at once), dirty-rect redraws of the static parts
of the dialogue / pet / concert stages, and a stand-in image while a
chibi's new expression renders in slices.

### Speed pass 4: baseline (G1, `make -C gba test-speed`)

One command measures every target scene: `make -C gba test-speed` plays
`harness/scripts/speed_tour.txt` on the game ROM (about 21 s of wall
time), fails if a scene drops below its `expect-fps` floor (about 85% of
the numbers below), writes `build/speed_tour.log` and
`build/shots/speed/*.png`, and prints the table with
`tools/speedtable.py` (standard-library Python). It is not part of
`make -C gba test`. Every scene settles for 200 logic ticks, is measured
over 130, is shot, and then the frames of the next 30 ticks are hashed
(`vhash-seq`, below). The tour runs `sync on 61` (tick-exact from
absolute tick 61 on) with `seed 1` before every scene, so a ROM gives
byte-identical numbers and hashes on every run.

The numbers belong to this exact script: every scene's ticks are
absolute, and a scene's cost depends on the tick it starts on and on the
scenes before it (the town, the concert's generated show: `wait 7` before
the concert moves it from 12.0 to 12.9 fps, and playing the concert first
drops the hahaland visit from 16.9 to 13.6). Adding or removing scenes or
waits therefore moves every later scene's numbers and frames; changing an
`expect-fps` floor moves nothing.

Baseline, `959acc8` + G1 (game ROM 3,668,820 bytes):

| scene | how the tour gets there | logic ticks/s | drawn fps | work avg / max ms |
|-------|-------------------------|--------------:|----------:|------------------:|
| title | `scene TITLE` | 29.9 | 27.6 | 27.7 / 62.4 |
| hahaland visit | `scene STORY 10` | 29.8 | 16.9 | 52.1 / 52.6 |
| overworld pause | the visit, START | 18.4 | 10.5 | 91.5 / 145.6 |
| hellvile visit | `scene STORY 11` | 30.1 | 22.7 | 37.7 / 39.5 |
| park visit | `scene STORY 12` | 29.9 | 22.1 | 37.5 / 39.8 |
| invaded | `scene STORY 219` | 14.9 | 8.6 | 113.6 / 132.3 |
| story map + dialogue | `scene STORY 110` | 30.0 | 15.0 | 53.6 / 55.4 |
| concert | `scene CONCERT -2` + the setup keys of `concert_solo.txt` | 21.3 | 12.0 | 59.8 / 76.7 |
| combat first person | `scene TITLE`, `push COMBAT 3` | 16.4 | 9.8 | 84.7 / 162.7 |
| combat third person | the same, SELECT | 16.2 | 9.6 | 84.3 / 186.1 |
| house plain | `scene HOME -1` | 30.0 | 30.0 | 10.0 / 12.0 |
| options | `scene TITLE`, `push OPTIONS 0` | 30.0 | 30.0 | 25.1 / 27.2 |
| island hub | the New Island keys of `fp_island.txt` | 12.5 | 7.5 | 96.8 / 97.3 |
| island town hall menu | the hub, A | 11.1 | 6.7 | 108.7 / 131.0 |
| pet | `push PET` over the island, A, START | 17.4 | 10.3 | 67.4 / 94.9 |

G5 added a scene after the park visit, the map walk (the park visit with
RIGHT + DOWN held, so the camera scrolls on both axes while the frames are
measured and hashed): every later scene starts on other ticks than in the
table above, so a parity base must be recorded with the current script
(below). Section 6b has the G5 numbers.

Where the time goes (the profiling pass of 2026-11-01 on `959acc8`:
`harness fps` and `profile`, plus temporary `gba_cycles()` marks around
the draw regions; the marks are a local patch and are not in the tree):

| scene | fps / work ms | breakdown |
|-------|--------------:|-----------|
| title | 28.4 / 27.2 | |
| hahaland visit | 16.6 / 52.7 | ground 13.7, billboards 23.7 (24 sprites, 28,466 px, only 7 distinct images), blocks ~6, HUD 4.1 (objective pill 2.5, clock 1.5), logic 3.1 |
| hellvile visit | 22.2 / 38.1 | ground 13.0, sprites 11.0 (16, all on the tinted slow path), HUD 4.1 |
| park visit | 21.9 / 38.0 | ground 15.0, sprites 9.8 (31k px), `owfx_world` 1.5, HUD 4.1 |
| invaded | 8.3 / 114.7, logic 14.4 ticks/s | `ow_sprite` 77.2 for 47 tinted sprites (45,293 px, 23 distinct images, 20.8 KB as padded 8bpp, max 46x49), ground 15.2, HUD 4.3, toast 2.7, logic 6.9 (`ownpc` 2.5, `find_interactables` 1.8) |
| story map + dialogue | 15.0 / 54.0 | blocks / raised cells ~30, sprites 9.9, HUD 4.4, `dlg_draw` 5.5 |
| island hub | 7.5 / 94, logic 12.5 ticks/s | `fp_island_draw_map` ~100 per drawn frame, `town_count(-1)` 3.0 every frame, `fp_ticker` 2.3, `fp_hud` 3.3; profile: `vid_rect_narrow` 9.7%, `fp_island_draw_map` 9.6%, `put` 8.2%, `town_count` 6.2%, `vid_blit` 5.7%, `vm_span`/`vm_flush` 9% |
| island town hall menu | 6.7 / 107 | map 82 + menu window 41.5 |
| combat first person | 9.5 / 86, logic 15.8 ticks/s | r3d 46-50, HUD 22.7-28 (weapon view 10.3, health + ammo panels 12.3, top left/right 5.5), other drawing ~20, logic 9 (`fx_dist` 10.9%, `__muldi3`, `__udivmoddi4`) |
| combat third person | 9.5 | HUD 9.8 |
| concert | 15.0 / 51.5 | `vid_blit` 32%, `plat_checker` 12% |
| pet | 10.6 / 65.6 | `lcd_rect` 19%, `fx_isqrt` 14.5% (per pixel), `lcd_sprite` 8%, `egg_fill` 6.7% |
| house plain | 30 / 10.0 | |
| options | 30 / 24.9 | |

On the maps the HUD costs 4-5.5 ms and the dialogue box 5.5 ms; the maps
themselves cost 28-98 ms (the per-pixel billboard blit, ~0.8 us/px and
~1.7 us/px on the tint / night path; ~190 cached 16x16 ground tiles copied
per frame at ~74 us each with no cache misses; the block fronts and tops).
In combat the HUD is a third of the frame. The pacing never skips two
ticks in a row, so a frame over 66 ms slows the game itself: the island,
the invaded map, the pause panel and combat run below 30 logic ticks/s.

The tour agrees with the profiling pass within 5% on every scene except
the concert (12.0 fps / 59.8 ms against 15.0 / 51.5): its show (lyrics,
crowd) is generated, and the tour's pinned seed and entry tick give a
dearer show than the profiling run's. The same ROM measured 15.0 / 51.3
with a different show while the tour was being written. The overworld
pause is new (no earlier figure).

#### G2: the overworld in Mode 4, exact (`make -C gba test-speed`)

G2 sped up `plat_overworld` without changing the display mode or a pixel
(section 6 has the how). `make -C gba test-speed SPEED_BASE=` against its
merge base (`6714f43`, H6), both ROMs on the same tour:

| scene | logic ticks/s | drawn fps | work avg / max ms | base fps | base avg / max ms |
|-------|--------------:|----------:|------------------:|---------:|------------------:|
| hahaland visit | 30.1 | **26.9** | 34.3 / 35.5 | 16.9 | 52.1 / 52.6 |
| overworld pause | 22.8 | 12.5 | 75.2 / 127.8 | 10.5 | 91.5 / 145.6 |
| hellvile visit | 30.0 | **30.0** | 23.8 / 24.9 | 22.7 | 37.7 / 39.4 |
| park visit | 30.0 | **30.0** | 26.6 / 28.4 | 22.1 | 37.5 / 39.8 |
| invaded | **30.0** | **15.2** | 53.4 / 68.7 | 8.6 (14.9 ticks/s) | 113.6 / 132.3 |
| story map + dialogue | 30.0 | 24.0 | 36.1 / 37.3 | 15.0 | 53.6 / 55.4 |
| every other scene | | unchanged | | | |

All 15 scenes' 450 hashed frames are identical to the merge base's.

After the rebase onto G5, HART, C1 and H4 (`d5181cb`, the 16-scene tour with
the map walk), against that tip's ROM, with `CV_TILES_WIN` retuned
(section 6b; at G5's 250 the tile frames held hellvile at 23.3 fps and
park at 25.2):

| scene | logic ticks/s | drawn fps | work avg / max ms | base fps | base avg ms |
|-------|--------------:|----------:|------------------:|---------:|------------:|
| hahaland visit | 30.0 | **26.1** | 34.7 / 38.1 | 17.1 | 52.4 |
| overworld pause | 22.5 | 12.3 | 74.8 / 128.3 | 10.4 | 92.4 |
| hellvile visit | 30.0 | **30.0** | 24.1 / 26.6 | 23.3 | 34.6 |
| park visit | 30.0 | **30.0** | 27.0 / 31.7 | 22.1 | 37.7 |
| map walk | 30.0 | 29.0 | 28.4 / 47.7 | 23.3 | 37.6 |
| invaded | 29.5 | **15.2** | 54.3 / 69.2 | 8.5 (14.7 ticks/s) | 114.3 |
| story map + dialogue | 30.1 | 22.7 | 37.8 / 42.6 | 15.0 | 55.5 |
| every other scene | | unchanged | | | |

All 16 scenes' 480 hashed frames are identical to the tip's. The invaded
map's logic rate reads 29.5 ticks/s here (30.0 on G2's own base): the
tip's content costs the core a little more there, and the worst frame
(69.2 ms, `build_billboards()`) still decides it (G11). The
gates (hahaland >= 24 fps, hellvile and park >= 29, invaded >= 15 with 30
logic ticks/s) hold; the tour's floors are now 24 / 29 / 29 / 15 (and 11
for the pause, 20.5 for the story map). The invaded map's worst frames
(68.7 ms) are its core's: every ~18 drawn frames `build_billboards()`
(`core/src/overworld.c`) takes ~20 ms instead of 4 (a sprite-frame
rebuild), on top of `plat_overworld`'s 36-38 ms; it is the one thing
between that map and a steady 15 fps margin (G11). The `shot`s of the
hahaland, invaded and pause scenes differ from the base's: a `shot` in
sync mode shows whichever frame the speed put on screen (section 4,
above), the hashed frames do not.

Where the overworld's time went (temporary `gba_cycles()` marks, per
drawn frame, base -> G2): hahaland ground 13.7 -> 8.1 ms, sprites 23.7 ->
13.7 ms (24 sprites, 28,466 px); hellvile sprites 11.0 -> 5.2 ms; park
sprites 9.8 -> 6.1 ms (31,084 px); invaded sprites 77.2 -> 24.6 ms (47
sprites, 45,284 px; 4 table builds a frame); the story map's blocks ~30 ->
~16 ms.

Exactness beyond the tour: a sweep of every story beat (`scene STORY
100+b` by day and `200+b` at night, b = 0..65, every tick drawn and
hashed from the scene's entry: `vhash-seq 70`) gives 132 x 70 = 9,240
frames identical to the merge base's; and `make -C gba test-ow` (part of
`make -C gba test`) checks 2,400 random sprites, including the mirrored,
translucent, scaled and clipped cases the maps rarely show. A sweep that
waits before hashing (`wait 60`, then `vhash-seq 10`) differs in 8 of
the 132 scenes, in both directions, while every-tick hashing of the same
scenes matches: the core keeps some state per *drawn* frame (sparkles,
the invaded portal's animation), so a ROM that skips different catch-up
ticks before the window draws a different state. That is a core
property, not a renderer one; parity checks must hash from the scene's
first tick, as the tour's windows do after its settle.

#### Frame parity between ROMs (`vhash-seq`, `SPEED_BASE`)

A speed package must not change a pixel. The harness hashes the frame
mGBA puts out (`vhash`: FNV-1a of the 240x160 output as BGR555), which is
the same whatever the display mode, unlike a CRC of a Mode 4 VRAM page.
`vhash-seq 30 LABEL` hashes the frames of the next 30 logic ticks: the ROM
draws every tick meanwhile (mailbox `draw_all`, no catch-up ticks), and
every frame is tagged with the tick that drew it; the harness fails the
run unless each of the 30 ticks presented exactly one frame. With `sync
on` the harness holds the ROM at the end of every wait (mailbox
`hold_at`, see section 9), so keys and commands land on the same tick in
every ROM however fast it draws; with `seed 1` equal ticks are equal game
states, and two ROMs' frames compare tick for tick. A single `vhash` is
not for parity: it hashes the last frame presented, which depends on the
ROM's speed (catch-up ticks), and in sync mode it shows the frame before
the held tick's flip (`shot` too).

The parity check (`tools/speedtable.py --parity` / `--compare`) is exact:
for every hashed scene of either log, both logs must hold all 30 frames
(no gap, no vhash-seq error), the same ticks, and identical hashes. A ROM
that enters a scene a tick late, drops a presented frame or freezes the
display fails; so does a base log from an older harness (no frame count)
or from a different tour script (other ticks: the report names a tick
shift under which the frames would match, as a hint).

To prove a package exact, run the package's own tour script on its merge
base, keep the log, and run the package against it:

```sh
cp gba/harness/scripts/speed_tour.txt /tmp/tour.txt   # the package's tour
git checkout --detach $(git merge-base HEAD <target branch>)
make -C gba test-speed SPEED_SCRIPT=/tmp/tour.txt SPEED_GATES=0 SPEED_LOG=/tmp/base.log
git checkout -                       # back to the package
make -C gba test-speed SPEED_BASE=/tmp/base.log
```

`SPEED_SCRIPT` plays the package's script on the base ROM, so both logs
hash the same ticks even when the package added scenes or waits;
`SPEED_GATES=0` (harness `--no-fps-gates`) reports a floor the base ROM
misses (a package that sped a scene up raises its floor) instead of
stopping the tour. The table then shows the base's fps and the change per
scene, and every hashed frame must be identical (`parity ... ok: 30
frames identical`); `tools/speedtable.py --compare A.log B.log` prints
only the parity. Use the merge base, not a fixed baseline ROM: script and
text changes alter dialogue frames. The base needs G1's harness (and its
fixes: `SPEED_SCRIPT`, `SPEED_GATES`, the exact check); a ROM from before
G1 has no `hold_at` / `draw_all`, and `sync on` and `vhash-seq` refuse it.

Checked when G1 landed, on the committed tour: two runs of one ROM give
byte-identical logs (15 scenes, 450 frames); a ROM with a busy loop at
boot and 8.9 ms of extra work per drawn tick (a temporary patch) drew
7-12% fewer frames, yet all 450 hashed frames matched the base ROM's at
the same ticks; a ROM that flips two pixels on one tick in 64 failed in
the 9 scenes whose 30-tick window holds such a tick and passed the other
6; a ROM that skips presenting one tick in 64 even under `draw_all` failed
the harness at the first such window ("29 frames from ticks ..."); and
logs edited to shift a scene's frames by one tick, or to keep 10 of its 30
frames, fail the parity check.

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

## 4b. Retained panels (G7, `src/panel.c`)

The core draws its HUD pills, dialogue box, toasts and island HUD as
*panels* (`platform.h` `plat_panel_begin` / `plat_panel_end`): a panel id,
the rect it draws in and a key of everything its pixels depend on (text,
style, animation phases, the language and its glyph page: `ui.h`
`ui_panel_key`). When the GBA still has a panel from the last frame drawn
with the same id, rect and key, `plat_panel_begin` returns 0: the core
draws only the panel's drop shadows, and `plat_panel_end` puts the panel
back.

**Where a kept panel comes from.** Mode 4 draws into the back page while
the front page shows the last frame, so a panel drawn (or put back) last
frame is still in the front page, pixel for pixel. Putting it back is a
copy of its body from the front page to the back page: `pn_span`
(`video_loops.s`, ARM in IWRAM) per row, odd edge pixels as halfword
read-modify-writes and the rest as one 16-bit DMA. The body is what the
first frame drew with `plat_rect` / straight `plat_line` inside the panel,
kept as a list of rects (a rect inside one already listed is skipped; an
80-rect pool shared by all panels). Text, images and checkers lie on that
body (the `platform.h` rule), and shading is not body: the core shades the
new picture again before the copy. A panel's *hole* (`plat_panel_hole`) is
left out of the copy: the news ticker's scrolling line, and the dialogue
box's text area, which `PANEL_DIALOG_TEXT` repaints (the box's rows under
the text through `ui_window_rows`, the text, the heart), so the
typewriter redraws only that.

**When it is valid.** Only on the frame right after the one that showed
it, from the page that frame was shown from (`pn_flipped` in
`video_flip`), and only if nothing drawn after it on that frame touched
its rect outside its hole. While a panel is drawn, put back or shown,
`gba_nodraw` is `GBA_NODRAW_PANEL` (5): `plat_rect`'s ARM entry tests the
call's rect against `pn_box` (the shown panels' bounding box) and the C
entries test their rows, so calls that cannot touch a panel keep the
Mode 4 fast path; the others go through `vid_route` and `pn_route`, which
marks a panel they touch spoiled. Once nothing is left to watch,
`gba_nodraw` goes back to 0. Not kept: tile frames (section 6b: the canvas
draws as before), skipped ticks (nothing is drawn), the harness's armed
`expect-text`, and a panel that used `plat_clear`, a squash or figure OBJ,
drew outside its rect or overflowed the pool. A panel missed twice in a
row (it changes or is drawn over on every frame: the typewriter, a toast
over the clock, the pause panel) is drawn uncaptured for 2, 4, then 8
frames between tries, so it costs nothing.

**The adopted panels** (`core/src`): the overworld objective, clock and
action prompt (`overworld_fx.c`), the dialogue box with its portrait, name
pill and choices plus its text area (`ui_dialog.c`), the toast (`ui.c`),
the island HUD pills and the news ticker (`fp_common.c`). Their keys are
checked on the native platform: `core/native/panel.c` keeps panels the
same way (with memory instead of a front page), and
`core/tests/test_panel.c` plays the overworld HUD, a narration, portrait
lines with a shout, the pill and a choice, toasts, the island and a
language switch at 240x160 and 320x240 with the cache on and off: every
frame is identical, no panel breaks a rule, and most frames keep their
panels.

**Exactness.** `make test-speed SPEED_BASE=` against the merge base's log
(`54f2983`, the same tour script, and again on `17d385b` after H5): all 16
scenes' 480 hashed frames are identical; and the story sweep (`scene STORY 100..165` and `200..265`, seed
7, `vhash-seq 70` from each scene's first tick: 9,240 frames with portrait
lines, pills, choices and toasts) is identical to the base ROM's.

Measured (libmgba, work per drawn tick, `54f2983` -> G7; the same on `17d385b`):

| scene | drawn fps | work avg ms | base fps | base avg ms |
|-------|----------:|------------:|---------:|------------:|
| hahaland visit | **29.4** | 33.3 | 26.1 | 34.4 |
| hellvile visit | 30.0 | 22.7 | 30.0 | 24.1 |
| park visit | 30.0 | 25.6 | 30.0 | 27.0 |
| map walk | 29.0 | 27.0 | 29.0 | 28.5 |
| story map + dialogue | **25.3** | 35.8 | 22.7 | 37.8 |
| invaded | 15.1 | 55.9 | 15.2 | 54.2 |
| overworld pause | 12.3 | 75.9 | 12.3 | 74.7 |
| island hub | 7.5 | 96.8 | 7.5 | 96.8 |
| island town hall menu | 6.5 | 109.9 | 6.5 | 107.7 |
| every other scene | | within 0.1 ms | | |

The visits' work per drawn tick drops by 1.1-1.5 ms (the HUD, drawn every
frame, cost ~4 ms there), the story map with its narration box by 2.0 ms
(the profile's 2D share of that frame went from ~18% to ~9%, about a third
of it the copies); the story map's world (~30 ms of blocks, ground and
sprites) is what keeps it below 30 fps. The invaded map, the pause panel and the town hall menu draw over
their panels on every frame (a toast over the HUD, the full-screen shade,
the menu window over the island HUD): they gain nothing and pay the key
and the rare capture, 1-2 ms. The plan's tile-mode figures (HUD ~0.3 ms,
STORY 110 at 30 fps with work <= 12 ms) assume tile frames, which the
tour does not show (section 6b, `CV_TILES_WIN`): a tile frame draws its
panels into the canvas as before, and keeping them there (canvas tiles a
panel owns across frames) is still open.

Memory: EWRAM -544 B (the panel table 8 x 20 B, the 80-rect pool 320 B,
the page-length cache of `ui_dialog.c` and the box / state words), IWRAM
resident code +300 B (`pn_span`, `r3d_up2`, the `plat_rect` panel test):
main stack 6,308 B (>= 6,144).

### "3D: Faster / Better" (Options, Performance page)

`OPT_3D_FAST` (`town.h`; the row shows on the GBA only, default Better):

* **Better** (0): the 3D view at full resolution; `video_flip` shows a
  frame with a 3D view at most every third vblank (20 fps), so it holds
  steady once the view is fast enough (today it is slower: combat runs at
  ~10 fps, so nothing changes there).
* **Faster** (1): `plat_view3d` renders the view at half resolution (half
  width and height, the same field of view) into the top left quarter of
  its rect, and `r3d_up2` (`video_loops.s`, ARM in IWRAM) doubles it in
  place, from the bottom right so no source pixel is overwritten before it
  is read; the HUD is drawn afterwards at full resolution. Measured on the
  tour's Vape Cellar (COMBAT 3, 130 ticks): first person 9.9 -> 11.8 drawn
  fps, logic 16.4 -> 21.3 ticks/s, work 83.5 -> 62.4 ms; third person 10.5
  -> 12.6 fps, 78.7 -> 55.0 ms. This is the minimal version: the plan's
  hardware scaling (BG2 affine 2x with the HUD as OBJs) and the 30 fps
  target are G9's, which moves the combat HUD and the weapon to OBJs and
  cuts the logic cost.

## 5. `plat_view3d`: the height raycaster

Files: `src/r3d.c` (THUMB setup), `src/r3d.iwram.c` (ARM column pass,
sprites), `src/r3d_loops.s` (hand-scheduled ARM: column walk + boundary
handling, flat/face/sky/sprite loops), `src/r3d_int.h`. The ARM code of
`r3d.iwram.c` and `r3d_loops.s` is the IWRAM overlay's 3D group (section
3, G4): `plat_view3d()` loads it first, and nothing else may call it.

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
front height and tint state) in a 2-way, 32-set cache in the arena, so a
frame of ground is mostly 16-pixel block copies; horizontal scrolling
moves in 2-pixel steps so every copy is halfword aligned. Cells covered by
a block top + front face skip their ground tile. A frame may instead show
the world as a tile BG (section 6b).

G2 (Mode 4, exact; numbers in section 4, "G2"):

* Sprites (`ow.iwram.c`, `ow_loops.s`). A sprite pixel is the colormap,
  then the night dither, then the view-tint dither: one of four classes
  per 4x4 screen position. `ow_frame_luts()` works out the frame's
  classes, and `owf_sprite_luts()` composes a colormap with them into
  256-byte tables once per colormap and frame (an 8-table cache, 2 KB of
  `plat_overworld`'s stack: 8 colormaps by day, 4 at night or under a
  tint). Every pixel is then one table lookup. `ow_spr_rows` (ARM, IWRAM)
  draws all of a sprite's rows: four texels per aligned word read (ROM is
  a 16-bit bus), transparent quads skipped, opaque pairs stored without a
  read, two quads per loop, the clipped odd edge columns masked off the
  first and last quad. Mirrored, scaled and translucent sprites go
  through it row by row from a texel buffer. The house (`room.c`) calls
  `ow_sprite` outside `plat_overworld`: no tables, the colormap as is.
* Ground. One scan of each cell row's visible columns draws the ground
  tiles (row pointers into the map, a run of equal cells reusing one
  lookup) and notes the raised cells in a bit mask; the raised pass then
  visits only those.
* VRAM tile copies. A lit tile drawn a second time in a frame is first
  DMA'd into one of 20 slots in the two never-displayed 2,560-byte gaps
  after the Mode 4 pages (`0x06009600`, `0x06013600`) and copied from
  there: VRAM reads cost about half of EWRAM's. The raycaster keeps its
  colormap rows in the same gaps (`r3d_int.h`), so a frame that takes a
  slot clears `r3d_cmrow[0]` and the next 3D frame rebuilds them (once,
  ~0.5 ms). The house's roof tables (HART, section 6a) live there too:
  `plat_room` and `plat_overworld` are never drawn in the same frame (no
  core scene calls both), and the house rebuilds its tables after any
  frame it did not draw. Tile frames (section 6b) never touch the gaps;
  the copies are made only by Mode 4 frames.
* Blocks. A front face whose column the next row's top covers (the cell
  below is raised at least as high) is not drawn: everything drawn there
  in between, that row's sprites included, was always overwritten by that
  opaque top. A row's contact lines are shaded as runs, after its blocks
  and before its sprites (no top or other front reaches that screen row).
  Fronts and tops reuse the previous lookup when the cell repeats.

IWRAM after G2: 120 bytes more resident code (`ow_sprite` shrank,
`ow_spr_rows` is new): 6,608 bytes for the main stack on top of G5
(was 6,728); main stack high-water over the speed tour 3,376 bytes (was
2,280: the table cache).

---------------------------------------------------------------------------

## 6a. `plat_room`: the dream house (H2, `gba/src/room.c`)

<!-- H2 (SPEC sec.7 H2): text for I9 to merge into this doc -->

`SCENE_HOME` draws its room through `plat_room()` (platform.h; HF sec.6,
SPEC C7). The GBA renderer draws exactly what the reference
`core/src/room_ref.c` draws. `make -C gba test-home` checks the parity
layout of `core/tests/test_home_render.c` against
`core/tests/golden/room_ref.txt` hash for hash: views 0-3 and night, a
full recompose, the dirty-rect path and the UI-restore path.

Since H1 the golden also holds the shell frames of
`core/tests/room_shells.h` (lines 5-37; the table is shared by the native
test and `room_hc_parity`, so the two cannot drift): every wallpaper,
floor and ceiling ramp, the five tints, the Garden and Park Plot kinds by
day, dusk and night, and the 8x6 room turned. The GBA draws only
wallpapers 0-3, floors 0-1, the wood cornice, no tint and the noon sky
(the H2 gap), so those frames are `xfail` in `scripts/home.txt`: the
harness prints their mismatch and fails if one starts to match. Lines 5
and 33-37 stay in the vocabulary the GBA draws (the non-square room
turned, an open kind, dim light) and are required on every path. As of
H1, 6 of the 33 shell frames match (`room-parity` probe: the tinted
frames hash the same as the untinted classic view, the tint is ignored).

**A static bitmap with dirty rects.** Each mode-4 page remembers what it
shows: the shell key (room, view, light, paints, viewport, HUD bands),
the draw list (screen box + signature per item) and a checksum of every
16x8 block of the managed area. Each frame then runs four steps:

1. Re-read the page's block checksums. A block that changed was drawn
   over after `plat_room` (menus, captions, the iris), so it is redrawn.
   The core never has to report its 2D drawing.
2. Diff the draw list. New, gone or changed items dirty their old and new
   boxes as exact rects, merged only when that wastes little.
3. Redraw each rect: background, walls, floor, then the items that touch
   it, in room_ref's order.
4. Re-checksum the blocks the rects touched.

A full recompose happens on a changed view, light, room or HUD band, a
lost `gba_big` claim, or more than 70% dirty area. The other page then
starts from a VRAM-to-VRAM copy of the shown page (about 2 ms) instead of
a second recompose. The opaque HUD bars (`plat_room_ui_opaque`, called
after `plat_room`) are never drawn or checked. Rects a scene registers
*before* `plat_room` in the same frame skip the check for that frame: a
modal window that stays put then costs nothing.

**Pixel paths.** There is no new IWRAM code; everything per pixel runs in
the existing ARM loops or DMA.

| Part | How it is drawn |
|---|---|
| Walls | The wallpaper repeats every 16 units, so a wall is one sheared 16 px strip. It is cached as 8 phase rows, so a wall's screen row is one DMA. |
| Floor | The tiles repeat every 32x16 px (upper-half tiles win the 2 px overlaps). A floor row is one or two DMAs from 16 cached pattern rows, plus at most 2 px patches on the two front edges. |
| Plain pieces | No paint, full light: each opaque run goes straight from ROM. (H9) Nothing is mirrored: a piece turned away from the camera draws its own back facing (furn.h), so the turned views take the same paths as view 0. |
| Other pieces | Each run goes through a 256-byte LUT (paint recolour composed with `doomo_colormap[light]`) into a stack buffer laid out at the destination's pixel parity, so the copy is always a halfword DMA. |
| Actors | `ow_sprite`. |
| Squash | `plat_image_xform` (S1's affine OBJs) when nothing covers the piece; otherwise drawn unsquashed in order. |

Per object, the item and its sort entry are cached until the object or
the view changes. The sort is room_ref's topological pick, done on
bitsets and re-run only for entries that moved.

**Memory.**

| Resource | Use |
|---|---|
| arena | Claim `GBA_BIG_BY_ROOM` (`gba_plat.h`); the house's region `gba_room` (17,200 B, section 3). State is at most 15,808 B (C7's 15,488 B plus 320 B for H8's lamp-pool table; measured 15,692 B); the harness layouts sit behind it. |
| IWRAM | No resident bytes. (H8) The IWRAM overlay's room group (3,648 B of ARM, section 3; 3,248 B after H9): the lamp pools' floor and wall rows, the glow spans' run copy and the LUT copy loops, loaded by `plat_room()` |
| `.bss` / `.data` | About 34 B for room.c and 27 B for hcmd.c (budget 64 B) |
| OBJ VRAM | None of its own. obj.c's four squash slots fill it, and obj.c only copies OAM while a squash shows, so the cursor and the held ghost are bitmap items. |
| THUMB ROM | About 18.3 KB, of which about 3.7 KB is harness hooks (H1: the shell frame table) |

**Speed** (libmgba, game ROM, `harness fps`, drawn frames per second;
`room-test` swaps in fixed layouts because this ROM keeps no HOME state
before G0):

| Scene (`home.txt`) | C-G gate | Measured |
|---|---|---|
| `home_plain`: template room, player, cursor | - (H8: >= 28) | 30.0 (room 80k cycles) |
| `home_steady`: parity room, Live, 4 chibis, 3 animated pieces | >= 28 | 29.8 (H8: 29.2-29.5) |
| `home_modal`: Score panel over the plain room | >= 18 | 20.0 (HART: 18.8; H5 18.7; H8: 18.3-18.8) |
| `home_modal_busy`: Score panel, 3 walkers still moving under it | - | 13.4 (H8: 12.0) |
| `home_rotate`: two view turns (brake + recompose) | >= 20 | 26.5 (HART: 23.2; H8: 24.1) |
| `home_cellar_full`: 8x8, 40 pieces, dim | avg >= 24 | 26.2 (H8, its lamps now lit: 4 lamp pools, 26.2) |
| `home_defend_w3`: 10 citizens + player walking, 24 lit pieces | >= 20 | **12.6 (not met; H8: 12.1-12.3)** |

**The light (H8).** `room_ref.c` defines it (HOUSING.md section 9,
`home_int.h` `RoomLight`) and `room.c` draws the same pixels (golden
lines 4, 34, 36, 40 and the light frames 53..57 required on the full,
dirty-rect and UI-restore paths):
* *Lamp pools.* `room_light_lamps()` finds the up to 4 light sources every
  frame (cheap); their tables (each floor tile's, each back-wall tile
  column's and the corner post's level) are refilled into `RBig.light` only
  when the lamps, the level or the turned size change (`light_key`), and the
  shell key follows them (a full recompose when a lamp is switched). A
  floor row with lamps is laid out tile segment by tile segment in a stack
  buffer at the screen's parity and copied once (`floor_row_lit`: room_ref
  draws j-major, i-minor, so the upper half of a tile always wins the 2 px
  it shares with the lower half before it); a lit wall tile is rewritten
  column by column on top of the ambient strip copy, from the two walls'
  strips at full light kept in `stage` as indices into their few colours
  (`wall_strips15`, `RBig.wpal`; rebuilt after a squash used the stage),
  one halfword per 2 px column. Pieces take the level at their view box's
  centre (their LUT key).
* *Glow and panes.* A facing's class spans (furn.h: between the row
  offsets and the rows) are read only when they change something: a glow
  span below full light, a pane at dusk or night (`RItem.lt`: `RIL_CLS`, a
  window's `RPANE_*`). Then the run goes through the piece's LUT into the
  buffer as usual and the spans are patched there through their own LUT (a
  full-light one; a pane LUT that maps every index to the sky's shade by its
  luma), before the one copy; the night panes' stars come from
  `room_pane_star()` per pixel. LUT slots a run still uses are pinned
  (`lut_pin`). By day and at full light nothing of this runs: the class
  lists cost 13,324 B of ROM and no time.
* *Speed.* The per-row loops are ARM in the IWRAM overlay's room group
  (`IWRAM_ROOM_CODE`, `iwram_ovl.h`; `render()` loads it, a compare when it
  is in): `floor_row_lit`, `wall_lit`, `runs_glow` (the class spans without
  the night panes' stars, which stay in THUMB `runs_cls`) and `lut_copy`
  (H9 dropped `lut_copy_rev`: no piece is drawn mirrored). As THUMB in ROM the lit rows cost 2-3x the ambient pattern
  copy (a lit cellar frame 1.05 M cycles, 20.7 fps); in ARM the cellar with
  its four lamps lit runs at 26.2, as before H8 with its lamps off. The
  cellar layout's lamps are on now (`test_view`, as `home_obj_view()` has
  them), so the gate measures the lamp pools' dim case.


**Roofs (HART).** `room_ref.c` defines the roof (HOUSING.md section 9) in
screen space, row d above each wall-top column, and `room.c` draws the
same pixels (golden lines 38..43 required on the full, dirty-rect and
UI-restore paths; 44..45 `xfail` with the H2 cornice / tint frames). A
full recompose redraws the roof's ~40 visible rows, so the per-pixel code
was too slow (+36 ms on the modal's full recomposes); it is drawn from
tables built with the shell (`build_shell`) in the two 2,560 B VRAM gaps
after the mode 4 pages (byte reads as cheap as IWRAM's, halfword writes):
gap B holds the slope's repeating 16-column pattern as 8 phase rows of 6
sheared 16-byte strips (like the walls: the pattern part of a screen row
is one copy) and the chimney's rows; gap A holds every column of the
special ranges (valley, hip, dormer, skylight) stored as the screen rows
it covers. A roof row is then a few halfword DMAs (the above-the-band
dormer and spike pixels and the chimney go through `roof_runs`): about
2.5 ms per full recompose. The raycaster keeps its colormaps in the same
gaps; `r3d_invalidate()` (called when it claims the arena after the
house) now clears its signature so it copies them again, and the house
rebuilds its tables after any frame gap. The parity frames run inside the
house scene (`home.txt` enters `HOME -1` first). The roof's helpers are
`noipa`: GCC 13's IPA register allocation across this file's mixed
-Os / -O2 functions once kept the side in r1 across a call that clobbered
it.

Since H6 the cellar layout (`room.c` `test_view`) holds the bunny beanbag
on frame 0, as it was before H6 (one frame): the beanbag now sinks only
while someone sits on it and sighs for `HT_SIGH` after (`home_draw.c`
`anim_frame()`). As a 2-frame loop on the layout's beat it took the cellar
from 26.3 to 20.9 fps, under the gate; the layout's other looping pieces
still flip every 8 ticks. H6 measured: steady 29.8, modal 20.0, rotate
25.4, cellar 26.3, defend 12.4. The pieces H6 animated in real rooms (the
neon, bubble chair, kettle, record player, Mr. Flops) are in no stress
layout yet.

A full recompose of the parity room costs about 0.95 M cycles (about
57 ms): 250k for the shell, 420k for the pieces, 63k for the checksums.
Running THUMB code from ROM costs about 400 cycles of loop and DMA setup
per sprite run, which dominates the busy cases. The defend crowd redraws
about 7k px of rects every frame through the LUT path.

The remaining levers, in order:

1. LZ or aligned runs in `furn_iso.c` (H4), so every run is a DMA.
2. Drawing a covered actor only once per page.
3. Having the house scene register its modal windows with
   `plat_room_ui_opaque` before `plat_room`, which removes the per-frame
   restore under a static modal.

## 6b. The tile mode: Mode 0 world + overlay canvas (G5)

Files: `src/canvas_int.h` (layout, state, contract between the files),
`src/canvas.c` (THUMB: frame policy, flip, conversion, the 2D calls' slow
path), `src/canvas_2d.c` (ARM in the IWRAM overlay's 2D group: the canvas
pixel loops), `src/ow_tiles.c` (THUMB: the world composites, the world
map), `src/ow_obj.c` (G6: the billboards in order, as OBJs, section
6c). `overworld.c` decides per frame; `main.c` (`video_flip`), `video*.c`,
`video_loops.s`, `postfx.c`, `obj.c`, `r3d.c` and `room.c` route through
it.

**What is shown.** A frame whose first drawing call is a full-screen
`plat_overworld()` may be shown in Mode 0:

* BG2 (priority 1) is the world. Every visible cell is a 16x16
  *composite*: what the Mode 4 renderer leaves in that cell's square
  before any billboard (its ground, or its block's top rows and front face
  with the contact line, then the lifted top of a raised cell below it).
  A composite depends only on the cell, the cell below, the daylight
  level, the night / tint state and the frame of animated textures, and it
  is made from the same `lit_cached()` tiles as Mode 4, so it is the same
  bytes. 36 composites (4 tiles each) are cached in VRAM, keyed by the two
  cells' raw 8 bytes, the light / tint state and an animated-texture
  epoch; a slot the screen shows is never rewritten. The world map pass is
  skipped when the visible cells, the scroll origin and the key state hash
  to last frame's 64-bit signature. Scrolling is BG2HOFS/VOFS (the 2-pixel
  horizontal step of Mode 4 is kept).
* BG0 (priority 0) is the canvas: every 2D call after the world
  (billboards with their shadows, `owfx` particles, the HUD, dialogue,
  toasts) in 8bpp tiles, colour 0 transparent. The palette has no
  colourmap or tint entry that maps a colour to 0, and colour 0 equals
  `PAL_BLACK`, so transparency is exact. Billboards are drawn in Mode 4
  order; the raised cells that Mode 4 paints over a billboard later (the
  rows below its feet) are clipped out of it, using a lift grid of the
  visible cells. Shadows and `plat_shade_rect` read the world pixel under
  a transparent canvas pixel.
* OBJs (squash actors, title figures) stay above everything, as in Mode 4.
  Since G6 the billboards are OBJs too, between the two BGs (section 6c).
  A shake moves BG0 and BG2 by their offsets; window 0 cuts out what an
  affine Mode 4 page would leave as backdrop. `plat_post_blend`'s BLDY
  also targets BG0.

**VRAM.** Both BGs use charblock 0. `0x0000` SB 0 (canvas map), `0x0800`
SB 1 (world map), `0x1000` the composites, `0x3400` the canvas pool (392
tiles; pool tile 0 is all zero), up to `0x9600`. The raycaster's colormap
rows in the two 2.5 KB gaps (`0x9600`, `0x13600`) are never touched (it
caches them). The frame being built draws its canvas *dense* into page 1's
bytes: screen tile `t` at `0x0600A000 + 64 t`, zeroed on first touch.

**Frame kinds** (`cv.mode`):

| screen shows | this frame | how it ends |
|---|---|---|
| a tile frame | dense canvas in page 1's bytes, world composites in free slots | the used tiles are planned into the pool (identical tiles shared when they would not fit) and copied in the vblank with the two maps; if they still do not fit, or the frame zooms (`plat_post_scale`), page 1 is converted in place into a Mode 4 page (each 1,920-byte band of 30 tiles holds exactly the bytes of its 8 screen rows), and the rest of the frame is Mode 4 |
| Mode 4 page 1 | sparse canvas straight into the pool (page 0 is not shown) | shown as tiles; a pool overflow drops the frame (not presented, like a catch-up tick) |
| Mode 4 page 0, or not eligible | Mode 4, as before G5 | |

Anything that needs a bitmap page during a tile frame (`plat_view3d`,
`plat_room`, a second `plat_overworld`, a call covering half the screen
or more: the pause panel, a fade, a flash) first leaves the canvas the
same way (a dense frame converts, a sparse frame is dropped). Every path
draws the same pixels: the speed tour's frame parity (below) holds on
every scene with tiles on and off.

**Routing.** `gba_nodraw` keeps its meaning (1 = a skipped tick, the only
value `plat_draw_skipped()` reports) and gains three values: 4 = nothing
drawn yet this frame (set by `plat_begin_frame`), 2 = a tile frame, 3 = a
Mode 4 frame that measures the canvas estimate. Every 2D entry point's
fast path tests `gba_nodraw == 0` as before; the others take `vid_route()`
(`canvas.c`). `plat_rect` in a tile frame goes straight from the asm entry
to an ARM fill in the 2D group.

**The flip.** The canvas copy has to happen inside one vblank (lines
160..227); a frame that arrives late in a vblank, or after it (the VBlank
IRQ's mixer can take most of one), waits for the next vblank
(`cv_vblank_lines()`), or the top rows of the next picture would show the
previous canvas.

**When it is used.** A canvas tile costs more than the same pixels drawn
into a Mode 4 page (tile addressing, read-modify-writes, the plan and the
vblank copy), while the world saves a fixed ~11 ms. Measured on the tour:
hahaland (321 canvas tiles) 48.0 ms tiled against 43.8 ms in Mode 4,
hellvile (~200 tiles) 35.0 against 38.6. So a tile frame with more than
250 canvas tiles hands the next frames to Mode 4 (`CV_TILES_WIN`), and an
entry from Mode 4 needs the last Mode 4 frame's estimate (the tiles its 2D
calls and billboards covered, measured every 16 frames, not on every
frame: measuring takes every 2D call through the slow path) to fit the
pool and to undercut the estimate that sent it to Mode 4 by 10%, with a
backoff that doubles on each failed attempt. Measured on the G5 tour:
hellvile 204 tile frames of 208, park 141, the map walk 108; hahaland,
invaded, the story map with dialogue and the pause stay in Mode 4 (1 tile
frame, 1 conversion and no drop in the whole tour).

**After G2** (Mode 4 sprites, ground and blocks much cheaper; the canvas
still draws billboards per pixel) a tile frame loses on every map of the
tour: hellvile 35.0 ms tiled against 24.1 ms in Mode 4, park 31.4 (mixed)
against 27.0, the map walk 29.2 against 28.4. So `CV_TILES_WIN` is now 64
canvas tiles (about a HUD's worth), the entry also requires the Mode 4
estimate to be at or under it, and an estimate above it is measured again
only every 64 frames (`CV_REMEASURE_FAR`; measuring every 16 cost the
story map 0.5 fps). The tour now shows no tile frame (counted with a
temporary log line); every tour scene runs within 0.1 ms of a `TILES=0`
ROM. G6 (billboards as OBJs) measured again and kept the window (section
6c); the tile path's exactness is checked with the window forced open.

**Text.** Every text path reads glyphs through `vid_glyph_rows()`
(`video_int.h`), today the core font transposed at boot. That is the hook
for L4's per-language glyph pages: mapping a character to its language's
page there changes both display modes at once.

**Memory.** EWRAM unchanged: the tile metadata (`CvMeta`: the two map
shadows, the composite keys and hash, the copy runs, the dedupe table, the
lift grid, the animated-texture table; asserted <= 8,576 B) lives in the
lower half of the overworld's arena region, and the lit-tile cache keeps
the upper half while tiles are on (`cache_fit`). IWRAM: `cv` (212 B of
`.iwram_bss`), 228 B more resident code (routing), the 2D group 8,224 B.

Measured (libmgba, `make test-speed SPEED_BASE=` with `f010771`'s log of
the G5 tour script; the tour gained the map walk, RIGHT + DOWN held on
the park visit): all 16 scenes frame-exact (30 frames each); hellvile
37.7 -> 34.6 ms (22.7 -> 23.3 fps), every other scene within 1 ms of the
base. The plan's gates (hahaland >= 25 fps,
STORY 110 >= 25, hellvile and park >= 30) are not met: with billboards
still drawn by the CPU, the canvas costs about what the world saves. They
need G6 (billboards as hardware sprites: 24 ms of hahaland's frame) and
G7 (retained HUD panels).

## 6c. Overworld billboards as hardware sprites (G6)

File: `src/ow_obj.c` (THUMB, with its per-pixel loops in the 2D group);
`obj.c` hands it OAM entries 16..127 and the free parts of OBJ VRAM;
`canvas.c` / `video.c` call it where a tile frame leaves the canvas or
shades it. Mode 4 frames are unchanged.

**What is shown.** In a tile frame every billboard becomes one or more
8bpp OBJs of priority 1: above the world (BG2, priority 1), below the
canvas (BG0, priority 0). Pieces are 8..64 px (a 46x49 sprite is a 32 + 16
by 32 + 32 grid), so images up to 255 px work. Among themselves they keep
the Mode 4 order: the billboard drawn last gets the lowest OAM entry. A
piece's tiles hold the final screen pixels: the texels through
`doomo_colormap[level]`, translucency as the Mode 4 checker, the night /
view tint dither at the screen pixel (the dither phase `(x & 3, y & 3)` is
part of the key, so the pixels are Mode 4's exactly; sprite-anchored
dither is allowed by DECISIONS but not used yet, see below), and the raised
cells that Mode 4 paints over the sprite later (the rows below its feet)
cleared from the tiles. The shadow blobs stay in the canvas (they darken
the canvas, an earlier billboard's OBJ pixel or the world, and stay
transparent where a later billboard's OBJ is opaque), so does every
billboard that gets no OBJ (OAM entries, VRAM or the per-line OBJ cycle
budget ran out, or over 255 px), together with every later billboard
whose box meets one of those. A shake hides OBJs outside window 0 when a
billboard piece crosses the screen edge (the billboards move with the
picture).

**VRAM.** OBJ VRAM `0x06014000..0x06018000`: 256 tiles (the dense canvas
holds the rest of the Mode 0 OBJ area). Squash slot k owns its 4 KB region
while in use, the title figures regions 2 and 3; billboards allocate blocks
from the top of what is left. A block is one image under one key (image,
scale, flip, light level, dither phase and tint state, checker parity,
occlusion mask), uploaded when the key is new and kept LRU; the tiles the
screen shows are never rewritten (a bitmap of them is kept from the flip
on), and a squash slot that needs its region while a billboard there is
still on screen draws unsquashed for that frame.

**Leaving the canvas.** `cv_to_m4` (a pause panel, a fade, 3D, a zoom)
first draws every OBJ billboard into the canvas under its pixels, back to
front, so the conversion to Mode 4 keeps every pixel; `plat_shade_rect`
does the same for the billboards under the rect (and the later ones
meeting them) before it shades. `cv_read_px` (squash checksums) sees them.

**Memory.** The block table (32 blocks), the frame's billboards (64) and
the unit bitmap (1,700 B) live in the IWRAM overlay slot's bytes past the
2D group: scratch that exists while the 2D group is in (every tile frame
loads it); a load of the 3D group wipes it, which `iwram_ovl_loads()`
detects (the table is rebuilt; OBJ VRAM was the 3D view's anyway).
`gba.ld` asserts that the slot keeps 1,712 B past the 2D group: a 2D group
that grows into them must shrink the tables first. The OAM entries are
built at the flip into a 896 B buffer on `video_flip`'s stack and DMA'd in
its vblank (OAM keeps them until the next flip). EWRAM: 56 B less free
(the shown-tile bitmap and the state; 12,952 B free, the H11 gate of
12,352 B holds).

**Place keys.** Each block also keeps its last *place key* (a hash of
the image, the billboard's world position, map row, light, scale, flags,
dither phase, tint state and the frame's lift grid): a billboard that has
not moved finds its block by one word and skips the occlusion mask and
the content key.

**Policy and measurements.** Measured first on G5's tree (`3706744`, Mode
4 billboards still per pixel), with `CV_TILES_WIN` raised to 360: all 16
tour scenes frame-exact against that merge base; work per drawn tick
hahaland 52.4 -> 37.0 ms (17.1 -> 20.2 fps), hellvile 34.6 -> 25.3 (23.3 ->
29.4), park 37.7 -> 30.7 (22.1 -> 25.7), the map walk 37.6 -> 30.9, the
story map with dialogue 55.5 -> 50.4 (15.0 -> 17.5); invaded stayed in Mode
4. Then G2 landed (Mode 4 billboards, ground and blocks much cheaper) and
on its tree (`65209c0`) the tile frames with OBJ billboards still lose,
measured with the window forced to 360 (frame-exact there too): hahaland
35.6 ms tiled against 34.7 in Mode 4 (21.2 against 26.1 drawn fps: a tile
frame also waits for a vblank its copy fits in), hellvile 25.9 / 24.1, park
31.9 / 27.0, the map walk 32.2 / 28.4, the story map with dialogue 51.7 /
37.8. So the window stays at G2's 64 tiles and the tour shows Mode 4 frames
only, at the base's numbers (all 16 scenes frame-exact against `65209c0`,
every scene within 0.1 ms); so do the synced story scenes (STORY 155, 154
and the P0 opening: identical frames and fps). The Mode 4 estimate now
counts only the billboards' shadows (their OBJs take no canvas tiles).

What a tile frame of hahaland still pays (profile, forced tiles): the world
pass `ow_tiles_world` ~10% (every visible cell is re-keyed when an
animated texture changes frame), the canvas HUD and text ~17% (G7's
retained panels), the shadow rows ~10% with their reads, the billboard
bookkeeping and OAM ~15%, and the copy's vblank alignment. Those, not the
billboards, decide the window now; G7 should measure again and raise it.
(G7 kept it: its panels are kept in Mode 4 frames only, section 4b; a
tile frame still draws its HUD into the canvas every frame.)
Invaded cannot be tiled exactly: its 47 tinted billboards each need a
block under screen-anchored dither and overflow the 16 KB (15 spill, the
frame converts); the sprite-anchored dither DECISIONS allows (one block
per image, the native reference changed the same way) is the way there.

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

`src/save_flash.c` (logic, THUMB) and `src/flash_ram.c` (chip access,
ARM, interrupts off while the chip is in command mode). The chip routines
must not run from ROM (the cartridge bus is busy with the flash commands),
but since G0 they no longer sit in IWRAM for good: they are one position
independent ARM block in ROM (`.text.flashram`, `__flash_ram_start` ..
`__flash_ram_end`, 1,092 B; `gba.ld` asserts <= 1,152 B), and every
`plat_save_*` entry point copies it onto its own stack (`FL_RUN`, a
0.07 ms DMA) and calls the copy (`FL()`, `flash_int.h`). That gave the
main stack its 1,092 B back (6,320 -> 7,412 B); a save's stack high-water
is 2,520 B (1,568 B without a save). Saves are byte-identical and as fast
as before (`test-coresave`: the 58.6 KB save 7,696 ms in the emulator,
both builds).

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
  arena above the scene scratch's claim (section 3: 14.7 KB beside the
  largest 12 KB claim, 26.4 KB with none; it claims the arena from the
  renderer caches), less than a slot, so no RAM copy of a save ever
  exists. `save_game()` sizes and
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
* With `HARNESS_VHASH_SHOTS=1` in the environment, `vhash-seq` also writes
  every hashed frame as `SHOTS/vh_TICK.png` (G5): two ROMs' frames of a
  failing parity window can then be compared pixel by pixel.
* Script format = the core native runner's: `wait N` (logic ticks, read
  from the mailbox, so scripts behave the same however fast the ROM
  renders), `press BTN..`, `hold BTN.. N`, `shot NAME`, `log TEXT`,
  `expect-log TEXT`, `expect-scene NAME`, `type` (warning only), plus GBA
  extensions `down`/`release`, `frames N` (raw video frames), `fps
  [LABEL]`, `audio-reset` / `audio` / `expect-audio MIN`, `profile N
  [FUNC]`, `vhash`, `vhash-seq`, `sync` (below). The LABEL of `fps` /
  `expect-fps`, `vhash` and `vhash-seq`, and the TEXT of `log`, are the
  whole rest of the line (before G1 only the first word was printed).
* Frame hashes (G1): `vhash [LABEL]` prints the FNV-1a of the frame mGBA
  put out (240x160 as BGR555, after BG / OBJ composition, blending and
  the affine scale), so it means the same in any display mode;
  `vhash-seq N [LABEL]` hashes the frames of the next N logic ticks (the
  ROM draws every tick meanwhile: mailbox `draw_all`, no catch-up ticks)
  and prints a sequence digest plus `TICK:HASH` per frame, TICK being the
  tick that drew it. `tools/speedtable.py --compare` matches two logs
  tick by tick (section 4, "Speed pass 4"). Only `vhash-seq` compares
  across ROMs: `vhash` hashes the last frame presented, which depends on
  the ROM's speed (catch-up ticks).
* `sync on [TICK]|off` (G1, game ROM): tick-exact scripts. The game loop
  waits at its top while the mailbox's `hold_at` equals its tick count
  (`gba_harness_hold()`, `hcmd.c`, running posted commands meanwhile);
  every wait moves `hold_at` on and runs the CPU an event slice (under a
  scanline) at a time until the ROM holds again. Keys and commands then
  land on the same tick in every ROM however fast it draws; without
  `sync` a wait ends on a video frame and may overrun by a tick. The
  first hold is at absolute tick TICK (default: the next tick; an error
  if the ROM is already there), so a script whose unsynced boot wait
  overruns differently in two ROMs still reaches the same ticks (the
  speed tour: `wait 40`, `sync on 61`). In sync mode `shot` and `vhash`
  first finish the video frame (the ROM stays held), and `profile`
  releases the hold while it samples. A ROM from before G1 has no hold:
  `sync on` and `vhash-seq` refuse it.
* `vhash-seq N` fails the run unless each of the N ticks of its window
  presented exactly one frame; its summary line is `vhash-seq LABEL: N of
  N frames, ticks T0-T1, seq HASH`.
* `--no-fps-gates` (G1): `expect-fps` prints a missed floor ("under the
  floor MIN (gates off)") and goes on instead of failing; for running a
  newer tour script on an older ROM (`make -C gba test-speed
  SPEED_GATES=0`).
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
* House commands (H2, game ROM): `room-parity ARG [INDEX [xfail]]` hashes
  a parity frame (bits 12-19 of ARG: the golden line of a shell frame of
  `core/tests/room_shells.h`, 5 and up; otherwise the classic layout in
  view `ARG & 3`, night `ARG & 0x10`; path `(ARG >> 8) & 15`: 0 full, 1
  dirty rects, 2 UI restore) and compares it with line INDEX of
  `--golden FILE` (`core/tests/golden/room_ref.txt`). `xfail` marks a
  frame the GBA does not draw yet: a mismatch is printed, a match fails
  (so the mark is dropped once it passes); `room-test MODE`
  swaps the house test layouts into `SCENE_HOME` (0 off, 1 steady, 2
  cellar, 3 defend crowd); `expect-fps MIN [LABEL]` is `fps` that fails
  below MIN drawn frames a second.

`make -C gba test`:

| target | what |
|--------|------|
| `test-et` | engine-test ROM, `scripts/enginetest.txt`: boot, division self-test, DirectSound + PSG audio, 16 screenshots (courtyard, blocks, path/tower, hall door, stairs, skylight look-up, step faces, mezzanine look-down, third person x2, low corridor, hell room sprites, hell third person x2, overworld x2), frame-rate log lines |
| `test-save` | engine-test ROM twice with the same `--save` file and SELECT held at boot: first run "no previous data / write ok", second run "persisted ok" (both 30 KB slots verified byte for byte, slot 1 erased and rewritten) |
| `test-game` | game ROM, `scripts/smoke.txt`: boot, title, START, menu presses, screenshots in `build/shots/game` |
| `test-coresave` | game ROM, `scripts/coresave1.txt` then `coresave2.txt` with the same `--save` file: the core's streamed saves (worst-case island in slot 1, a too-big save refused in slot 0, a story island in slot 0), loads checked by town CRC, then persistence across a reboot |
| `test-home` | game ROM, `scripts/home.txt` (H2): `plat_room` parity with the room_ref golden on every path (15 classic frames, 15 shell-frame checks, 12 roof checks and 8 open-kind checks (H5: lip, backdrop, front-edge fences) required, 31 `xfail` until H2), then `SCENE_HOME` fps gates (section 6a) and a profile, screenshots in `build/shots/home` |
| `test-ow` | game ROM, `scripts/owsprite.txt` (G2): 2,400 random sprites drawn by `ow_sprite` and by the G1 baseline's per-pixel blit (`src/owtest.c`, mailbox command `GBA_HC_OW_SPRITE_TEST` = 16, posted 150 cases at a time) must leave identical back buffers (section 6) |

`make -C gba test-speed` (G1, not part of `test`): game ROM,
`scripts/speed_tour.txt`: every target scene's fps with an `expect-fps`
floor, a shot and a 30-tick `vhash-seq`, in `sync on`; the log goes to
`build/speed_tour.log` (`SPEED_LOG=`), the shots to `build/shots/speed`,
and `tools/speedtable.py` prints the table. With `SPEED_BASE=LOG` (another
ROM's tour log, made with the same script) it also prints that ROM's fps
and fails unless every scene hashed the same ticks with identical frames;
`SPEED_SCRIPT=FILE` plays another tour script and `SPEED_GATES=0` only
reports the floors (the merge-base recipe, section 4, "Speed pass 4").

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
seconds. The overworld view also swaps the IWRAM code overlay's 2D group
in and the next 3D map swaps the 3D group back (`ET ovl:` lines; section
3, G4).

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
