# BOREagotchi LIFE! on the Game Boy Advance

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
  fills the header (Nintendo logo, title `BOREAGOTCHI`, game code `BDME`,
  maker `DM`, complement check). The game ROM is a full **256 Mbit (32 MB)
  cartridge**, the most the GBA can address (0x08000000-0x09FFFFFF):
  `gbafix.py --pad-to 33554432` pads it to exactly 32 MB and
  fails the build if the code and data outgrow that (`CART_BYTES` in the
  Makefile). The engine-test ROM is padded to the next power of two.
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
| ROM | `0x08000000` | 4,203,444 B game after LS0 (padded to the 32 MB cartridge; the furniture's iso sprites are 878 KB of it) / about 1.1 MB ET | THUMB code, art, tables; WS0 3/1 + prefetch |
| EWRAM | `0x02000000` | 256 KB | 80 B harness mailbox, `.data`, `.bss` (core state, the arena `gba_arena`) |
| IWRAM | `0x03000000` | 32 KB | ARM hot code + data (`.iwram`), the code overlay slot (G4: 3D or 2D group), `.iwram_bss`, stacks |
| IRQ stack | `0x03007FA0` down | 672 B | the master IRQ handler (`crt0.s`) |
| main stack | `0x03007D00` down | >= 6 KB reserved (linker `ASSERT`) | system mode |
| VRAM page 0/1 | `0x06000000` / `0x0600A000` | 2 x 37.5 KB | Mode 4 double buffer |
| VRAM gaps | `0x06009600` / `0x06013600` | 2 x 2.5 KB | the 3D view's 16 colormap rows + tint map (behind the two Mode 4 pages; byte reads as fast as IWRAM); per frame, the overworld's tile slots and the house's roof tables; on the island hub the clean copy of the toast's 18-row band (SP3, `hold.c` `plat_strip`, docs/ISLAND_SPEED.md I11) |
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
`fps`, the 20-byte name argument, (G1) `hold_at` and `draw_all` for
the harness's `sync on` and `vhash-seq` (section 9), and (CL4) `test_flags`
at +72, which `plat_test_flags()` returns (0 on a real cartridge: the boot
code clears it; the harness writes `PLAT_TEST_NO_TIPS` once the ROM is up,
so the first-time tip cards never eat a script's presses, or 0 after a
script's `tips on`). 76 of the 80 bytes are used. The test harness reads
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
              .ovl_ui     388 B  (SP4) section .iwram_ui: the bitmap
                                 scenes' loops (the pet LCD's dot expand
                                 and egg rows, src/dots.c), loaded by
                                 plat_dots() / plat_hshapes()
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
| 17 | LS0: saved life stage + both-direction romance gate | `d642736` | 12,408 B | **12,408 B** | 0 | 6,308 B | no EWRAM / IWRAM: the stage lives in bits 4..6 of the resident record's `romance` byte (`DoomoResident` stays 96 B, `doomo_town` unchanged); ROM 4,201,244 -> 4,203,444 B (+2,200, measured on `d642736` after H9 and the 32 MB cartridge: the gate, the sweep, the version-1 save migration, the creator's Age row); `test-coresave` passes with the version-2 stream (worst-case free-play island 59,204 -> 60,244 B native, slot 1 cap 90,112 B) |
| 18 | K0: shared registry (save chunks 7 -> 12 slots, 6 placeholder scenes, `plat_ext.h`, `life_contract.h`) | `ae9d159` | 12,408 B | **12,384 B** | -24 B | 6,308 B | `save_budget.c`'s per-slot snapshot keeps one 16-bit size per registry slot: `snap` 48 -> 72 B (the plan's "0 B" missed it); the five new chunks (LIFE, PLAY, ENDS, OPT2, PROF) are weak NULL until LS1 / PA1 / E1 / U0 / MP0 (nothing written, save files byte-identical); the 6 placeholder scenes are `const` (ROM); the `plat_ext.h` defaults are gc'd until called. ROM 4,203,444 -> 4,203,604 B (+160, measured against LS0's tree `a29e375` built the same way; +160 on `d642736` too); IWRAM and the overlay slot unchanged |
| 19 | H2: room.c shell patterns, ceiling ramps, tints, skies | `16b6929` | 12,384 B | **12,384 B** | 0 | 6,308 B | no EWRAM / IWRAM: `RBig` unchanged, the room overlay group unchanged (3,248 B); `RG` (on the stack) +16 B, `floor_row_pat` a 248 B row buffer on the stack (as `floor_row_lit`); `.bss` 34 B. ROM 4,203,604 -> 4,206,996 B (+3,392: `room.o` text 24,632 -> 27,986 B). Room parity: golden lines 5..57 all required (the 32 `xfail` checks flipped), plus 15 new dirty-rect / UI-restore checks; house fps: plain 30.0 (30.0), steady 29.5 (29.5), modal 18.5 (19.0, gate 18), rotate 24.8 (24.7), cellar 26.2 (26.3; its herringbone floor now drawn), defend 12.6 (12.6) |
| 20 | LS1: news as 8 B records, the `LIFE` chunk | after `16b6929` (measured on the wave-1 tree; H2 changes no EWRAM) | 12,384 B | **13,920 B** | +1,536 B | 6,308 B | measured on the wave-1 tree (12,384 B: K0's weak tables cost 24 B against row 17). `sim_s` 7,084 -> 5,032 B (the 32 x 64 B feed is gone); the feed is now `news_s` in `sim_events.c` (388 B: 32 x 8 B records + 2 x 64 B free-text lines) plus `sim_news()`'s two rotating 64 B buffers (128 B). The plan's +1,792 B assumed story lines stored as text ids; they stay in the 2-line pool (a text id cannot be decoded at draw time on the PS1 without a CD read that evicts the side pack), so -256 B against the plan: about 488 B of the plan's 744 B slack after H11 remains. ROM 4,203,604 -> 4,205,688 B (+2,084: the template table, the formatter, the LIFE codec); news screen (life tour) 20.0 fps, work 38.9 / 41.1 ms (was 20.1, 38.3 / 40.5: formatting on draw); `test` and `test-coresave` pass |
| 21 | E0: endings / content pack / VM contracts (`ending.h`, `pack.h`, `JMAT`, `SHOW`, `TXF_RAW` user packs) | `2526e58` | 13,920 B | **13,856 B** | -64 B | 6,308 B | `txt.c`'s user pack table (16 pointers for text packs 112..127, `txt_user_set`): the plan's -64 B exactly; the ending / pack / `opt_mature` / `mature_adult_ok` weak defaults have no `.bss` and are gc'd until called; ROM 4,204,164 -> 4,204,352 B (+188: the JMAT rule, the raw-string branch in `txt_get`, `txt_user_set`; measured against `d81b95e` built the same way); IWRAM and the overlay slot unchanged; `make -C gba test` green (only the pre-existing room-parity xfails) |
| 22 | PA0: play-as contract (`playas.h`, weak defaults), the save header's byte 47 | `16b6929` | 12,384 B | **12,368 B** | -16 B | 6,308 B | `DoomoSaveInfo` gains `flags` (header byte 47, `PLAYAS_HF_*`): 32 -> 36 B, and the title (`T`) and the save / load screen (`L`) each keep two of them in `.bss` (the plan's "0 B" missed these copies); the weak `playas_*` getters are gc'd until called, only `playas_profile_flags()` is linked (save.c). ROM 4,203,604 -> 4,203,628 B (+24); IWRAM and the overlay slot unchanged; save files byte-identical (byte 47 stays 0 with the weak default) |
| 23 | CRA: creator part catalog, many-part drawing (slot hooks, macro ops), age gate (CR0/CR1/CR2/CR12) | `b080562` | 3,540 B | **3,540 B** | 0 | 6,356 B | measured on `b080562` (the rename) + CRA, and on `4851147` + CRA (3,556 B before and after) (rows 23+ continue from the tree's real figure: the rows after 22 were not ledgered, see CREATOR_PLAN CR0). No EWRAM / IWRAM: the catalog, the age table, the script registry (`doomo_part_minv` 128 B) and the 15 family parts are `const` in ROM; the tools-only shape mode has no variable on the GBA (`p_shape` is a 0 there); the PS1 part arena is not built for the GBA (it reads scripts raw from ROM). ROM 4,301,668 -> 4,314,712 B (+13,044; on `4851147` 4,299,852 -> 4,312,888) |
| 24 | CL1: toast kinds, lanes, scene-change drop, unlock news (clarity C1 + C5) | `4114eb7` | 3,532 B | **3,468 B** | -64 B | 6,356 B | "before" is the tree's real figure on `4114eb7` (CL2 landed after row 23 without a row: 3,540 -> 3,532 B, -8 B); the same figures after rebasing on `fef1ccc` (Talk T3 / T4 change no GBA RAM or ROM). `ui.c`'s toast state: each queued line's kind and post tick, this frame's lane, the last drawn rect, the per-kind counters, and the unlock news (count, timers and two pointers to the first unlock's name and kind word in the ROM tables; the line is written when it is told, so no 40 B copy), plus the story HUD band's rect and lane tick (`overworld_fx.c`, 16 B). ROM 4,317,416 -> 4,320,012 B (+2,596); IWRAM resident and the overlay slot unchanged; `make -C gba test` green |
| 25 | CL4: first-time tip cards (`tips.c`), the Field Guide (`lib_help.c`), Tips option | `5bb55e7` | 3,468 B | **3,444 B** | -24 B | 6,356 B | measured on `5bb55e7` (CL1's last commit; 3,468 B = row 24) and that tree + CL4, built the same way. `tips.c` state 15 B (14 B `.bss` + 1 B `.data`: the card, its scene, the seen word, flags), `options_store.c` +1 B (the Tips value), alignment; the Field Guide's open page (1,404 B) is a new member of the library's scene-scratch union (`lib_int.h` `LibWingData.help`, smaller than the Almanac's), the island's `hub_tip` 2 B is in its scene scratch, the harness's `test_flags` word fits the 80 B mailbox (76 B used). The seen flags ride in the save's `OPT2` chunk (+4 B, version 2): the GBA keeps no config record. IWRAM unchanged (resident 11,176 B, slot 10,384 B, stack 6,356 B); ROM 4,320,012 -> 4,335,572 B (+15,560: the tip and Field Guide text, the card and book code). `make -C gba test-tips`: island hub with the island card up 20.7 drawn fps (work 29.7 ms), after it 27.2; a Field Guide page 20.0 (work 44.3 ms: the page's wrapped text is drawn every frame); both under the 30 fps target, open |
| 26 | CRB: creator picture grid, part icons, creator speed (CR6/CR7/CR9) | `696ffa6` | 3,444 B | **3,444 B** | 0 | 6,356 B | No EWRAM / IWRAM: the grid's state, the lock memo, the icon unpack buffer, the 3,840 B locked-part compose buffer, the shown / pending preview views and the compose cache live in `CrState` in the 12,288 B scene scratch (5,992 -> 11,296 B on the GBA and the PS1); the glint switch shares the toast's byte in `ui.c` (`ui_held`), `CR_CALM` is a constant on the GBA, the solid-panel flag is a spare bit of `Pn.flags`; the 351 raw icons (140,400 B) and the grid / slicing code are ROM; the creator's retained panels reuse the overworld HUD's and island HUD's panel slots (`PANEL_COUNT` stays 8). ROM 4,335,572 -> 4,505,568 B (+169,996: icons 140 KB, grid / help / panels / slicing ~29 KB; measured on `696ffa6` and on CRB rebased onto it). Creator (profile-creator): idle 12.7 -> 30.0 fps, rows 12.9 -> 26.2, grid 7.6 -> 21.1, a value held 8.6 -> 20.5; `make -C gba test`, `test-speed` and `test-creator` pass, and all 16 speed-tour scenes' vhash-seq equal those of `696ffa6` |
| 27 | CL3: one prompt bar (C3), the pet screen's bar (C14), workshop / studio / house legends (C15) | `4114eb7` | 3,532 B | **3,188 B** | -344 B | 6,356 B | measured on `4114eb7` (the clarity CL2 tree) built the same way, before and after CL3 (on `b080562`: 3,540 -> 3,196 B). `pmemo` 168 B (`ui.c`: the last bar's key, cut flag and text, so the GBA builds nothing while a screen shows the same bar; `ui_hint`'s 160 B line is on the stack), the `unlock_find` memo `ul_fc` 96 B, `PANEL_PROMPT` in the panel table (`pn` 160 -> 180 B; the shown mask 8 -> 16 bits), and the rest small caches (the hub's NEWS badge text and its pad / language, the news ticker's line width, the pet's per-tick geometry, the bar's frame). The house's dots caption is a raw unlock bit (`HUL_RAW_DOTS`, in the save's unlock bits) and a `const` timing, so 0 B. IWRAM resident and the overlay slot unchanged (main stack 6,356 B). ROM 4,317,416 -> 4,325,324 B (+7,908: the bar builder and its tests' strings, the pad name table, the converted screens) |
| 28 | CL1 1.6f: the purse flashes on a money change | `9c6b8c3` | 3,092 B | **3,084 B** | -8 B | 6,356 B | "after" measured on `9c6b8c3` + this change (ROM 4,511,992 B); the change itself measured on `38a362d`: 3,444 -> 3,436 B, ROM +272 B, so `9c6b8c3` alone is 3,092 B (row 27's 3,188 B was measured on `4114eb7`). `fp_common.c` keeps the last money seen, two 16-bit ticks and the flash state (9 B, 8 B packed); the hub keeps no frame while the purse blinks (32 ticks after a change). `make -C gba test` passes (on `38a362d` + this) |
| 29 | SP6: concert loops in the IWRAM UI group (scale-2 images, word checkers, beams) | `7729ada` | 3,540 B | **3,540 B** | 0 | 6,356 B | no EWRAM, no resident IWRAM: `vid_blit2_arm`, its rows, `vid_checker_ui` and `vid_beam_ui` are 1,868 B of the overlay slot's UI group (SP4's group; the slot stays the 3D group's 10,384 B); ROM .text +3,056 B, .rodata +36 B; speed tour frame-exact (16 scenes) |
| 30 | SP5: held dialogue backdrop, loading fades, governor resume | `87271d8` | 3,540 B | **3,468 B** | -72 B | 6,296 B | measured on `87271d8` (CRA, row 23, changed no EWRAM); the band's state 14 B (`hold.c`), `gba_big_epoch` 4 B, the dialogue hold's state in `overworld.c` 19 B, `ui_dialog.c` 5 B, `scene.c` 4 B, the loading fade 9 B (`loadfade.c`, `gba_vbl_hook`), the red map's governor budget 12 B; the band itself borrows the arena (the overworld's lit-tile cache) and needs no RAM of its own. IWRAM resident +60 B (the VBlank IRQ's hook call): main stack 6,356 -> 6,296 B (>= 6,144). ROM +~3 KB |
| 31 | SP7: the exorcism's kept backdrop and retained stage (`docs/EXORCISM_SPEED.md`) | `7729ada` | 3,540 B | **3,348 B** | -192 B | 6,356 B | `stage.c` (34 B: the pages' serials and overlay boxes), the altar's outline rects (100 B), `backdrop.c` / the scene's pass state (~58 B); the stage's records and the true-name cache (~1.5 KB) live in the exorcism's scene scratch. IWRAM resident unchanged (11,176 B); the overlay's room group 3,248 -> 3,588 B (`stage_rows`, the partial-row restore; the slot is sized by the 3D group). ROM 4,280,388 -> 6,593,468 B (+2,266,084: the 59 baked church backdrops, `build/gen/backdrops.bin`; the rest code). Exorcism frames identical to `7729ada` (`vhash-seq` of every phase) |
| 32 | SP8: Combat detail (governor, held pace, catch-up ticks), HUD badges as panels, faster `r3d_up2`, ARM 64-bit helpers | `950c9bd` | 2,404 B | **2,332 B** | -72 B | 6,308 B | rebased onto the shared branch after rows 26-31: "after" measured on `950c9bd` + SP8 (2,332 B free, main stack 6,308 B, ROM 6,823,116 B), "before" is after + SP8's own -72 B / +12 B stack (the figures below were measured on `696ffa6` before the rebase). `.bss`: the resolution governor 12 B, the panel table +3 entries (+60 B: `PANEL_COMBAT_FACE/HEALTH/AMMO`; `panel.c`'s shown mask 16 bits). IWRAM: `__aeabi_lmul` and the 64-by-32 `__aeabi_ldivmod` path +124 B resident; `r3d_up2` (rewritten, +~80 B) moved from the resident section into the 3D overlay group, and `r3d_tint_pass` from the 3D group to `r3d.c` (ROM), so the slot is 10,384 -> 10,352 B and the main stack 6,356 -> **6,368 B**. The 3D group must stay at least 10,304 B (the 2D group's `ow_obj.c` tail, `BB_TAIL_BYTES` 1,712 B; the linker asserts it). resident 11,176 -> 11,196 B. Measured on `696ffa6` (row 25's tree) and that tree + SP8, built the same way. ROM 4,335,572 -> 4,336,900 B (+1,328) |
| 33 | S2-1: the modal hold (`ui_modal_hold`, docs: "Speed wave 2: S2-1" below) | `6c1d937` | 2,332 B | **2,292 B** | -40 B | 6,308 B | measured on `6c1d937` + S2-1 (ROM 6,826,460 B): the hold's state in `ui.c` (key, state, window rect, the toast band's rows: ~24 B), the library's detail signature (8 B), the studio's held flag and the fair title's tier (1 B each); no IWRAM, no buffer (the two Mode 4 pages are the copy; a toast's band goes to `plat_strip`'s page gaps). PS1: compiled out (`ui.h`), resident and overlay slot byte-identical (66 KB below the stack top with the boot FMV, the same as without S2-1) |
| 34 | S2-2: the 3D view held under a card (`viewhold.c`, docs: "Speed wave 2: S2-2" below) | `af4eef9` | 2,292 B | **2,268 B** | -24 B | 6,308 B | measured on `af4eef9` + S2-2: the held view's state in `viewhold.c` (key, rect, rows per area, options, shade level: 20 B) and the level select's held flag (1 B, padded); the copy itself lives in VRAM that a frame without a 3D view does not use (OBJ VRAM + the two Mode 4 page gaps), no EWRAM buffer; IWRAM resident and the main stack unchanged, the pack / unpack loops in the overlay's UI group (2,720 B, which the 3D group's 10,352 B slot holds). PS1: the hold is not built there (`combat_int.h`, `levels_select.c`), resident RAM 96 B smaller than `af4eef9` with the boot FMV (66 KB below the stack top): the column-window text cache (+192 B) paid for by `text_emit` at `-Os` (it runs on cache misses only) |
| 35 | S2-3: the Prize Wheel (tables in ROM, the held frame; docs: "Speed wave 2: S2-3" below) | `8c950fa` | 2,276 B | **2,276 B** | 0 B | 6,308 B | the held tick's state (the last full frame's tick, line, face, the POWER bar's flags: 14 B) is in the minigame arena (`MG_ARENA_BYTES`, claimed by the scene); the tables (`core/gen/mg_wheel_tab.c`: the atan octant 5.5 KB, the rims 3.6 KB, the pie rows 0.1 KB) are ROM; no IWRAM. PS1: the tables join the minigame overlay group (`ovl_plan.py`; 87 -> 96 KB, the slot is the house's), resident RAM byte-identical (66 KB below the stack top with the boot FMV) |
| 36 | S2-6: the Unlocks gallery's grid as a retained panel, the counted owned unlocks (docs: "Speed wave 2: S2-6" below) | `9c9fb45` | 2,276 B | **2,276 B** | 0 B | 6,308 B | measured on `9c9fb45` + S2-6: no new state in RAM (the panel borrows `PANEL_OW_OBJECTIVE`'s table entry, the last frame's key lives after the gallery's id list in the scene scratch, a tile's colours are memoised on the stack); IWRAM unchanged. PS1: the grid panel is not built (`UI_PANELS` 0) and the owned-count fast path is not either (`unlocks.c` is resident there): resident RAM 12 B smaller (`unlock_at` walks the lengths only), 66 KB below the stack top with the boot FMV |
| 37 | S2-8: the Field Guide's and the Almanac page's text as retained panels (docs: "Speed wave 2: S2-8" below) | `fc32672` | 2,276 B | **2,276 B** | 0 B | 6,308 B | measured on `fc32672` + S2-8: no new state in RAM (the panel borrows `PANEL_OW_OBJECTIVE`'s table entry; the Field Guide's preview scroll max is one byte in the wing's scene scratch, in the `help` struct's padding: `LibWingData` the same size); IWRAM unchanged; ROM 6,846,988 B (code only). PS1: nothing built (`UI_PANELS` 0), 66 KB below the stack top with the boot FMV |
| 38 | S2-7: the credits parade (one pose, sliced bodies, the body memo; docs: "Speed wave 2: S2-7" below) | `8816f24` | 2,276 B | **1,796 B** | -480 B | 6,308 B | the body look-hash memo (`portrait.c` `blh`) grows from 40 to 48 slots of 60 B on the GBA (its new `seen` byte sits in the old padding); no IWRAM. PS1: no data (memo stays 40), resident code +836 B (`portrait.c`: the body job path, until now GBA-only, and the memo's skip), 65 KB below the stack top with the boot FMV (was 66 KB) |
| 39 | S2-5: scaled images in the UI group, the Derby's racer pictures, the welding runs (docs: "Speed wave 2: S2-5" below) | `c0dc4ba` | 1,796 B | **1,796 B** | 0 B | 6,308 B | no resident EWRAM or IWRAM: `vid_blitn_arm` is in the overlay's UI group (2,720 -> 3,916 B, the slot stays the 3D group's 10,352 B); the Derby's six racer pictures (6 KB) and the weld seam's y table (640 B) live in their scenes' scratch claims; ROM 6,847,476 -> 6,850,520 B. PS1: the Derby path and `plat_ui_loops` are GBA-only, the weld runs are in the workshop overlay group; resident RAM unchanged, 65 KB below the stack top with the boot FMV |

| 40 | S2-4: the Fun Fair's kept playfield (docs: "Speed wave 2: S2-4" below) | `183f2e8` | 1,796 B | **1,788 B** | -8 B | 6,308 B | measured on `183f2e8` + S2-4: the kept-playfield mode and the key being baked in `mg.c` (5 B, padded to 8); IWRAM unchanged (no new ARM code: the restores use SP7's `stage_rows` in the room group); ROM 6.85 -> 8.00 MB (Whack's 30 baked pictures, 38,400 B each, in `backdrops.bin`). PS1: compiled out (`mg.h` macros, `ui_round_rect_inset` not built), 65 KB below the stack top with the boot FMV (`_end` unchanged by S2-4) |
| 41 | CR5a: the island hub's recorded lists in the arena (docs: "CR5a" below) | `4c0f369` | 1,788 B | **15,444 B** | +13,656 B | 6,308 B | `.bss` 259,460 -> 245,804 B: the 13 recorded rect lists of `fp_island_gfx.c` (land 640, patches 320, roads 160, glints 40, walkers 60, weather 48, deco variants 400 rects, `dv_tmp` 40; 1,715 rects, 13,720 B) now live at the top of the arena (`plat_scene_scratch` in `bigbuf.c`, owner `GBA_BIG_BY_ISL`), over the other owners' regions and the billboards that the 2D hub does not use; IWRAM and the main stack unchanged; ROM +~0.2 KB. Frames identical (below) |
| 42 | CR3: the gene lists live, the gene's new look fields (docs/CREATOR.md "CR3") | measured on `ee686c7` and that tree + CR3 | 15,444 B | **15,444 B** | 0 | 6,308 B | `.bss` 245,804 B before and after, `.iwram` 11,256 B, `.iwram_bss` 4,084 B, `.data` 816 B all unchanged (`make -C gba sizes`). The new fields ride in a resolved set (`DoomoDeep`, 21 B) that the render job already held: `Phase`, `Job.status` / `Job.expr` and the frame struct were narrowed to bytes and the job's `D` moved next to them so the job did not grow. ROM: `.text` +2,224 B, `.rodata` +20,212 B (the catalog rows and part icons of the live lists, the creator rows). |
| 43 | S3-3: the creator's held frame, regions drawn before their panels (docs: "Speed wave 3: S3-3" below) | after CR3 (`0df7284`) | 15,444 B | **15,444 B** | 0 B | 6,308 B | measured on `4c0f369` + S3-3: no EWRAM or IWRAM (the new state is 26 B in `CrState`, inside the 12 KB scene scratch the creator already claims: `pk_key`, `pk_dn`, `dn`, and the PS1-only icon tags); IWRAM resident 11,256 B and the overlay groups unchanged. ROM 8,004,784 -> 8,006,400 B (+1,616). PS1: no resident byte (the disc ends at 0x801efe60 before and after: 64 KB below the stack top with the boot FMV; the icon cache lives in the overlay slot's unused tail) |
| 44 | LV0: the level registry (`levels2.c`), the `BRCH` records chunk (7 of the 24 new levels on the GBA: 76 B working copy), the packed size classes | `0df72845` | 15,444 B | **15,460 B** | +16 B | 6,308 B | measured on `0df72845` (after CR3) + LV0 (`make -C gba sizes`, both trees built clean): `.bss` 245,804 -> 245,788, IWRAM and the overlay slot unchanged (0 B). The 76 B of `BRCH` (`levels_rec.c`: arc 48 B + 7 records x 4 B) are paid by `LvRun.size_cls` going from one byte an actor (128 B) to two bits (32 B, 96 B saved). The GBA lists the seven old levels and HB8, HB9, HB13, HB15, N1, N7, N14 (`LVG_GBA` in `lv_reg`); the 17 others are registered (their rows are in ROM) but not listed, so the select, the records and the unlock rules skip them. `make -C gba test`: 0 failures. ROM +3.6 KB (the 24 registered levels' text and tables) |

Known claimants still to land (their own packages measure and add rows):

| package | EWRAM | gate it must meet (against the last row) |
|---|---:|---|
| H11: `HOME` `.bss` on the GBA (`HOME_PERSIST`) | +3,432 B | free >= 12,352 B before it (SPEC C9) and >= 8,192 B after it; after G1, H10, L1, C1, G2, G6, G7, LS0, K0, LS1 and E0: 13,856 -> 10,424 B **Done (2026-11-01):** measured on the tree after the household tiers, 13,552 -> 10,120 B free (+3,432 B), ROM 4,213,788 -> 4,219,424 B |
| L4: Latin glyph pages | 0 (`const` rows in ROM) | |
| G3: the island's run layer | 0 `.bss` (from the arena) | free >= 9,000 B |

**L1: the language lookup** (`core/src/lang.c`, `doomo/lang.h`). Every
UI text call goes through `lang_tr()`. Its state and a 64-entry cache (16
sets of 4, 512 B) are one 528 B block in EWRAM: a string in the cartridge
ROM (`plat_text_is_const()`, `src/lang_gba.c`) is looked up once per
language and then found by its address, and a translation passed in again
(a `ui_*` call handed text another one translated) is recognised by its
address inside the table; text built in RAM is hashed and searched every time (one
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

**L2a: one text path.** The GBA's `plat_text` never translated, while the
native and PS1 ones did, so text drawn straight with `plat_text` (the
workshop's stat labels and "Materials:", the weld game's HEAT, the combat
HUD's AMMO, the free-play help marquee, whose `text_clip` drew one glyph
per call) stayed English on the GBA only. Now no platform translates:
the core does it once at each text entry point (`doomo/lang.h` "LOOKUP"),
`fp_marquee` measures and clips the translation, and `text_clip` draws
each run of glyphs between blanks in one `plat_text` call. `make test-lang`
(`harness/scripts/lang_es.txt`, part of `make test`) switches the Options
screen to Spanish and expects the Spanish help marquee and the Unlocks
header ("Logros") from `plat_text`. Same harness, seed 1, work per drawn
tick, base `d642736` -> L2a:

| screen | base | L2a |
|---|---:|---:|
| Options, English | 25.1 ms | 25.2 ms |
| Options, Spanish | 26.2 ms | **25.9 ms** (`lang_tr` 0.9% -> 0.8% of cycles) |
| Unlocks, Spanish | 65.2 ms | 65.5 ms (the header and the card marquee now translated) |

`make test-speed SPEED_BASE=` (`d642736`'s tour): all 16 scenes
frame-exact; work per drawn tick unchanged except the island hub (96.8 ->
98.2 ms in the tour, 97.0 -> 97.2 ms measured alone: which ticks get
drawn shifts; no text in its profile). EWRAM and IWRAM unchanged (12,408 B
free, 6,308 B stack), ROM +488 B.

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

### Speed package SP1: combat billboards, unlock lookups, news icons, profile names (2026-11-08)

Four exact steps from the 2026-11-08 speed audit (X5, X3, X2, T1), measured on the game ROM against its merge base
`b080562` (re-measured after the rebase; identical to the first base `4851147`) with the same harness and scripts (speed tour `make -C gba test-speed`, life tour `make -C gba test-life`,
the creator scene of `ps1/harness/scripts/perf_creator.txt` played on the ROM, `harness/scripts/entry_parity.txt`):

* **X5, combat:** `combat_build_view`'s billboard pass called `fx_dist` (a 64-bit root in THUMB) for every actor
  every frame. `combat_bb_sort()` (`core/src/combat.c`) culls with the exact squared test, sorts by a 32-bit key of
  the squared distance and takes a root only when two actors' distances agree to within ~2^-12 of a unit (the old
  sort kept equal `fx_dist` values in index order; the root decides those ties exactly). The squares are 16-bit-half
  multiplies on the GBA (no `__aeabi_lmul`). Test `x5_billboard_sort_matches_the_fx_dist_sort` (4,000 layouts, ties,
  near-ties, the cull and flash rings).
* **X3, unlocks:** `unlock_find()` scanned ~640 entries of 16 tables for each locked option a menu drew, every frame.
  It now binary-searches `core/gen/unlock_index.c` (const, ROM; written by `core/tools/ulgen.c` in `make -C core gen`,
  checked against the registry at first use: a stale index makes `unlock_find` scan as before). Not on the PS1 (its
  EXE is RAM). Test `x3_unlock_index_matches_the_scan`.
* **X2, news:** the Herald classified each visible headline with up to 33 `fp_strcasestr` scans per row per frame;
  `fp_news_icon()` (`core/src/fp_news.c`) does the same test in one pass (keywords indexed by first letter). Test
  `x2_news_icon_matches_the_strcasestr_chain`.
* **T1, profile names:** the IWRAM overlay groups share one slot, so the harness used to name a 3D-group PC after
  whichever 2D-group function sat at that address (`put_row`, `lay_of`, `canvas_row` in combat). `gba/Makefile`
  (`NM_SYMS`) now writes `build/doomogachi.ovl` next to `doomogachi.nm`, and `profile` names a slot PC by the group
  that is loaded (`ovl_cur`): the combat profile reads `r3d_flat_loop` 19.0%, `r3d_column_asm` 6.9%.

| scene | logic ticks/s | drawn fps | work avg / max ms | base fps | base avg / max ms |
|-------|--------------:|----------:|------------------:|---------:|------------------:|
| combat first person | 19.2 | **11.1** | 76.4 / 107.7 | 9.8 (16.0 ticks/s) | 83.7 / 161.1 |
| combat third person | 18.5 | **10.8** | 73.6 / 118.7 | 9.8 (16.2 ticks/s) | 81.3 / 100.7 |
| options | 30.0 | 30.0 | 24.9 / 25.9 | 30.0 | 26.3 / 27.1 |
| creator (new citizen) | 28.2 | **14.4** | 50.6 / 69.3 | 12.9 (23.8 ticks/s) | 57.6 / 108.1 |
| life: news | 30.0 | **30.0** | 30.9 / 34.3 | 20.0 | 39.7 / 43.1 |
| every other scene of both tours | | unchanged (+-0.2 ms) | | | |

Profiles (`profile`, game ROM): combat `fx_dist` 13.8% -> 0 (`combat_bb_sort` 3.1%); creator `unlock_find` 13.0% ->
under 1%; news `fp_strcasestr` 29.7% -> `fp_news_icon` 3.7%. RAM: EWRAM and IWRAM unchanged (`.bss` 257,764 B,
`.iwram` 11,176 B); ROM +4.5 KB (the index 3.9 KB).

Parity: 15 of the speed tour's 16 hashed windows are identical to the base's. The island hub's window differs
because it starts 200 ticks after the hub's entry and the hub keeps per-*drawn*-frame state (the I10 slices and
hold, see the G2 note above): the base skipped other catch-up ticks there. Hashed from each scene's first tick
(`vhash-seq` with every tick drawn: `harness/scripts/entry_parity.txt`, compared with `speedtable.py --compare`), combat first and third person (210 ticks),
options (90), the creator through eight list and tab moves (190), the New Island screen, the hub (120) and the
news with its cursor and page (130) are all identical to the base's; so are all 1,810 screenshots of the native
runner scripts at both resolutions.

What is left is outside SP1: combat is the r3d floor and column loops (~30% of the frame; G-C1 / G-C2), the creator
is rects and chibi renders (X1), and the news screen now holds 30 with ~2 ms to spare.

### Speed package SP4: pet LCD, life menus, LYRICS (2026-11-08)

Speed audit G-P1 + G-L1 + G-S1, all exact (the same pixels): native per-frame framebuffer hashes (`runner
--frame-hash FILE`, new) of every runner script and journey at 240x160 and 320x240 match the tree before (226 runs,
200,435 frames after the rebase onto the clarity work), and the SP4 tour's 13 `vhash-seq` windows matched on the ROM
before the rebase.

* **G-P1, pet (`core/src/pet_scene.c`):** the 64 x 44 LCD is drawn into a dot buffer (one value a dot, 2,816 B in
  `pet_gfx.c`'s scene scratch) and goes out once a frame through `plat_dots` (`plat_ext.h`): the GBA expands it in
  one ARM loop (`src/dots.c`, s = 2: a pair of dots is one word, stored on both pixel rows); native and PS1 draw runs
  of rects with the dot-gap rule (`pet_dots_rects`, test `pet_lcd_rects_match_per_dot`). The sprite and its shadow
  are composed once into a dot overlay; the shell's three eggs (480 rows) go to `plat_hshapes` (one ARM loop into
  the same `vid_rect` fills in the same order); the backdrop leaves out the biggest rect inside the rim egg; the
  bezels skip the rect the LCD covers; the last four rendered pet images are kept (a render was a 5 ms hitch); under
  the full-size Boredex the LCD (and the bezels where they fit under it) are not drawn.
* **G-L1, life menus:** apartments draw the sky only beside the tower; roster and resident leave the backdrop out
  under their big window (`ui_backdrop_hole`, inset 4 px from its r = 4 corners).
* **G-S1, LYRICS (`core/src/studio_words.c`):** each line's three colour strings are built a syllable at a time
  (checked against the old function on 400,000 random lines).

The ARM loops are the IWRAM overlay's new UI group (`.ovl_ui`, 388 B, section `.iwram_ui`, `IWRAM_UI_CODE`):
the pet uses none of the other groups. EWRAM unchanged (the buffers are scene scratch).

| scene (SP4 tour, `harness/scripts/sp4_tour.txt`) | drawn fps | work avg / max ms | before SP4 (fps, avg ms) |
|-------|----------:|------------------:|---------:|
| pet main | **30.0** | 28.3 / 32.0 | 12.8, 54.6 |
| pet cursor moving | 29.5 | 28.7 / 48.6 | 12.7, 56.2 |
| pet health | 29.7 | 30.1 / 41.9 | 12.0, 56.8 |
| pet Boredex | **30.0** | 22.2 / 24.5 | 9.1, 82.9 |
| pet-care boss | 28.6 | 28.7 / 48.3 | 13.3, 53.0 |
| life roster | 29.8 | 30.8 / 44.4 | 24.2, 36.2 |
| roster cursor moving | 19.8 | 41.4 / 92.2 | 17.4, 45.4 |
| life resident | 29.3 | 31.1 / 39.8 | 24.1, 36.5 |
| life apartments | **30.0** | 30.6 / 33.7 | 24.3, 35.2 |
| studio LYRICS | **30.0** | 32.4 / 33.8 | 25.2, 34.6 |

"After" is the rebased tree (with the clarity and SP1 / SP2 work under it); "before" is the SP4 base `7729ada`, so
the roster and resident gains are partly SP2's. Speed tour (`make -C gba test-speed`): pet 28.5 drawn fps (its
window includes the scene's entry). Left: the roster's cursor moves (19.8: the list rows and chibis redraw; G-L1's
retained window body is not done), the resident just under 30 (29.3), the pet's cursor and boss frames with a
48 ms worst frame (a new pet image render).

### Life tour (M0, `make -C gba test-life`)

The free-play screens the life sim lives on, in the game ROM
(`gba/harness/scripts/life_speed.txt`, a Hellvile Park Classic island
with seed 1): each screen settles for 200 ticks and is measured over 130
(`fps`), and the press that opens it is timed with `latency` (section 9):
video frames from the press until the output first differs from the same
ROM's unpressed frames. The `expect-fps` floors are about 85% of these
numbers. Not part of `make -C gba test`, like the speed tour. The press
budgets of the same journeys are native (docs/GAME_DESIGN.md 4.1).

| Screen | logic ticks/s | drawn fps | work avg / max ms | opening press | latency |
|---|---|---|---|---|---|
| island hub | 12.5 | 7.5 | 97.0 / 103.8 | (cursor DOWN / UP) | 13 / 9 frames (218 / 151 ms) |
| news (L) | 30.0 | 20.1 | 38.3 / 40.5 | L on the hub | 7 frames (117 ms) |
| island menu (Town Hall) | 10.8 | 6.4 | 110.5 / 131.5 | A on the hub | 13 frames (218 ms); its cursor 11 (184 ms) |
| Citizens (roster) | 30.1 | 25.0 | 35.6 / 52.0 | R on the hub | 13 frames (218 ms); its cursor 3 (50 ms) |
| a citizen (resident) | 30.1 | 22.2 | 38.0 / 54.7 | A in the roster | 3 frames (50 ms); its cursor 3 (50 ms) |
| Hillvile Towers (apartments) | 28.5 | 20.8 | 38.6 / 60.6 | A on Browse homes | 8 frames (134 ms); its cursor 6 (101 ms) |

The island hub and its location menu are the slow ones: they draw the
whole bitmap map every tick (about 100 ms of work, 6.5-7.5 fps, and the
logic falls to 11-12.5 ticks/s), so a press there shows up 150-220 ms
later; every screen pushed over the island runs at 30 logic ticks/s and
answers within 50-135 ms. (Measured on G7's tree; a cursor's latency
moves by a frame or three with where its press lands in the 30 Hz tick.) G8 (the island on the tile backend) and LS-IN
(the press latch) are the packages that move these numbers.

### Speed tour 2 (SA2, `make -C gba test-speed2`)

The scenes the speed tour and the life tour do not visit, one script per group in
`gba/harness/scripts/speed2_GROUP.txt`: `fair` (the seven Fun Fair minigames: title card, the bot on Sweet and
Hellfire), `fishcook` (fishing, the kitchen), `library` (every wing, the Field Guide), `action` (the Hell Breach
level select, intro cards, pause menus, the arena and arena-mode cards and results, the workshop and welding),
`dream`, `house` (Build, the catalog, a turn, Photo, Live, the Score panel, Hold the Door, a visit, HomeMart),
`studio` (the choir picker, STAGE, FILE, the song book, templates), `creator` (all nine tabs and each tab change,
the grids, swatches, the START menu), `life` (the new-island load, family dashboard and add-member wizard, play-as,
every Options page, Save / Load and a save, care popups, the shops, news tabs, graveyard, Unlocks, the picker) and
`story` (first-boot questions, title -> New story, the quest log's six tabs, the cutscenes, the credits). Each window
is `fps start` / `wait N` / `fps GROUP: NAME` after `seed 1` and a scene command, with `sync on`, so the numbers
repeat exactly. `make -C gba test-speed2 [SPEED2="fair house"]` runs them with `--worklog` and prints one table per
group (`tools/speed2.py table`); `make -C gba profile-speed2` adds a PC profile after every window
(`gba/tools/profile_report.py`). Reports by default; the results and the fix packages are in the speed audit 2
(SPEED_AUDIT2.md of the speed wave 2). A window a landed package raised closes with `expect-fps MIN LABEL` instead of
`fps LABEL` (S2-1 onwards); `make -C gba test-speed2 SPEED2_GATES=1` fails a run under any of those floors
(`tools/speed2.py` reads both closings, so the PS1 and Dreamcast conversions are unchanged).

Every `fps` line is now followed by `harness: gaps LABEL: vblanks-per-frame histogram 2:N 3:N ... max M (ms) at
tick T`: the presented frames by the vblanks each one stayed on screen (2 = 30 fps, 3 = a 50 ms frame, 4+ = 67 ms
and more), counted from the mailbox's `drawn` word. Read it rather than the work per tick: a tick of 34-38 ms of work
can still flip on its second vblank when it ends inside the vblank lines (`video_flip`), so "work over 33.4 ms" over-
counts dropped frames, and a run of them is what the histogram shows as 3s. The same scripts play on the PS1
(`ps1/harness/speed2.sh`, docs/PS1.md) and the Dreamcast (`dc/harness/speed2.sh`).

### Speed wave 2: S2-1, the modal hold (2026-11-08)

The speed audit 2's cause C1: a modal (a card, a popup, a menu) over a scene whose picture does not change while it
is up, but the scene beneath was re-composed and re-dimmed 30 times a second (house catalog 57.7 ms a tick, creator
START menu 70, library cards 43-66, studio popups 45-62, shop quantity 60, care popup 58, fair title cards 56-61).

**How.** One core helper, `ui_modal_hold(KEY, ok, x, y, w, h)` / `ui_modal_hold_end()` / `ui_modal_full(KEY, ok)`
(`core/include/doomo/ui.h`, `core/src/ui.c`). A scene's draw asks first; `ok` is its own "the modal is up, fully
popped, nothing under it moves". After one full frame with `ok` was presented, the next drawn tick calls
`plat_hold_frame(2)` (the pause panel's P2/P3 path, `gba/src/hold.c`: the shown Mode 4 page is DMA-copied into the
other one, so both carry the settled, dimmed frame) and from then on the scene draws only its modal: every UI shade
is cut to the window rect (`ui_shade_clip`) and `ui_window_shadow` skips the shadow of a window with exactly that
rect, so the window repaints over itself with the same pixels and nothing is dimmed or shaded twice. A dialogue box,
a tip card, an overlay scene drawn above (`scene_drawn_above`, SP5), a scene change (`scene.c` resets the state), a
tile frame, or `ok` going 0 (a cursor move that changes something outside the window, the modal closing) go back to
the full frame; the next settled frame is held again. Toasts may come and go over a held modal: the marked frame's
toast band is copied with `plat_strip` (SP3) before the toast draws, and each held tick puts it back before the
modal and the toast draw again (not in the house: its roof tables share those VRAM gaps, `ui_modal_no_band`; its
hold waits for the toast to go). Callers ask on every drawn tick (`ok` 0 when the modal is closed); a draw that
bails out (`plat_clear` on a lost scratch) releases first.

**Where.** The house Score panel and the list modals (catalog, themes, blueprints, rooms; the room renderer is not
called, so the room sees a frame gap and recomposes when the menu closes; the HUD bands repaint, they are opaque),
every library detail page (bestiary, almanac, weapons, items, trophies, gallery, Field Guide; page turns hold too,
`lib_detail_hold`, the weapons wing's Cute / Brutal label is its signature), the studio's popups and citizen picker,
the shop's quantity popup, the care tab's Feed / Give popup, the creator's START menu (only while no portrait job is
under way), and the Fun Fair title cards (held whole until the tier changes; a new tier is one full frame).

**Pixel-changing, by the owner's speed defaults (held frames), GBA only.** While a modal is held the dimmed scene
under it stands still: the backdrop's drifting dots and hearts, the house's animated pieces in the margins, the
studio choir's bounce, the shopkeeper's bob. The fair title card's idle motion (bulbs, bunting, the host's bob, the
selected tier's bob, Hellfire's embers) stops between tier changes. The window's inside is the same picture as a full
frame on every tick: `core/tests/test_modal_hold.c` plays the shop popup, a library page turn, the house catalog and
a fair title card with holds on and off and compares the window's inside tick for tick, checks that a held tick is
the previous one everywhere outside the window, and that a toast over a held popup leaves no trace. The hold is on by
default on the GBA only (`ui_modal_hold_enable`): the PS1 compiles it out (`ui.h` macros; resident RAM and the
overlay slot byte-identical, 66 KB below the stack top with the boot FMV, the same as without S2-1), the Dreamcast and the native runner keep
drawing every frame (they hold 30 there; their pictures and goldens are unchanged).

**Measured** (libmgba, `make -C gba test-speed2`, on `6c1d937` + S2-1; drawn fps, work avg ms):

| window | before | after |
|---|---|---|
| house: catalog modal / cursor move | 15.0 / 57.7 | **30.0** / 17.9 |
| house: Score panel | 20.0 / 45.0 | **30.0** / 13.1 |
| library: bestiary jar, weapons plan, items card, trophy page, gallery tiled | 15.0-20.0 / 47-66 | **30.0** / 13-26 |
| library: almanac profile / face | 15.7 / 57.0 | **30.0** / 29 (one 34 ms tick) |
| library: Field Guide page, page turn, scroll | 20.0 / 42-43 | **29.8-30.8** / 16 |
| studio: choir quick fill, member card, song book | 15.0-20.0 / 44-58 | **30.0** / 4-9 |
| studio: citizen picker / picker move | 14.4 / 61.5, 12.6 / 63.7 | 29.7 / 17.6, 28.5 / 22.3 |
| life: shop quantity popup | 13.3 / 59.7 | **30.0** / 5.5 |
| life: care popup (with the unlock toast over it) / after the toast | 14.5 / 58.3 | **30.0** / 12.0, **30.0** / 9.3 |
| creator: START menu | 12.2 / 69.9 | **30.0** / 10.0 |
| fair: the six title cards | 15.0 / 56-61 | **30.0** / 1.0 |

Logic is 30 ticks/s in all of them. Floors: those windows close with `expect-fps 28` (the picker and its move 27 /
26). PS1 (Redux, `ps1/harness/speed2.sh`): every one of these windows 30.0-30.8 before and after (the
hold is not built there). Still short of 30: the studio picker's moves (a new chibi each step: S2-7); the
add-member wizard (a `ui_choice` box over the family screen: the dialogue overlay is not held, 25 fps); the
Fishdex and the kitchen's book / result card (full animated scenes, not modals: S2-4); the house's own menus that
repaint the room live (walls); combat cards (S2-2). The care reaction and the Unlocks gallery are not modals (S2-6,
S2-7).

### Speed wave 2: S2-2, the 3D view held under a card (2026-11-08)

The speed audit 2's cause C2: the Hell Breach and arena intro cards, pause menus, results and death screens rendered
the 3D view on every frame (~80 ms of the raycaster) for a picture that stands still behind the card (the world does
not tick there), so they ran at 8.5-11 drawn fps with the logic at 15-19 ticks/s, and the level select rendered its
orbiting preview every frame (15-19 fps).

**How.** A platform hook, `plat_view_hold(op, key)` (`plat_ext.h`; weak default 0): `PLAT_VIEW_KEEP` right after
`plat_view3d` keeps a copy of the picture under a key, `PLAT_VIEW_PUT` puts it back instead of rendering,
`PLAT_VIEW_PUT_SHADED(level)` puts it back already darkened as a full-screen `plat_shade_rect` would (a card's dim).
The core's `combat_view_draw(v, still, salt, shade)` (`core/src/combat_int.h`) hashes the view (camera, viewport,
tint, texture clock, every billboard's position, image and flags) with the world's tick count and asks for a put
whenever the scene says the world stands still; the HUD and the card are drawn over it on every frame as before.
`gba/src/viewhold.c`: the raycaster draws 2-pixel columns (its planes and sprites too, and `r3d_up2` doubles the
half-resolution view), so every halfword of the view holds one pixel twice: KEEP checks that (or keeps nothing) and
packs one byte per halfword, 120 x 160 B, into VRAM a frame without a 3D view does not use: OBJ VRAM (the
raycaster's texture cache, claimed as `OBJV_HOLD` so the raycaster refills it and `obj.c` keeps its OBJs off) and the
two Mode 4 page gaps (the colormap rows, rebuilt). Any `plat_view3d`, `plat_overworld`, `plat_room` or
`PLAT_STRIP_SAVE` drops the copy first; a skipped tick (SP8's catch-up ticks build no billboards) leaves it alone.
PUT unpacks in an ARM loop of the IWRAM overlay's UI group (eight ALU steps a packed word: ~4 ms for the full
screen); the first shaded put darkens the copy itself once, so a dimmed card costs the same.

On top of the held view, S2-1's modal hold (`ui_modal_hold`) holds the whole frame where the card or menu has a
window: `combat_modal_hold` / `combat_pause_hold` (`combat_int.h`) for the pause menus (`pause_menu_draw`'s window,
`pause_menu_rect` in `ui.h`; the key changes with the camera, the HUD's weapon and the hint line), the Demon Arena's
pause and results windows, and the Hell Breach intro card once everything on it is in (t >= 26: only the banner's
band repaints, its title wave and sparkles stay inside it; `lvd_banner_band`). The kept page has no toast band
(`ui_modal_no_band`: the page gaps hold part of the held view). Exact steps beside the holds: the card dims are
`ui_dim_under_window_pop` (not under the window they pop: the same pixels, ~60% fewer shaded), the banner shades
only its rim rows (the band paints the rest), and the control card flows its lines once a frame
(`ccard_draw_above`), and big text wider than 255 px is cached in column windows (`ur_text_big`, below).

The level select renders its preview once per level change (and on each step of the slide) and holds it; where the
platform keeps the picture (the GBA) the preview's slow orbit stops (`pv_held`).

**Pixel-changing, by the owner's speed defaults (held frames), GBA only.** The level select preview no longer
orbits. While a view is held, translucent billboards (vape clouds, ghosts) keep the checkerboard phase of the frame
that was kept instead of alternating every frame (`r3d.iwram.c`). While a pause menu, the arena results or the
settled Hell Breach card is held, the HUD under the dim stands still (the face's blink, a blinking Time Trial or
challenge badge) and so does the sliding glint along window borders outside the repainted window. Measured against
`af4eef9` with `vhash-seq` (`opt 63 1` and `2` for a fixed Combat detail): the arena results match frame for frame;
the other windows differ only in those held motions, in the resolution SP8's governor picks for the baseline's slow
frames (half resolution there, the held view keeps the frame it kept) and in one loading-fade frame.

**Measured** (libmgba, `make -C gba test-speed2 SPEED2=action`, on `af4eef9` + S2-2; drawn fps, logic ticks/s, work
avg ms):

| window | before (`7e55f1f`) | after |
|---|---|---|
| level select | 18.8 fps / 30.2 / 49.9 | **29.7** / 30.1 / 33.3 |
| level select move (the slide: a render per step) | 15.0 / 30.0 / 55.9 | 16.9 / 30.8 / 46.2 |
| level select after a level | 18.0 / 30.0 / 50.7 | **29.7** / 30.0 / 33.3 |
| Hell Breach intro card | 10.2 / 18.3 / 94.5 | **30.0** / 30.0 / 7.2 |
| Hell Breach pause menu | 8.8 / 15.9 / 103.6 | **30.0** / 30.2 / 14.5 |
| Demon Arena intro card | 9.4 / 17.0 / 94.8 | **30.0** / 30.0 / 26.7 |
| Demon Arena pause menu | 8.5 / 15.5 / 100.5 | **30.0** / 30.0 / 5.5 |
| Demon Arena results | 8.6 / 15.0 / 106.5 | **30.0** / 30.0 / 11.1 |
| Boss Rush intro card / results | 11.1 / 9.8 fps | **30.0** / **30.0** (28.1 / 29.0 ms) |
| Horde intro card / results | 11.1 / 10.3 fps | **30.0** / **30.0** (28.8 / 29.7 ms) |
| Monster Mayhem intro card | 10.3 / 17.1 / 86.4 | **30.0** / 30.0 / 30.4 |

Floors: those windows close with `expect-fps 28` (the slide 15) in `speed2_action.txt`
(`SPEED2_GATES=1` gates them). Still short: the level select's slide (a new level's map and a render per step, 0.3 s)
and the level select itself at 33.3 ms average (2-3 frames of 50 ms in 4 s: its animated backdrop and UI, not the
preview). The combat windows are SP8's (14-15 drawn fps, logic 30).

### Speed wave 2: S2-3, the Prize Wheel (2026-11-08)

The speed audit 2's worst fair number: the Wheel of Fate at 6.5-8 drawn fps (work 120 ms a tick). The pie asked
`fx_atan2` for the angle of each of its ~1,500 colour spans a frame (`atan_oct`'s 64-bit products: 44%), the rim was
three filled discs ~95% hidden under the pie (~10 ms), and the whole tent, host frame and speech bubble were drawn again
under a wheel that turns every tick.

**Exact steps.** `tools/gen_wheel.py` writes `core/gen/mg_wheel_tab.c` (ROM; `make -C core gen`, `check-gen`):
`atan_oct(n, d)` for one octant of the disc (`mg_wheel_atan2()` folds a point into it exactly as `fx_atan2` does;
`test_minigames` checks every point of the disc and the fallback around it), the pie's row half-widths, and the rim
for both radii (50 at 240x160, 72 bigger) as ring rects: each circle only where the next ones leave it visible, rows
with the same piece merged (369 / 529 rects, the circles' rows are `ui_circle_dx`'s). The pie sorts each half's rays
once a frame instead of an insertion sort a row. Frames identical (`vhash-seq` of the intro, the spin and the done
state; native 240x160 and 320x240 shots byte-identical): 120 -> 54.7 ms, 8 -> 15 fps.

**The held frame (pixel-changing).** Outside the result card the only thing that moves is the wheel, and everything
it draws (bulbs, pegs, icons, the eyeball, the flapper) lies inside its rim's disc, which the rim and pie repaint
opaquely. So the wheel uses S2-1's `ui_modal_hold`: after one full frame the GBA keeps it in both pages and a tick draws
the disc only (the stand and the rim's shadow are already there): 26.9 ms. What changes with the state is painted on
two held ticks (one per page): the new speech line, then Wagerine's face (at the held frame's tick, so her frame does
not move), and the POWER bar while winding up; its rows' clean copy (`plat_strip`, saved in the full frame before the
bar) takes it away when the spin starts (the toast band copy is not kept: `ui_modal_no_band`). The result card (window,
confetti, flash, shake), a stand-in portrait job and a scene change go back to full frames. While held, the tent's
sway, the sparkles' twinkle and the host frame's bob stand still. Wagerine's talking face is prefetched before the
spin (`portrait_prefetch`).

**And a logic spike.** `progress_set` (the day's spin is marked used when it starts) walked the unlock registry with
an `unlock_at()` per entry, a 16-table walk each: ~23 ms of the spin's first tick. `check_rules` now walks the tables
directly in the same order (GBA, native, DC; the PS1 keeps the shorter walk so its resident code does not grow).

| window (libmgba, `speed2_fair`) | before | exact steps | + held frame |
|---|---|---|---|
| wheel intro | 7.9 fps, 119.9 ms | 15.4 fps, 54.7 ms | **30.0 fps, 26.9 ms** (no frame over 33 ms) |
| wheel spin | 8.0 fps, 119.2 ms | 17.1 fps, 53.9 ms | **29.9 fps, 27.2 ms** (one 67 ms frame: the result card's first) |
| result card (60 ticks) | ~8 fps | | 15.0 fps, 59.4 ms (full frames: confetti over the whole screen) |
| done (60 ticks) | ~8 fps | | 26.7 fps (full frames while the confetti lasts, then held) |

RAM: 0 B EWRAM / IWRAM (the held tick's state is in the minigame arena; 2,276 B EWRAM free, main stack 6,308 B);
ROM +~11 KB (the tables). Dreamcast (Flycast, no hold there): intro, spin, result card and done all 30.0, worst tick
14.7 ms. Floors:
`expect-fps 28` on both wheel windows of `speed2_fair.txt`. Still short of 30: the result card (S2-4's kept backdrop
with restored rects under the confetti would cover it) and the done state's first second.
### Speed wave 2: S2-6, the Unlocks gallery (2026-11-08)

The speed audit 2's cause C6: the Unlocks gallery (`fp_unlocks.c`) took 66 ms a tick (11 fps; the Paints tab 40 ms,
15 fps): `unlock_owned_count` walked all ~600 registry entries through `unlock_at`, which walked the 16 registry
slots through their volatile ROM pointers (`unlock_at` 27-37% of the frame), and the 44 tiles of the grid (two round
rects, a checker, a glyph and a padlock each, ~0.6 ms a tile) were drawn on every frame.

**Exact steps.** `unlock_at` walks the table lengths in RAM only; `unlock_owned_count` counts the owned bits whose id
names an entry (`ul_by_id`; only when `ul_init` saw no duplicate id, else the old walk; not on the PS1). A tile's
colours are looked up once a frame per category and state instead of ~11 `ui_darker` / `ui_lighter` calls a tile;
the backdrop leaves out what the grid and card windows cover (`ui_backdrop_holes`, as the studio and the creator).
**The grid is a retained panel** (`PANEL_OW_OBJECTIVE`'s entry, `plat_panel_solid`): the window is drawn whole on
every frame as before, the tiles of every row but the cursor's are put back from the shown page, and the cursor's row
is the panel's hole, drawn on every frame (the cursor tile bobs, blinks and sparkles; moving it along its row keeps
the panel). The hole reaches 4 rows above that row (the bob, the blink frame, the sparkle), so the last 2 rows of the
tiles above are drawn again in it, row-clipped (`tile_rows`, `ui_round_rect_rows` and single-row checkers in phase);
on the top row the hole covers the window's gloss rows, so its glint slides on. The key: the page (tab, scroll, the
cursor's row, layout) and the owned count. A frame whose key is new draws the whole grid before the panel (a solid
panel keeps its rect as it is at `plat_panel_end`, and the GBA draws before a panel on its fast path: drawn inside a
panel, every 2D call is checked and the capture frame took 90 ms). The cursor's sparkle on the last column is cut at
the panel's edge (`sparkle_cut`: the same pixels, so the arrows beside the grid never touch the panel).

**Same pixels.** `test_panel` plays the gallery (owned and locked tiles, paints, the cursor along and down the rows
and past the last one, a grant on the screen, tab changes) with the panel cache on and off at both resolutions: every
frame identical, 823 of 840 panels kept; and 133 runner shots a resolution of the same moves before and after S2-6
are byte-identical.

| window (`speed2_life.txt`, libmgba) | before | after (drawn fps, work avg / max) |
|---|---:|---:|
| Unlocks gallery | 11.2 fps (66 ms) | **30.0** (30.9 / 37.2 ms in the whole tour; 28.7 / 30.7 alone) |
| cursor move (along a row) | 11.3 fps | **30.0** (30.1 / 34.7 ms) |
| row moves (new) | ~11 fps | 28.9 (32.2 ms; one 50 ms frame per row change) |
| Paints tab | 15.0 fps (40 ms) | **30.0** (25.3 / 27.0 ms) |

Floors: `expect-fps 28` on the three 30 fps windows, 27 on the row moves. Still short: a row change, a scroll or a tab
change draws the grid whole once (~50 ms: one dropped frame), and the steady frame in the lower rows is ~31-32 ms (the
cursor's row and the 2 rows above it are drawn every frame), close to the budget.

### Speed wave 2: S2-8, the Field Guide and the Almanac page as text panels (2026-11-08)

The speed audit 2's S2-8: text pages re-wrapped and re-drawn on every frame (`wrap_core` + `vid_text` 15-25% of the
frame). S2-1's modal hold had already taken the Field Guide page to 30 (43 -> 16.6 ms); left were the contents
screen at 30.7 ms (2 vblanks with no margin), the Almanac's open page at 27.6 ms with one 50 ms frame (and its SEL face
at 29.3 ms, one 50 ms frame).

**The text is a solid retained panel** (`lib_keep_begin` / `lib_keep_end` in `core/src/lib_int.h`:
`PANEL_OW_OBJECTIVE`'s entry, `plat_panel_solid`). The panel's rect lies on a `ui_window`'s still body below its
gloss rows; the window is drawn whole every frame as before, and the panel draws the same window rows again first
(`ui_window_rows`) as its body, so the text lies on body (the panel rules). Not begun while the page pops in or turns
(`lib_keep_ok`).

* **Field Guide** (`lib_help.c` `page_text`): the contents' preview text and the open page's text. Key: the page,
  the language, the scroll and the rects. The scroll arrows bob, so the panel ends left of their column and they are
  drawn every frame from the scroll as last drawn (the preview's scroll max is kept in the wing's scratch).
* **Almanac page** (`lib_almanac.c` `detail`): Profile, Likes and the locked "???" page under the tabs (the tabs bob
  above it, the figure and its sparkle stay left of it). Key: the citizen's whole record (every line there reads only
  it and the id), the page, the scroll; the bio's up arrow bobs inside the panel by the last stat line, so its phase
  is in the key while it shows; the down arrow is the panel's hole, redrawn every frame over the window rows. Bonds
  (the town's ties) and the big screen's Profile / Likes (the persona bars shine, `ui_bar`) draw as before.

**Same pixels.** `test_panel` plays the Almanac page (open, scroll, face, Likes, Bonds, three citizens on) and the
Field Guide (contents moves, a page, scrolls, page turns, close) with the panel cache on and off at 240x160 and
320x240: every frame identical (564 of 584 and 783 of 804 panels kept at 240x160). `vhash-seq` of the same moves on
the GBA against `fc32672`: the Field Guide identical frame for frame; the Almanac identical outside the figure (750
frames, 0 pixels differ right of x 64): the figure's animation phases are drawn in the idle time after frames (SP2),
so with cheaper frames a new phase lands a few ticks earlier.

| window (`speed2_library.txt`, libmgba) | before (drawn fps, work avg / max) | after |
|---|---:|---:|
| Field Guide contents | 30.0 (30.7 / 32.1 ms) | 30.0 (**25.4** / 26.7 ms) |
| Field Guide page | 30.0 (16.6 / 17.9 ms) | 30.0 (**8.7** / 10.0 ms) |
| Field Guide page turn | 30.8 (16.5 / 20.5 ms) | 30.8 (**10.8** / 20.5 ms) |
| Field Guide scroll | 30.0 (15.6 ms) | 30.0 (**8.6** ms) |
| Almanac profile | 30.0, one 50 ms frame (28.6 / 33.9 ms) | **30.0, none** (25.1 / 36.4 ms) |
| Almanac face | 30.0 (29.0 / 32.3 ms) | 30.0 (27.3 / 32.5 ms) |

The Almanac's remaining work is the figure (`finish_marked`, `sh_span`, `scan_transitions`: its breath / blink / fx
phases re-rendered in slices, 14 ms frames with the figure cached, up to ~32 ms with a slice); the face window's
talking figure re-renders every 4 ticks. Floors: `expect-fps 28` added on the contents window (the others had one).
Still short in the library (not text): the lobby and jukebox (~20 fps, S2-4), the weapons wing's frames (24.9,
S2-5), the Almanac list move (25.4: the list and the preview's portrait drawn whole on a move).
### Speed wave 2: S2-7, the credits parade (2026-11-08)

The speed audit 2's worst PS1 number (the parade at 1.5-2 fps) and the GBA's second worst (5-7 fps, 470-686 ms
frames). Every marcher asked for its body in one of four walk frames: the ~34 marchers in the cull range (it reached
64 units past each screen edge, and on the PS1 / Dreamcast past what their 3D view shows) needed ~140 bodies, three
times the portrait cache's 48 blocks, so the cache drew 8-10 bodies a tick (`finish_marked`, `capsule_plain`,
`sh_span`...: ~90% of the frame), and on the Dreamcast each re-draw was a new texture (5,124 uploads, 3,538 evictions
in the measured window).

**What changed.** Each marcher (you too) shows one pose, the side view's frame 0, and marches by the hop it already had
(`credits.c` `CR_POSE`). The cull range follows the view (`view_range`: the GBA's flat screen plus half a billboard;
the PS1's and Dreamcast's 3D view, ~485 units either side of the camera plus its lead): ~32 marchers + 12 fans + you,
under the 48 blocks. A body the cache lacks is drawn by the portrait job slot over ticks and is not shown until it is
whole (`bodyspr_lazy(3)`: V11's sliced body job, until now GBA-only, on every platform; the job is due within
`PC_STANDIN` ticks like a stand-in's, so its slices run whatever `pslice` guesses of the room), and `build()` asks for
the next rank before it comes on. The body look-hash memo skips slots used this tick when it replaces one (with more
bodies a tick than slots, round-robin evicted each slot just before its next use: every body missed and redid
`res_get_look` + the hash), and has 48 slots on the GBA (+480 B EWRAM: parade 24.5 -> 26.2 fps).

| window | before | after |
|---|---|---|
| GBA credits (libmgba, `speed2_story`) | 7.2 fps, 136 ms avg, 460 ms max | **29.3 fps**, 30.2 ms avg, 45 ms max |
| GBA credits parade | 5.3 fps, 192 ms avg, 673 ms max | **26.2 fps**, 34.2 ms avg, 51 ms max (no frame over 50 ms) |
| PS1 credits (PCSX-Redux) | 10.88 fps, 33 frames of 217-283 ms | **30.00** (one 3-vsync frame) |
| PS1 credits parade | 2.01 fps, 129 frames over 50 ms, worst 550 ms | **30.00** (one 3-vsync frame) |
| Dreamcast credits / parade (Flycast) | 30.2 / 29.5, 73 ticks of 50-63 ms, 546 of 600 ticks drawn | **30.0 / 30.0**, no tick over 33 ms, 601 of 602 drawn, 131 texture uploads, 0 evictions |

Pixel-changing (SPEED DEFAULTS: stand-ins for a few ticks): the marchers no longer swing their legs (one pose,
hopping); a body appears once its job is done, so at the start of the credits the fans and the first ranks pop in
over the first second or two (GBA, PS1; at once on the Dreamcast and native), and when A fast-forwards the parade
the marchers come on a little after they would have. Native and the Dreamcast draw a body's job in the tick it starts.
`core/tests/test_portrait_s27.c`: a lazy body is never drawn at once, at most one new body a tick, each one equal to
a body drawn at once; the memo gives the right picture with 60 bodies a tick and still sees a look edit. Floors:
`expect-fps 28` (credits) and `24` (parade) in `speed2_story.txt`.

Still short on the GBA: the parade's frame is ~30 ms before any body work (the 2D overworld's tiles and sprites,
`plat_overworld` + `ow_tile16` ~20%, the credit card's and ticker's text ~10%, the body lookups ~7%), so the job's
~20 ms per body (2 bodies every 11 ticks) pushes about a third of the frames to 3 vblanks. And the GBA overworld's
sprite governor (V14, `GBA_GOV_MS_N` 29 ms) drops the far marchers while the frame is over budget, as it did before:
the GBA parade shows only the few marchers nearest the middle (pre-existing; the PS1, Dreamcast and native show all
of them). Both want the parade's frame under ~26 ms: a held overworld under the parade's text (S2-4 style) or text as
kept panels.

### Speed wave 3: S3-3, the creator (2026-11-13)

The speed audit 3's S3-3 (the old G2-CR; docs/CREATOR.md "S3-3" has the table and the detail). On the GBA the creator
is four retained panels on a still stage (CR9). Two things were still slow: every frame that captured a panel drew
inside it (every 2D call routed and checked: `pn_route`, `plat_rect_slow`, `vid_route`, 10-30 ms a capture), and
every tick redrew all four regions even when nothing moved.

* **The held frame** (the saved G2-CR work, `CR_HOLD`): after one full still frame (with a clean copy of the button
  bar's rows, `plat_strip`) the platform keeps the picture in both pages and a tick draws only the regions whose key
  changed, on two ticks (one per page). `hd_strip` is no longer reset by a skipped tick: under catch-up ticks the
  hold never began, which is why this alone gave 76-84 ms frames at first.
* **Panels drawn first** (`cr_region_pre`, S2-6's recipe): a region whose key the platform cannot keep is drawn before
  its `plat_panel_begin` on the fast path and taken as one solid panel. The state is `pk_key` / `pk_dn` (the key a
  region was begun with on the last drawn tick): the held frame's `hd_n` is zeroed by every full frame, so it cannot
  say whether the platform still has the panel (a first version drew every region first on every full frame and the
  persona / home tabs fell to 20 fps). Not the button bar: `ui_hint`'s pill is a panel of its own, and as a top-level
  one it is not put back from the page the creator's panel keeps (the bar vanished; `test_panel`'s creator test
  caught it).
* `plat_ui_loops()` before the pane's frame: the figure's 2x / 3x images take `vid_blitn` (S2-5).

Measured on this tree (libmgba, `make -C gba test-creator`, `test-speed2 SPEED2=creator`, `profile-creator`): idle
30.0 -> 30.0, grid scrolled 17.2 -> 26.8, rows 29.4 -> 30.0, a value held 22.2 -> 29.4, the Outfit picker 22.7 -> 29.1
(`creator_profile`), the hat grid at rest 27.2 -> 29.8, the try-on move 15.2 -> 26.9, the filter chip 21.7 -> 30.5, a
tab change two frames of 50 ms on four tabs, one on two, none on two (before: one of 50 and one of 67 on five of the
eight tabs). Not reached: 30 on the held grid and
the try-on (each step is a whole new bust: S3-2), one 50 ms frame a tab change (two). Settled screens are exact
(18 of 20 shots byte-identical, the other two differ by the pane's sparkle phase); in-between frames differ only inside
the preview pane (the new picture's landing and its sparkle come a tick or two later), and the first three frames of
the entry fade, which is timed by the clock.

### Where the title's time goes (`make -C gba profile-title`)

`harness/scripts/title_profile.txt` plays the game ROM to the title, lets it settle, and for two windows
(idle, then the main menu open after START) prints `fps` (drawn frames/s, average and **worst** CPU work of a
drawn tick in ms) and the harness profiler's hottest 25 functions over about 12M instructions (the emulated
cycles, wait states and halt time included). It reports and never gates; the log is `build/title_profile.log`.

Read the worst-tick figure as much as the average: a tick over 33 ms drops a frame (the pacing shows a frame
every second vblank at best), so one slow tick in a smooth screen reads as stutter even when the average is
fine (the G1 baseline was 27.7 ms average and 62.4 ms worst for the title). CI runs this and the speed tour
after `make -C gba test` in a step that never fails the job (`continue-on-error`) and prints both in the
run's Summary tab; the logs and the tour's shots are the `gba-speed-report` artifact. That is how GBA numbers
reach a session that has no emulator: ask for the Summary text of the latest run.

### The concert (SP6, `make -C gba profile-concert`)

`harness/scripts/concert_profile.txt` plays the four concert screens a player sees on the game ROM, each settled
on fixed ticks (`sync on`), measured over 120-130 ticks, shot and hashed (`vhash-seq 30`): Nana's solo (the speed
tour's concert), Rivet's band (the PS1 perf scene `perf_concert.txt`), a 12-voice choir on risers and the setup
screen. `make -C gba profile-concert` plays it twice, as it is and then with `profile N` after each window (a
profile shifts the later windows' ticks, so the hashes come from the first run only). Reports, never gates.

**Measured** (libmgba; drawn fps / logic ticks/s / work avg / max ms; `7729ada` = the tree SP6 started from; the
last column is SP6 up to `45d2e81` merged with SP2's portrait slices on a scratch tree, for an idea of the two
together):

| window | `7729ada` | SP6 | SP6 (`45d2e81`) + SP2 |
|---|---|---|---|
| solo (Nana, Lullabies: star wall) | 13.7 / 25.4 / 53.7 / 72.9 | 14.9 / 29.9 / 44.6 / 61.9 | 20.5 / 29.9 / 39.0 / 45.2 |
| band (Rivet, Doom Metal: flame wall) | 9.5 / 15.2 / 84.6 / 101.9 | 11.9 / 20.7 / 60.9 / 70.2 | 12.4 / 22.6 / 57.9 / 67.3 |
| choir (12 voices, Doom Metal) | 12.2 / 22.4 / 61.5 / 65.7 | 15.0 / 30.0 / 46.7 / 50.3 | 14.6 / 29.1 / 49.2 / 52.8 |
| setup screen | 11.8 / 20.8 / 68.8 / 84.7 | 12.7 / 23.3 / 65.9 / 81.4 | 13.4 / 25.6 / 63.4 / 72.2 |
| speed tour concert (`make test-speed`) | 13.8 / 25.3 / 53.9 / 74.4 | **15.0 / 30.0 / 43.6 / 51.9** | |

The solo with each of the 16 genres (so every LED wall show; 96-tick windows), work avg `7729ada` -> SP6 (ms):
stars 53.6 -> 44.0, spots 57.6 -> 46.5, glass 56.5 -> 47.0 (Hymns, also glass: 54.9 -> 48.1), **fields 65.2 -> 50.3**, disco 57.9 -> 49.0,
**waves 61.3 -> 45.2**, synthwave grid 56.9 -> 45.7, bats 55.6 -> 46.4, skulls 56.9 -> 49.4, **flames 70.9 ->
56.2**, gears 59.3 -> 48.7, hearts 56.6 -> 45.6, notes 48.3 -> 37.3, skyline 58.2 -> 48.6, pixels 53.5 -> 44.1.

**Where the solo's time went at `7729ada`** (per drawn tick, from temporary `gba_cycles()` marks; not in the
tree): the lead's figure 14 ms (9 of them its portrait job: one layer group of a new talk / breath / blink phase
a tick, every tick, because the 48-block cache cannot hold the figure's phases next to the crowd's 15 chibis),
the crowd 16.6 (the front row's six scale-2 chibis 9.8), static scenery 16 (backdrop 2.9, wall + frame 3.8,
sheen 1.2, speakers 2.3, floor and seats 3.6, truss 2.1), beams 5.0, karaoke and HUD 2-5. The band adds four
scale-2 chibis (10.3 ms) and its flame wall (17.7 ms), the choir the same flame wall.

**What changed (all exact: the same pixels):**
* *Scale-2 images in the IWRAM UI group* (`src/blit2.c`): `vid_blit_c`, the general scaled path, built a line and
  merged it pair by pair (2.6 ms for one 32x48 chibi). While the concert has the overlay's UI group in
  (`plat_ui_loops()`, `plat_ext.h`, called at the start of its draw), `plat_image` at scale 2 without a flip
  takes `vid_blit2`: on an even x a source pixel is one halfword stored on both of its rows, on an odd x a
  halfword holds two neighbouring source pixels and is merged only when one is transparent; the row walk is ARM
  too. The front row 9.8 -> ~3 ms, the band's four 10.3 -> ~2.2 ms. The UI group is SP4's (`gba.ld`,
  `iwram_ovl.[ch]` hunk for hunk the same, so the two packages merge cleanly there); SP6's part of it is 1,868 B.
* *Checkers a word at a time* (`vid_checker_ui`, same group): `plat_checker` merges two halfwords per
  read-modify-write while the group is in (the wall's sheen, the seats, smoke, the sky seams).
* *A light beam in one call* (`plat_beam`, `plat_ext.h`; GBA `vid_beam_ui`): every 3-row band of a beam in one ARM
  call instead of a `plat_checker` call (and its THUMB glue) per band; the core's `beam()` steps its
  `f = (y - ay) * 256 / dy` instead of dividing per band and draws the bands itself when the hook says 0.
* *The LED wall's busy shows* (`core/src/concert_gfx.c`): THUMB has no divide instruction and even `x / 10` is a
  call (~150 cycles); `ui_wave()` makes two. A wave stepper (`ws_*`, `concert_wave_step_equals_ui_wave`) gives
  ui_wave's value along a loop without dividing; waves and fields draw each layer only where no later layer
  covers it and merge neighbouring columns with the same piece; the flames step their half width, colour step
  and sway and merge row pairs whose bars repeat.
* *No division per fan, lamp, beam or confetti piece* (`DivStep`, `mod3` / `mod6`): the crowd's positions, row
  offsets and waves, the lamps' x / flash phase / colour, the two beam sweeps, and `concert_confetti`'s sway,
  fall and colour (8 divisions a piece before; the stage, the dreams and the results share it). Past 2^29 ticks
  of one show the stage divides as before.

**Exactness.** `core/tests/test_concert_gfx.c`: 576 stage frames (every genre and show, every stage theme, solo /
band / choir and the four choir layouts, lights, curtain, flashes, pyro, confetti, both resolutions) against
`core/tests/golden/concert_frames.txt`, recorded on `7729ada` before any of this; the wave stepper against
ui_wave, `mod3` / `mod6` against `%`, the quotient stepper against `/`, and `concert_confetti` against a verbatim
copy of its old body. The engine-test ROM's `et_blit2_test` (`ET blit2test ok (5600 cases)`) checks `vid_blit2`
(2,000 random images: odd and even x, clipped on every side, silhouettes), `vid_checker_ui` (3,000 rects) and
`plat_beam` (600 beams) against the old loops, and fails on a deliberate break of each. On the ROM: the four
`concert_profile.txt` hash windows and all 16 speed tour scenes (`make -C gba test-speed SPEED_BASE=`, `7729ada`'s
log) are frame-identical; the native runner's concert, dream and studio scripts give byte-identical shots at both
resolutions. In the profiles the UI group's loops show up under other groups' names at the same slot addresses
(`cv_fill`, `cv_text`, `boundary.constprop.0`, `wall_lit`: the overlay ambiguity of section 3).

**Tried and dropped:** a one-pass shadowed blit for the lead's figure (C in the UI group: slower than the two ARM
passes); composing the busy LED shows in RAM (9,180 B of the concert's scene scratch) and copying them with one
DMA a row (EWRAM byte stores from THUMB cost more than the IWRAM rect loops).

**What is left to 30 fps (33.3 ms a drawn tick):**
* the lead's figure: its portrait job (6-11 ms a tick) is SP2's to move into idle time (X1; see the last column
  above), and its two-pass shadowed blit is ~5 ms;
* the flame wall (Doom Metal: the band and that choir) is still ~13.5 ms: ~200 row pairs of THUMB math (ROM code at
  about 2.5 cycles an instruction) and ~360 bars;
* static scenery (~10 ms: sky, speakers, floor, seats, truss lattice) is redrawn every tick under the moving
  beams and crowd; a retained copy needs RAM (or the Mode 4 pages' gaps) and the beams cross it every frame;
* the setup screen's full-screen dim (`vid_shade_words` ~23% of its 66 ms).
Steps that would reach 30 but change pixels, for the owner to decide (SPEED DEFAULTS lists none of them): the
LED wall and the beams at 15 Hz (every other tick), the crowd's back rows as hardware sprites with the glow sticks
and hearts moved in front of them, a simpler flame wall (fewer, wider bars).

### Kept backdrops and the retained stage (SP7, `src/stage.c`)

A scene whose picture is a backdrop from a small key with layers over it (the exorcism's church,
`docs/EXORCISM_SPEED.md`) can ask `plat_stage_begin` (`plat_ext.h`) for it. The build bakes every
key of every family (`core/include/doomo/backdrop.h`) with the native runner (`build/native`,
`runner --bake-backdrops`) into `build/gen/backdrops.bin`, linked by `src/backdrop_blob.s`;
`plat_stage_restore` copies boxes of it from the ROM (whole rows by 32-bit DMA, partial rows by
`stage_rows`, ARM in the overlay's room group). Each page keeps the serial of the stage frame it
holds and the box drawn over it after `plat_stage_end` (`GBA_NODRAW_TRACK`: every 2D call routes
through `vid_route`, a kept panel adds its rect); a non-stage frame, a tile frame or a held / built
frame clears the page's serial. The core's stage (`core/src/backdrop.c`) then redraws only the
layers that changed. `make -C gba profile-exorcism` measures it.

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

### Speed wave 2: S2-5, scaled images, the Derby's racers, the welding bench (2026-11-08)

The speed audit 2's S2-5 (cause C4: scaled art redrawn the slow way every frame): the Vapula vignette's scale-3 demon
(`vid_blit_c` 38% of a 73 ms frame), the weapons wing's scale-3 weapon plan (17%), the welding minigame's scale-2
icon, the home vignette's scale-3 egg, and the Derby's racers (`mg_blit2` 28% of a 61 ms frame: one `plat_rect` per run
of a colour). S2-4 (static backdrops) had not landed, so this was measured and built on `c0dc4ba` alone.

**Not ROM copies.** The audit proposed pre-scaled copies in ROM drawn with the scale-1 blit. The weapon plans and the
welding icon are composed at run time (`weld_icon_image`), and the scale-1 `vid_blit` itself costs ~10 cycles a pixel
reading ROM bytes (the home vignette's full-screen wallpaper: 22 ms), so a ROM copy would not have been faster than a
good scaled loop. Instead:

* **`vid_blitn`** (`src/blit2.c`, ARM in the IWRAM overlay's **UI group**): any scale 2..4, flipped or not,
  silhouette or not, exactly `vid_blit_c`'s pixels. A source row is expanded once into a pair-aligned line (a
  per-scale store loop, no per-pixel run clamp), its words are sorted once into runs of four opaque pixels and single
  mixed words, and each of the `scale` rows it covers is then plain word copies (no read of the page) plus the mixed
  words merged halfword by halfword as before. `plat_image` (`video.c`) takes it whenever the UI group is in and the
  scale is not SP6's scale-2-no-flip case. The scenes ask for the group right before the image (`plat_ui_loops()`,
  `plat_ext.h`): `cutscene.c` (the Vapula demon, the home egg), `lib_weapons.c` (the plan), `weld_game.c` (the icon).
  `plat_ui_loops()` now refuses under a tile frame (the canvas' `cv_*` calls are 2D-group code). The Vapula demon:
  28 ms -> ~10 ms. Engine test: `blit2test` adds 700 random cases (scales 2-4, flips, silhouettes, widths to 80,
  every clip) against the verbatim old blit.
* **The Derby's racers** (`mg_derby.c`, GBA only): `dy_start` turns each racer's packed 2-bit sprite into an 8bpp
  picture in its jersey colours (6 KB in the scene scratch after the arena and the effects pool, claimed by the
  scene), and `racer_draw` draws it with `plat_image` at scale 1, one call per band of consecutive source rows (one
  for the whole sprite at full height, ~4 when squashed), the squash's source rows walked without divisions. Same
  rows as `mg_blit2` (`dy * h / dh`), colour 0 as `PAL_BLACK` like `plat_rect`. PS1 / native keep `mg_blit2`.
* **The welding bench** (`weld_game.c`, every platform): the seam's y per column is made once per weld (640 B more in
  the minigame's scratch) and the seam and the bead are drawn as runs of columns at the same y (and colour): every
  rect stays in its column, so a run is the same pixels as the columns one by one; 3 x 190 one-pixel rects -> ~3 x 40.

**Same pixels.** `vhash-seq` of every tick (draw_all from each push) against `c0dc4ba`: the Derby (500 ticks Sweet,
200 Hellfire), the home vignette, the weapons wing, the weld intro and welding identical; the Vapula vignette
identical when played on its own, and in a longer run identical but for two things this package does not draw: the
player figure's expression phases (the portrait cache works in idle time, SP2, so cheaper frames move a phase a few
ticks) and row 53's odd pixels, which `sky_bands` never writes (they keep an older frame's pixels: a pre-existing hole).

| window (`speed2_*.txt`, libmgba) | before (drawn fps, work avg / max) | after |
|---|---:|---:|
| fair: derby race sweet / mid / hellfire | 15.0 / 15.2 / 15.0 (60.6 / 55.9 / 59.7 ms) | **22.1 / 22.1 / 22.6** (38.0 / 37.5 / 37.7 ms) |
| story: cutscene vapula vignette | 12.0 (76.2 / 81.3 ms) | **16.1** (56.9 / 63.1 ms) |
| story: cutscene after a press | 17.9 (51.2 ms) | **20.0** (45.0 ms) |
| library: weapons frames | 24.9 (35.6 / 37.1 ms) | **30.0** (31.6 / 33.1 ms) |
| library: weapons plan | 30.0 (25.5 ms) | 30.0 (**21.5** ms) |
| action: weld intro card | 22.2 (37.8 / 39.2 ms) | **30.0** (28.7 / 30.0 ms) |
| action: welding | 21.5 (38.4 / 44.7 ms) | **29.9** (28.8 / 35.1 ms, every frame 2 vblanks) |

Floors (`expect-fps`): weapons frames, weld intro and welding 28, the Derby races 20, the Vapula vignette 14. The
harness names the UI group's functions in profiles now (`.ovl_ui`, they were "outside known symbols").
Still short, and why: **the Derby** (38 ms) is its backdrop (sky, grandstand, bunting, lanes: `vid_rect_narrow`,
~14% of the frame in divisions in `grandstand` / `mg_bunting` / `ui_wave`): S2-4's held backdrop. **The Vapula
vignette** (57 ms) is its sky bands and vapour puffs (`vid_rect_narrow` 25%) plus the remaining ~10 ms demon blit:
S2-4. **The home vignette** (41 -> 43 ms work, 20 fps either way) is its full-screen wallpaper (`vid_blit` 54%, scale 1)
and the figure's idle-time slices: S2-4.

### Speed wave 2: S2-4, the Fun Fair's kept playfield (2026-11-08)

The speed audit 2's cause C3 in the fair: Whack-an-Imp drew its whole booth on every frame (the striped tent, the
sign board with its bulbs and outlined text, the grass bands, nine dirt-ring holes with `fx_isqrt` rows: 57-61 ms, 15
fps) although only the critters, the mallet, the two peeking imps and a few flowers move.

**How (exact).** A fair game may split its playfield into *still* parts (what never changes for a key) and *live*
parts (`mg.h` "the kept playfield", `MgGame.kept_key` / `kept_keys`). The still parts are a backdrop family
(`DOOMO_BACKDROP_MG`, SP7's `backdrop.h`): the GBA build bakes every key with the game's own drawing code
(`runner --bake-backdrops`, `mg_backdrop_draw` in `MGK_BAKE` mode) into `backdrops.bin` in the ROM. A playing frame
asks `plat_stage_begin` for its key, copies the whole picture from the ROM (`plat_stage_restore`, ~3.5 ms of DMA) and
draws only the live parts over it, in their usual order (`MGK_LIVE`). Where the usual order draws a still part over a
live one, that part is put back from the ROM in the very spans it was drawn with: each hole's front ring and the ground
in front of it over the critter rising from it (`hole_front_kept`, down to the lowest row a critter reaches), the sign
board and its marquee bulbs over the imps peeking from behind it (`sign_kept`, `ui_round_rect_inset`). A flower that
lies on a hole makes that hole paint its ring and front again (as a full frame does). No layer bookkeeping, no dirty
rects: the frame is the kept picture plus the live parts, so it is the full frame by construction. Not kept: a shake
(`mg.ox` / `oy`), the results card, the PS1 (compiled out), the Dreamcast and native (no kept backdrop: they draw as
before). The first try, SP7's retained stage with every moving thing a layer, redrew ~21 of 27 layers a frame (the
holes' boxes overlap down each column and the dirty boxes merge), so it saved nothing; the kept playfield replaced it.

Whack's keys: the tent's five sways x the sign's three bulb phases, per tier: 30 pictures, 1.15 MB of ROM.

**Same pixels.** `test_stage` (`stage_kept_minigames_are_pixel_identical`) plays Whack with its bot (Sweet and
Hellfire at 240x160, Spicy at 320x240; READY / GO, the round, a pause, the END banner, the results) with the kept
picture on and off: every frame identical (1,314 / 1,252 / 825 frames drawn over the kept picture), and every key the
game asks for over 3,000 ticks is baked (`stage_kept_minigame_keys_are_baked`).

| window (`speed2_fair.txt`, libmgba) | before (drawn fps, work avg / max) | after |
|---|---:|---:|
| whack play Sweet | 15.1 (57.5 / 63.4 ms) | **20.9** (40.3 / 61.7 ms) |
| whack play Hellfire | 15.0 (61.5 / 69.7 ms) | **20.0** (44.9 / 69.8 ms) |

Floors: `expect-fps 19` / `18` on the two windows. **Still short of 30**: what is left is the live part itself (two
imps with flapping wings, up to four critters, the mallet, the outlined HUD text) plus the 3.5 ms copy and ~2.5 ms of
span restores. Next steps there: the imps and critters as OBJ sprites (G-K1), and copying back only what the live parts
covered two frames ago instead of the whole picture. The other fair games (Gallery 11 fps, Derby 22 after S2-5, Dodge 21-27,
Memory Hellfire 21.5) can take the same `kept_key` split; Derby scrolls (its backdrop is the rest of its frame after S2-5). The
kitchen, dreams, cutscenes, library lobby / jukebox and the studio STAGE are not done by this landing (S2-4 remains open
for them).

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

**The prompt bar (C3, `PANEL_PROMPT`).** `ui_hint` and the shared prompt bar
`ui_prompts` / `ui_prompts_at` (`ui.h`) draw their pill as a panel, once a
frame (a second bar on the same frame is drawn plainly). Its key is the bar's
text as drawn (pad names and translation included), the style and the pieces
flag. Nine panels now: `shown` is a 16-bit mask (`PANEL_COUNT <= 16`) and
`pn_route` / `box_shown` walk only the shown panels' bits. The bar's text is
built once and kept (`pmemo` in `ui.c`, keyed on the items' buttons, verbs,
the pad, the language and the width), so a screen that shows the same bar
frame after frame builds nothing. The pet screen pays for its new bar at
240x160 with its geometry worked out once a tick instead of per LCD dot
(`plat_screen_h` was 3% of the scene), the island hub with a memo of
`unlock_find` for the categories without alternate routes (the sim asks the
united-park mode every tick) and a news ticker that no longer measures its
line every tick; the house draws its bar inside its opaque bottom band
(`plat_room_ui_opaque`), so the room renderer never draws under it.
Measured on libmgba (`make test-speed`, CL3 rebased on `4114eb7` vs
`4114eb7`, both built the same way): pet 12.2 -> 13.4 drawn fps (56.8 ->
52.6 ms), island hub 24.3 -> 24.3 (22.5 -> 22.3 ms), house plain 30.0 ->
30.0 (10.6 -> 11.0 ms), options 26.1 -> 23.7 ms, every other scene within
0.3 ms or faster (first measured against `b080562` with the same result).
New `vhash-seq` (the bar is on these frames): house plain, island hub, pet;
the 12 other scenes are frame-identical. The hahaland visit's frames also
differ: its V14 governor drops far sprites by measured frame time, and a
0.1 ms layout shift moves which ticks it governs (with `GBA_GOV_MS_N=999`
both ROMs hash the hahaland visit identically, `a5dced3f`, measured on
`b080562`). EWRAM free 3,532 -> 3,188 B (ledger row 24). House gates:
modal 20.0 (gate 18), rotate 24.8, cellar 26.2, defend 12.6.

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

**Solid panels (CR9, `plat_panel_solid`).** A panel drawn over a backdrop that does not change while
its key is the same (the GBA creator's still stage) may say so right after `plat_panel_begin`: its
rects are then not recorded while it is drawn (no `add_rect` per call), and at `plat_panel_end` it
keeps one rect, its whole rect clipped to the screen, so a kept frame copies it back one span a row
(less its hole) instead of one span per row per body rect. The rounded corners and the drop shadow
come back from the front page, which holds the same pixels. The creator's tab strip, preview pane,
list window, part grid and button bar are solid; `core/tests/test_panel.c` plays the creator that
way (the native panel cache puts the whole rect back too) and every frame is identical with the
cache off. The creator's idle frame went from 22.2 to 15.6 ms of work with it.

#### P: the held pause backdrop (`docs/PAUSE_SPEED.md`, P2 + P3 + P4)

The overworld pause no longer rebuilds and dims the world every frame: the
settled frame is copied into both Mode 4 pages once, then only the window and
the title pill repaint. `make -C gba test-speed` on the same tour:

| scene | logic ticks/s | drawn fps | work avg / max ms | before fps | before avg ms |
|-------|--------------:|----------:|------------------:|-----------:|--------------:|
| overworld pause | **29.9** | **28.3** | 23.0 / 80.2 | 12.3 | 74.8 |

Every other scene's row is unchanged (hahaland 28.6, hellvile 30.0, park
30.0, invaded 14.9, concert 11.7, island hub 7.5). EWRAM 26,336 B free after
the change, IWRAM stack 6,260 B. The 80.2 ms max is the pop-in. The pause
floor in `speed_tour.txt` is 24.

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

#### D: the held dialogue backdrop (SP5, G-D1; `core/src/overworld.c` `dh_*`, `gba/src/hold.c`)

Under a dialogue box the story overworld's update does not run (`SCENE_DIALOG` is on top), so the world stands
still; only its clock-driven idle animations (prop frames, the pet's bob, sparkles, the invaded tint pulse) and
the HUD would move. Once the box has slid in and nothing else moves, `story_draw` draws the world one last time
and asks the platform to keep the band the box draws in (`plat_hold_band(PLAT_BAND_SAVE)`, plat_ext.h): rows
`[y, 160)` from 50 rows above the box (12 more when they fit: a 3-option choice window) without the box's opaque
body, which the box repaints on every frame (`ui_dialog_band()`). The next frame calls `plat_hold_frame(2)` (both
Mode 4 pages carry the shown picture, as the pause's P2) and every held frame puts the band back
(`PLAT_BAND_RESTORE`, ~16 KB of DMA, ~2.5 ms) and draws only the dialogue on it: its drop shadows land on clean
pixels, its retained panels (G7) are kept as before.

* **Pixel change** (SPEED DEFAULTS 2026-11-08): the idle animations pause while a box is up. Everything else is
  the full frame's: the native test `core/tests/test_dialog_hold.c` replays two runs (band on / off) at 240x160 and
  320x240 and requires every frame that is not held to be identical, every held frame to differ only where the
  full run itself changed since the kept frame, and the box body to be identical.
* **Falls back to the full redraw** (and holds again once it settles) for a flash, fade, shake or banner, a toast,
  any camera effect, the pet's "beep." or a gag, the pause, a scene over the dialogue, the box sliding in, a choice
  window taller than the band, a tile frame, a skipped tick, a frame built over ticks, or `expect-text`.
* **Memory:** the band lives in the arena between the scene scratch's claim and the per-frame billboards: the
  story overworld's lit-tile cache (`GBA_BIG_OW`) and the free bytes below it (about 18 KB beside the story's
  6.5 KB claim). A held frame draws no world, so the cache is borrowed; the next `plat_overworld` invalidates it
  (`hold_band_release`, it rebuilds in one frame). `gba_big_epoch` (bigbuf.c) bumps on any other claim (a save's
  stream, another region's owner): the copy is then lost and the core redraws in full. EWRAM: 14 B of state.
* **Dialogue stings:** the box now runs the camfx clock for its own `{!}` / `{cut}` effects (3 ticks). Before,
  the host's update (which ticks camfx) did not run under the box, so a sting's shake offset and white flash
  froze on their first frame until the box closed (and kept the hold off).

Measured (libmgba, seed 1):

| scene | base drawn fps | SP5 | base median / p90 / max ms | SP5 |
|---|---:|---:|---|---|
| story ch1, Mayor Giggs's portrait lines (PS1 perf scene as a GBA script) | 17.7 | **28.7** | 44.8 / 61.5 / 114.6 | **20.9** / 35.6 / 114.7 |
| speed tour "story map + dialogue" (beat 110 narration) | 29.8 | **30.0** | 29.9 / 34.4 / 36.6 | **7.2** / 10.9 / 13.1 |

The speed tour's other 15 scenes are frame-exact against the base ROM (`speedtable.py --compare`); "story map +
dialogue" differs on all 30 frames by the paused idle animations (the balloon and two sparkles). The Mayor's
remaining frames over 33 ms are portrait re-renders (SP2's slicing). Its floor in `speed_tour.txt` is 25.5 (85%).
On a scratch merge of SP2 (`fut-sp2` 920d280) with this package, the Mayor's lines run at 29.5 drawn fps, median
27.1 ms, worst 46.3 ms (9 frames over 33.4, none over 50).

#### LD: loading fades (SP5, G-LD; `gba/src/loadfade.c`)

`scene.c` calls `plat_loading()` (plat_ext.h) on every `scene_set` / `scene_push` / `scene_pop`, and the story
overworld before a map load. From then until the next flip the VBlank IRQ calls `gba_vbl_hook` (a THUMB function
in ROM, set only while armed: the IRQ itself gained one load and test): once 9 vblanks (150 ms, past the slowest
normal frame, combat's 100-160 ms) pass without a flip, it darkens the screen with the hardware fade (BLDCNT
brightness decrease on every layer, BLDY +2 a vblank, black after ~130 ms more). The flip that ends the wait shows
the new picture at the darkness reached and lifts it by 4 a flip; a frame's own darker blend wins. Registers only:
no page pixel, logic or timing changes, and nothing happens while the harness hashes frames (`draw_all`).

Real transitions measured on libmgba (the first frame's work): story -> combat 521 ms (fades: before/after sheet
in the SP5 evidence), story -> house 150 + 128 ms, title 158-176 ms, concert 140 ms, pet 90 ms (no fade under
150 ms). Not shortened here: the combat entry (weapon views built on the first frame) and the save / load
freezes, which are much longer on the GBA (`save 0` 11.7 s, `load 0` 6.7 s in the emulator for a story save; the
title's Continue runs the load in one tick) and want their own package (a busy indicator and a profile of the
codec on the GBA). The harness's `scene STORY <debug beat>` setups (1.9 s) build a whole town: they are test-only.

#### V15: the invaded governor resumes (SP5, G-V15; `gba/src/overworld.c` `gov_pick`)

A gap in the governed frames (a scene over the map, the pause, a held dialogue box, a map entry) restarted the V10
governor free, so the next ~28 governed frames drew every far sprite again and came down by 1/8 a frame. On the
red map the budget always settles at `GOV_MIN` (logged: by the 20th governed frame after an entry, and it stays
there), so a gap there resumes from the budget it left with and a first entry starts at `GOV_MIN`; the other maps
(V14) still start free. Invaded, the 45 ticks after closing the pause: 28.0 -> 30.0 drawn fps, frames over 33.4
ms 9 -> 0. The first seconds after an entry are unchanged: the budget is already at its floor during the sim's
entry burst.

What is left after a story-map entry (re-measured on `480e690`, libmgba, seed 1, `scene STORY 219`, `--worklog`;
the first window includes the harness's town setup):

| ticks after the entry | 0-30 | 30-60 | 60-100 | 100-130 | 130-200 | 200-300 |
|---|---:|---:|---:|---:|---:|---:|
| drawn fps | 11.1 | 22.6 | 24.8 | 30.0 | 30.0 | 30.0 |
| frames over 33.4 / 50.2 ms | 21 / 18 | 17 / 1 | 21 / 0 | 3 / 0 | 5 / 0 | 5 / 0 |

* **The sim's catch-up** (ticks ~40-75): 32 residents a tick brought fully up to date, a routine chosen for each
  (`update_resident_ex`, `decide_activity`); SP2's X4 makes those updates cheaper. The same pass comes back at
  **every in-game hour** (~1,150 ticks, 38 s, at the default day length): on invaded, which has no slack left (29 ms
  frames), each hour of a two-minute run gave 16 frames over 34 ms and 3 over 50 ms, and nothing in between; the
  visits (`STORY 10`, `12`: 22-28 ms frames) absorb it. What is left wants the routine choice cheaper again or
  spread (`sim.c`, X4's file), not a draw-side change.
* **The map / region name banner** (`ow_banner`, 100 ticks after every map entry and every region change) costs
  2.1 ms a frame (35,000 cycles, timer marks: the drop shadow 12,000, the pill 7,000, the text's two passes 13,500,
  the sparkles 1,500), mostly the 2D layer's per-call cost, not its place: drawn 40 rows lower, clear of every
  panel, it costs the same; with it off, ticks 60-100 run at 29.2 fps instead of 24.8. A retained middle (the
  pill's straight rows and the text as a G7 panel, the ends, shadow and sparkles drawn each frame; pixel-exact on
  the native panel tests) saved only 0.5 ms (25.8 fps) and was not kept. The banner wants a cheaper call path for
  narrow rects and shading, or a plate of OBJs (a G-area pass).

### Combat detail (SP8; G7's "3D: Faster / Better")

`OPT_3D_FAST` (`town.h`, option 63) is the Options row **"Combat
detail"** on the Performance page, shown on every platform (DECISIONS
2026-11-08): `OPT3D_BALANCED` (0, the default), `OPT3D_SMOOTH` (1) and
`OPT3D_MAX` (2). G7's saved values keep a close meaning: 0 (Better) reads
as Balanced, 1 (Faster) as Smooth. Nothing in the game logic depends on
it: only what is drawn and how often.

| mode | the GBA's 3D view | pace (`video_flip`) | effects |
|---|---|---|---|
| Smooth | half resolution always (G7's "Faster": the view at half width and height into the top left quarter of its rect, doubled by `r3d_up2`) | at most 30 fps | drop shadows only under the player (`combat_view_detail`, `combat_scene.c`) |
| Balanced | the resolution governor (G-C1, `r3d.c`): full resolution while a full-resolution frame shows at 20 fps or better, half resolution under load | full: at most 20 fps; half: at most 30 | all |
| Max detail | the same governor with a 15 fps limit: full resolution unless it falls below that | as Balanced | all |

* **The governor (G-C1, `r3d_gov_flip`).** Draw-side only, like the
  invaded map's sprite governor (V10): `video_flip` hands it every 3D frame
  with the vblanks it took. A full-resolution frame over the limit (3
  vblanks Balanced, 4 Max) switches the next frames to half resolution;
  from there it tries full resolution again only when the last frame plus
  what full resolution costs over half (r3d cycles measured at both, `3x`
  half before any full view) fits the limit, at most once every
  16 frames (doubled after every try that did not fit, up to 240). 12 B of
  `.bss`.
* **Held pace (from G9).** A 3D frame that needs more vblanks than the
  pace (counting the wait for the next vblank) moves it there (at most 4),
  and it steps back only after 8 frames in a row that fit the faster pace,
  so a view at a vblank boundary shows at an even 20 (or 15) instead of a
  ragged mix. `gba_view3d_frame` + `gba_view3d_pace` are two bytes where
  G7's `int` was.
* **The game keeps 30 ticks a second (G-C1).** After a frame with a 3D
  view, `plat_begin_frame` may run **two** catch-up ticks in a row instead
  of one (`skip_max`), so a 50-80 ms combat frame no longer slows the game
  down (the display floor of such a stretch is 10 fps instead of 15; the
  governor keeps it well above). And a catch-up tick in combat costs less:
  `combat_build_view` skips the billboards' cull, sort and `actor_image`
  when `plat_draw_skipped()` (the camera, the shake's RNG draws and the
  tints still run every tick), and `combat_draw_hud` runs only its tick
  counters (the story caption's countdown, the FPS counter). Every drawn
  frame is unchanged by that.
* **The HUD badges are retained panels** (G7's `plat_panel_*`; section 4b):
  the face badge (`PANEL_COMBAT_FACE`, keyed by the face image's `uid`, the
  sparkle's size and the hell style; not while the face bounces from a hit),
  the health box (`PANEL_COMBAT_HEALTH`: hp, armour, the low-health
  heartbeat and flash) and the ammo pill (`PANEL_COMBAT_AMMO`; not when its
  small line runs past the pill). A kept frame copies their body back from
  the front page instead of drawing their round rects, glyphs and the
  face (~2.3 ms of a frame on average; drawing no badges at all would save
  ~10 ms, most of it the face's portrait renders, see below). The face crop is 34 rows instead of 36, so it ends on the badge's
  inner window instead of running 2 rows over the outline's rounded bottom
  corners (a 2-row change of the picture on every platform; the panel rule
  "images lie on the body" needs it). `PANEL_COUNT` is 11: `panel.c`'s
  shown mask is 16 bits (+60 B of panel table). `core/tests/test_panel.c`
  plays a fight (walk, shoot, hits, low health, armour, a weapon swap,
  third person) with the native panel cache on and off: every frame is
  identical, 625 of 774 badges kept.
* **`r3d_up2`** reads two halfwords a word and writes four words to each of
  the two rows with STMDB: every halfword of the view is one 2-pixel column
  (both bytes equal), so `a a` becomes `a a a a`. Same pixels, 5.2 -> 2.6
  ms for the 120 x 80 view. It moved from the resident IWRAM to the 3D
  overlay group (only `plat_view3d` calls it), and `r3d_tint_pass` (the
  tint's once-a-frame set-up) from the 3D group to `r3d.c` (ROM), so the
  slot did not grow.
* **64-bit helpers (from G9, `div.s`).** `__aeabi_lmul` (every `FIXMUL`
  in THUMB code) is 4 ARM instructions in IWRAM instead of libgcc's THUMB
  from the cartridge, and `__aeabi_ldivmod` divides by a divisor that fits
  32 bits (the raycasts' `(CELLU << 32) / dx`) with the 32-bit core plus 32
  shift-subtract steps instead of `__udivmoddi4`. The engine-test boot
  self-test (`et_divtest.c`) checks both.
* `draw_sprites` (from G9) walks the nearest-first billboard list
  backwards into its far-to-near insertion sort (same order, equal depths
  in list order).

**Hardware scaling: evaluated, not taken.** G-C2 asked for BG2's affine 2x
of the half-resolution quarter with the HUD as OBJs. In Mode 4 the HUD
would have to fit the 16 KB of OBJ VRAM above the second page (256 8bpp
tiles, today the raycaster's texture cache), while the combat HUD covers
130-300 tiles a frame (badges, weapon with paws, texts, crosshair, boss
bar, pause / done / scope screens over it) and double buffering halves
that; the affine trick would save `r3d_up2` (2.6 ms after the rewrite
above) and cost a per-frame HUD-to-OBJ conversion and the texture cache's
move: at best ~2 ms of a ~45 ms frame, for a new display path. The badges
as panels (measured on the X5 tree: 48.1 -> 45.8 ms first person) and the
faster `r3d_up2` (5.2 -> 2.6 ms) took about that much each with no new
display mode.
A tile mode (Mode 1: BG2 affine tiles for the view, BG0 for G5's canvas)
would lift the VRAM limit, at the price of the raycaster writing a
column-major tile layout and the HUD through the canvas: a package of its
own if a future pass needs the last 3 ms.

Measured (Vape Cellar, `scene TITLE; push COMBAT 3`, 200 ticks of settling,
130 measured; libmgba 0.10.2):

| tree | mode | first person: logic / drawn fps / work avg ms | third person |
|---|---|---|---|
| base `4114eb7` | Better (old default) | 16.6 / 9.9 / 82.2 | 16.3 / 9.8 / 82.3 |
| base `4114eb7` | Faster | 21.6 / 12.0 / 58.6 | 21.0 / 12.1 / 58.0 |
| SP8 | Balanced (default) | **30.0 / 14.8** / 52.9 (max 85.6) | **30.1 / 14.8** / 49.3 (max 109.7) |
| SP8 | Smooth | 29.9 / 14.7 / 52.4 | 30.0 / 14.8 / 48.3 |
| SP8 | Max detail | 30.0 / 14.8 / 52.9 | 30.1 / 14.8 / 49.3 |
| SP8 + SP1's X5 (not landed when measured) | Balanced | 29.9 / 17.2 / 46.6 | 30.0 / 16.6 / 44.0 |
| SP8 + SP1's X5 | Smooth | 30.1 / 17.4 / 45.8 | 29.9 / 16.6 / 43.5 |
| SP8 rebased on `696ffa6` | Balanced | 30.0 / 14.8 / 52.9 | 30.1 / 15.3 / 48.6 (max 80.6) |
| SP8 rebased on `696ffa6` | Smooth | 29.9 / 14.7 / 52.4 | 30.0 / 14.8 / 48.2 |

The game now runs at full speed in combat (30 logic ticks a second,
from 16); the picture is 14-15 fps on this tree and about 17 with X5 (the
billboards' cull and sort without `fx_dist`, SP1), against 9.8. In this
fight full-resolution frames never fit 20 fps, so all three modes settle
on the half-resolution view (Max detail draws its first frames at full
resolution). Drawn frames take 40-45 ms median with X5 (0 of 75 under
33 ms), so **30 fps in GBA combat is not reached**: the half-resolution
raycaster (~14 ms column pass at 60 x 80, sprites ~3 ms, `r3d_up2` 2.6 ms),
the weapon blit (`vid_blit` ~6% of the cycles), the logic of the drawn tick
(~5 ms) and the HUD that is not a panel (crosshair, kill counter, damage
numbers) leave the frame at ~40 ms. The worst frames were the HUD face's
expression changes (a 45-150 ms portrait render in one frame): a new
expression is now drawn in slices while the face shown last stays up
(`portrait_hold`, the title's mechanism, for 8 drawn frames after a change;
DECISIONS 2026-11-08 "a stand-in for 2-4 ticks"), which took the worst
frame from 133.6 to 85.6 ms (first person). `make -C gba test-3d`
(`scripts/view3d.txt`, part of `make test`): the row, the game speed
(`expect-logic 29`) and an fps floor of 12 in every mode, and Smooth's view
made of one-colour 2x2 blocks (`expect-2x2`); the harness commands `opt ID
VALUE`, `expect-logic MIN` and `expect-2x2` are new (section 9).

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
day, dusk and night, and the 8x6 room turned. Since H2 the GBA draws all
of it (every wallpaper, floor and ceiling ramp, the five tints, the dusk
and night skies with their stars), so every golden line is required in
`scripts/home.txt`: each on a full recompose, and a sample of each kind
(patterned walls and floors, tints, the open kinds, the themed roofs) on
the dirty-rect and UI-restore paths too. No line is `xfail` any more.

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
| Walls | Wallpapers 0-3 repeat every 16 units, so a wall is one sheared 16 px strip. It is cached as 8 phase rows, so a wall's screen row is one DMA. (H2) Hearts and harlequin repeat every 32 units and stone and stars hash per tile, so wallpapers 4-7 are drawn column by column (`wall_cols`: room_ref's `wall_column`, its rects clipped to the region), after the roof's rows and before the floor's. |
| Floor | Floors 0-1 repeat every 32x16 px (upper-half tiles win the 2 px overlaps). A floor row is one or two DMAs from 16 cached pattern rows, plus at most 2 px patches on the two front edges. (H2) Floors 2-7 (tiles, herringbone, grass, flagstone, lava crack, cloud) are cut into cells and hash per tile: each row is laid out tile by tile in room_ref's order into a stack buffer, each tile in its own light, and copied once (`floor_row_pat`). |
| Ceiling, tint, sky | (H2) The cornice takes `ceil_ramp[ceiling]`. A view tint (`tint_level` >= 4) composes `doomo_tintmap` after the light into every shell colour, the roof, the lip and backdrop, the pieces' LUTs (key bit 20-23; a tinted plain piece goes through a LUT too) and the actors' colormap. The open kinds' sky follows daylight (noon, peach dusk, navy night with room_ref's 12 stars). Tint, daylight and ceiling are in the shell key. |
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
UI-restore paths; 44..45, the themed cornice and the tint, since H2). A
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

### Options volume (Refresh, `make -C gba test-optvol`)

The GBA keeps no machine config record (no `PLATF_CONFIG`: language,
volumes and the other machine-wide options ride in the save's `OPT2`
chunk), so the owner's freeze (the PS1 read and rewrote its memory card
file on every volume step) never reached the flash here. The core now
writes the record once, when the scene where it changed is left
(`opt_config_flush`); on the GBA that is a no-op. `harness/scripts/
options_volume.txt` changes the music volume 10 times on the Options'
Quick page and gates the worst presented frame (`expect-gap 51`: 3
vblanks are 50.2 ms): 2 vblanks (33.5 ms) for all 130 frames, before and
after; no flash write. Leaving the Options, the title's own re-entry
takes one 218 ms frame (unchanged).

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
  [FUNC]`, `vhash`, `vhash-seq`, `sync` (below), `latency BTN [MAX]
  [LABEL]` (M0: press BTN and count the video frames until the output
  first differs from a replay of the same frames without the press, taken
  from a save state; fails if nothing changes within MAX frames). The LABEL of `fps` /
  `expect-fps`, `vhash` and `vhash-seq`, and the TEXT of `log`, are the
  whole rest of the line (before G1 only the first word was printed).
* Profile names (SP1, T1): `--nm build/doomogachi.nm` also reads `build/doomogachi.ovl` when it is there (written
  with the `.nm` by every profile target: each IWRAM overlay symbol's address, `.ovl_*` section and name). A PC in
  the overlay slot is then charged to the loaded group's function (the ROM's `ovl_cur`), not to whichever group's
  symbol nm sorted last at that address; the profile prints "names N overlay symbols by the loaded group".
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
* `tips on|off` (CL4): every run keeps the first-time tip cards off (the
  mailbox's `test_flags`, above); `tips on` lets them show, as the native
  runner's command does (docs/GAME_DESIGN.md 4.2).
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
* `opt ID VALUE` (SP8, game ROM): `opt_set(ID, VALUE)` through the mailbox
  (`GBA_HC_OPT` = 17); fails unless `opt_get` then reads VALUE (the native
  runner and the PS1 harness have the same command).
* `expect-logic MIN [LABEL]` (SP8): fails unless the window the last `fps`
  / `expect-fps` line measured ran at least MIN logic ticks a second (the
  game itself slowing down, which drawn fps alone does not show).
* `expect-2x2 X Y W H MIN MAX [LABEL]` (SP8): counts the 2x2 pixel blocks
  (even X, Y) of a rect of the output frame that are not one colour and
  fails unless the count is in MIN..MAX (a doubled half-resolution 3D view
  has none).

`make -C gba test`:

| target | what |
|--------|------|
| `test-et` | engine-test ROM, `scripts/enginetest.txt`: boot, division self-test, DirectSound + PSG audio, 16 screenshots (courtyard, blocks, path/tower, hall door, stairs, skylight look-up, step faces, mezzanine look-down, third person x2, low corridor, hell room sprites, hell third person x2, overworld x2), frame-rate log lines |
| `test-save` | engine-test ROM twice with the same `--save` file and SELECT held at boot: first run "no previous data / write ok", second run "persisted ok" (both 30 KB slots verified byte for byte, slot 1 erased and rewritten) |
| `test-game` | game ROM, `scripts/smoke.txt`: boot, title, START, menu presses, screenshots in `build/shots/game` |
| `test-coresave` | game ROM, `scripts/coresave1.txt` then `coresave2.txt` with the same `--save` file: the core's streamed saves (worst-case island in slot 1, a too-big save refused in slot 0, a story island in slot 0), loads checked by town CRC, then persistence across a reboot |
| `test-home` | game ROM, `scripts/home.txt` (H2): `plat_room` parity with the room_ref golden on every path (15 classic frames, 15 shell-frame checks, 12 roof checks and 8 open-kind checks (H5: lip, backdrop, front-edge fences); since H2 every line required, none `xfail`), then `SCENE_HOME` fps gates (section 6a) and a profile, screenshots in `build/shots/home` |
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

## 12. The mature ROM variant (E8)

`make -C gba MATURE_PACK=/path/pack.dmpk` builds `build/doomogachi_mature.gba` (objects in `obj-mature`, so it never
touches the stock ROM). `gba/src/packrom.c` `.incbin`s the pack into `.rodata` (4-aligned) and defines
`plat_pack_blob()`; `core/src/pack.c` then opens it in place.

- Build the pack with `tools/mpack/mpack.py build MANIFEST --out pack.dmpk` using `port gba`. The Makefile runs
  `mpack.py validate --port gba --gba-default-cap` first (4 MB; pack.c itself takes up to 8 MB).
- Stills are 240x160 and stored RAW: the GBA has no scratch to decode into, so they are shown straight from the cartridge.
  mpack refuses an LZ or wrong-size still for port gba. `mpack.py example --out x.dmpk --port gba` makes the SFW example.
- Cost: 0 B EWRAM / IWRAM; the ROM grows by the pack's size (the cartridge image is 32 MB either way).
- Not yet run in mgba with a real pack (no ARM toolchain in the session that wrote it).

### CR5a: GBA EWRAM room for the creator's LOOK pool

The creator plan (CREATOR_PLAN 4.2, CR5a) needs about 8.5 KB of EWRAM for the deep parts' `LOOK` pool and the prefs.
The island hub's recorded rect lists were the biggest block that is live in one scene only, so they moved into the
arena (bigbuf.c) and EWRAM free went from 1,788 to **15,444 B** (`make -C gba sizes`: `.bss` 259,460 -> 245,804).
The 4 KB GBA-only look pool is not needed first; the CR5 package can place the `LOOK` pool in `.bss` (or in the
arena) and still keep more than 6 KB free.

* Where: the top 13,720 B of the arena (`gba_arena + 26,992 - 13,720`). That is above the scene scratch even when
  a scene claims all 12,288 B (asserted in `bigbuf.c`: `DOOMO_SCENE_SCRATCH + FP_ISLAND_SCRATCH_GBA <=
  GBA_ARENA_SIZE`), and it overlaps the billboard list and the raycaster / overworld / house regions, which the 2D
  island hub does not use. The hub's own scratch (`IslScratch`, about 12 KB) sits below it.
* How: the core already had a hook for a platform that lends scene scratch (`plat_scene_scratch`, SP3: the PS1's
  overlay slot tail). The GBA now implements it: it claims the arena as the new owner `GBA_BIG_BY_ISL` and returns
  the generation `gba_big_epoch`. Whoever else takes the arena (the raycaster, the overworld, the house, a save's
  stream, a new scratch claim) changes the epoch, and `lr_bind()` starts the lists over (they are recorded again in
  one frame; same rects). The caps are the old `.bss` caps (`LRS_*` equal `LR_*` on the GBA), so what is recorded,
  overflowed and replayed is the same as before. `dv_tmp` joined the block on the GBA (a pointer and a cap now).
* Rule for new arena writers: the island lists are only noticed as stale through `gba_big_epoch`. Any new code that writes the arena (or a billboard user without its own scene scratch claim) must call `gba_big_claim` (or claim a scene scratch) first. Today the overworld and combat billboard writers are safe because their scenes own a scratch claim, and the island re-claims its own on return.
* Cost: one re-record after a save, and after any visit of the raycaster / overworld / house. An island autosave
  (day change) re-records the lists in the next hub frame.
* Proof (libmgba, `4c0f369` against the branch, evidence in the CR5a run's scratch directory): speed tour fps
  identical on all scenes but the Town Hall menu (see next point), `profile-all` shows no fps loss (island hub 30.0 /
  14.6 ms avg, Town Hall menu 29.0 / 38.0 ms in both; the combat rows moved by timing noise only, they do not touch
  these lists); island hub `vhash-seq` 94cd2df9 before and after; the island hub shot byte-identical. The Town Hall
  menu's `vhash-seq` moved 9fe9806d -> 9b8d7163 (drawn fps 26.2 -> 25.5, avg work +0.3 ms): the screenshot differs
  only inside the menu's portrait box (x 17..55, y 79..133: the citizen's eye / blink phase); the map around it is
  byte-identical. The baseline with autosave off (`opt 10 0` after `expect-scene ISLAND`) gives the same 9b8d7163 /
  94cd2df9 as the branch (with autosave on or off), so the frames of the lists are the same and the 9fe9806d window
  is the baseline's autosave hitch moving the portrait's animation phase by a tick. Not isolated further (the
  branch's menu window does not show that shift); the Town Hall menu is bound by the portrait (X1), not by the lists.
* Test: the engine test's boot check `ET arena ok` (`gba/enginetest/et_arena.c`, run by `make -C gba test-et`): the
  region's address and alignment beside a 12 KB claim, a stable generation, a new one after the autopilot's /
  raycaster claim and after a save's stream, refusal of a block bigger than the arena. It fails on the old code (the
  weak default returns no scratch).
