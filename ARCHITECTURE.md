# DOOMOGACHI LIFE in HELLvile Park! — Architecture

One shared **portable C99 game core** runs the whole game. Two console
front-ends render it, and a native runner tests it:

| Target | What | Toolchain | Output |
|---|---|---|---|
| `gba/` | Real Game Boy Advance ROM. Mode 4 8bpp; a **height raycaster** with real 3D elements (variable floor/ceiling heights, step faces, textured floors/ceilings, look up/down, jumping, lifts, doors, z-sorted billboards); first- and third-person; PSG + DirectSound audio; 128 KB flash save | `arm-none-eabi-gcc` + newlib (apt `gcc-arm-none-eabi`), our own crt0 / linker script / header fixer | `gba/build/doomogachi.gba` |
| `ps1/` | Real PlayStation disc. GPU + **GTE true-3D polygon engine** (same maps as real geometry, perspective, subdivision against affine warp, gouraud + fog, 3D overworld camera, optional low-poly player model); SPU audio; memory card saves | PSn00bSDK (built from source) + `mipsel-linux-gnu-gcc` (apt) | `ps1/build/doomogachi.exe`, `.bin/.cue` |
| `core/native/` | Headless runner: software 8bpp framebuffer, scripted input, PNG screenshots, a reference software raycaster for `plat_view3d` | host `gcc` | `core/build/runner` |

The design is in [docs/GAME_DESIGN.md](docs/GAME_DESIGN.md). The binding
contract is the set of headers in `core/include/doomo/`.

## Repository layout

```
core/include/doomo/*.h   THE CONTRACT. Every module's public API + data layouts.
core/src/*.c             core implementation (one owner per file, see below)
core/art/*.c             art generators, compiled natively into artgen
core/tools/              host tools written in C (artgen, ...)
core/gen/*.c             GENERATED const data (art, roster, maps). Committed.
core/data/roster/        human-editable source for the 1000 premade citizens
core/data/maps/          ASCII map sources
core/native/             native platform (runner + PNG writer + soft raycaster)
core/tests/              native unit/integration tests
core/Makefile            make -C core {test,runner,gen,check-gen}
core/sources.mk          CORE_SRCS list used by gba/ and ps1/ builds
gba/                     GBA platform layer + Makefile + libmgba headless harness
ps1/                     PS1 platform layer + CMakeLists + iso.xml + harness
tools/*.py               python3 generators (stdlib only): roster_build, mapc, ...
.github/workflows/       CI: core tests, GBA ROM, PS1 disc, releases
docs/                    design, building, modding the roster
```

## Hard rules for core code

1. **C99, no floating point, no malloc, no stdio.** Only `<stdint.h>`,
   `<stddef.h>`, `<string.h>`. Use `fix16` and `fixmath.h`. Build with
   `-std=c99 -Wall -Wextra -Werror` on all three compilers.
2. **Deterministic.** Anything generated from a seed uses a local `DoomoRng`.
   The native, GBA and PS1 builds must produce identical results
   (`resident_fingerprint` parity test).
3. **Memory budget (GBA).** IWRAM 32 KB is for the stack and hot renderer
   code only. Big state (`doomo_town` ~110 KB, `doomo_map` 32 KB, caches)
   lives in `.bss`, which the GBA linker script places in **EWRAM**
   (256 KB). Total core `.bss` + `.data` must stay **under 200 KB**. Large
   constant data (textures, sprites, roster, maps) must be `const`, so it
   stays in ROM. Never generate big art at runtime.
4. **Per-tick cost is bounded.** One `doomo_frame()` runs at 30 Hz. On GBA
   the core logic gets about 6 ms of that, and the renderer gets the rest.
   Simulate the 1000 residents in round-robin batches. Never loop over
   all of them every tick doing heavy work.
5. **Text is ASCII** plus icon glyphs `0x80..0x9F` (`font.h`). All strings
   are `const`.
6. **Never break the contract silently.** You may *add* functions, enums
   (before `*_COUNT`) and fields to headers **you own**. If a header you
   don't own needs a change, make the smallest additive change and list it
   under "Contract changes" in your report. Never reorder save-relevant
   enums or struct fields.
7. **Weak stubs.** `core/src/stubs.c` (foundation) defines `__attribute__((weak))`
   placeholders for every cross-module function and scene. Owners provide
   the strong definitions, so the game always links while modules land.
   Link object files directly, **never via `.a` archives**: an archive
   member won't override a weak definition that is already linked. Stubs
   call `plat_log("STUB: name")`. The integration test fails if any stub
   runs during the scripted playthrough.

## Module ownership

| Owner | Headers | Implementation |
|---|---|---|
| foundation | types, fixmath, rng, font, catalog, resident, items, town, unlocks, save, input, scene, scenes, ui, game, platform*, doomo | fixmath.c rng.c font.c catalog.c resident.c items.c town.c unlocks.c save.c input.c scene.c ui*.c game.c stubs.c; core/native/*, core/Makefile, core/sources.mk, core/tests/ framework |
| art | palette, textures, sprites | palette.c (the 256 colours, colormaps, tintmaps), core/art/*, core/tools/artgen*, core/gen/art_data.c, textures.c, sprites.c |
| roster | roster | core/data/roster/*, tools/roster_build.py, core/gen/roster_data.c, roster.c |
| portrait | portrait | portrait*.c (portraits, chibi bodies, figures, fighter sprites) |
| life | sim, exorcism, pet | sim*.c, exorcism*.c, pet*.c (SCENE_EXORCISM, SCENE_PET) |
| action | map (runtime), actor, weld | map.c, maps.c (registry), actor*.c, combat*.c, weld*.c, tools/mapc.py, core/data/maps/{vape_cellar,cloud_nine_spire,arena}.txt (SCENE_COMBAT, SCENE_WORKSHOP, SCENE_WELD_GAME, SCENE_ARENA) |
| story | story, audio | story*.c, audio*.c, overworld maps hahaland/hahaland_invaded/hellvile/park_united (SCENE_STORY, SCENE_QUESTLOG, SCENE_CUTSCENE, SCENE_CREDITS) |
| creator | — | creator*.c (SCENE_CREATOR) |
| freeplay | — | fp_*.c (SCENE_TITLE, NEW_ISLAND, ISLAND, APARTMENTS, ROSTER, RESIDENT, SHOP, GRAVEYARD, NEWS, OPTIONS, SAVELOAD) |
| gba | — | gba/** |
| ps1 | — | ps1/** |
| ci | — | .github/**, Makefile, README.md, docs/BUILDING.md |

`platform.h` is shared: foundation, gba and ps1 must all agree on it.

Generated files: every generator writes **one file per producer**
(`core/gen/art_data.c`, `core/gen/roster_data.c`,
`core/gen/map_<name>.c`), so owners never collide. `make -C core check-gen`
regenerates everything into a temp dir and diffs it, and CI runs this.

## Build & test commands

```
make -C core test          # native unit tests + headless playthrough + parity
make -C core runner        # core/build/runner (see core/native/README.md)
make -C core gen           # regenerate core/gen/*
make -C gba                # gba/build/doomogachi.gba
make -C gba test           # libmgba headless harness: boot, script input, PNGs
cmake --preset ps1 ...     # see ps1/README.md -> ps1/build/doomogachi.{exe,bin,cue}
make test                  # everything that can run locally
```

The native runner:
`core/build/runner --script FILE --shots DIR [--w 240 --h 160]`.
FILE is a list of lines like `wait 30`, `press A`, `hold RIGHT 20`,
`shot name`, `type "Abc"`, `expect-scene CREATOR`. Run it at both 240x160
and 320x240 to check that layouts adapt. Every agent verifies its screens
by looking at the screenshots.

## Platform frame protocol

```
plat_init_hw(); doomo_init();
for (;;) { b = read_pad(); plat_begin_frame(); doomo_frame(b); plat_end_frame(); }
```

The core runs at 30 Hz. `doomo_frame` issues drawing in painter's order:
an optional `plat_view3d`/`plat_overworld` first, then 2D via `plat_*`.
The platform presents in `plat_end_frame`. The GBA waits for 2 vblanks,
or renders 3D at whatever rate it manages and repeats the logic tick. The
PS1 waits for 2 vblanks (NTSC) and double-buffers.

## Art direction (all teams)

**Premium, not cheap.** This is pixel art at console resolution, but it
must never look like cheap pixel art. Aim for the polish of a great
late-era GBA or PS1 game:

* **Cute, lively, constantly animated.** Nothing is ever static. Idle
  breathing (1 px squash and stretch), blinking, ear twitches, tail wags,
  hair and fur bounce, and secondary motion that follows the main motion.
  Use anticipation and overshoot on every UI pop-in. Sparkles twinkle,
  windows boing in, cursors bob.
* **Vaguely furry.** Most citizens are soft, fluffy critter-folk: cat,
  fox, bunny, bear, pup, mouse, sheep, raccoon and owl folk, plus the
  spooky species, all plush-toy round. Give them fluffy fur tufts on the
  outline (cheeks, head top, chest fluff, tail tip), big expressive animal
  ears, fluffy tails, little muzzles and button noses, and paw-like hands.
  Keep it wholesome and toy-like (tamagotchi / plush), never realistic.
* **Rendering quality.** Use 3–4 tone soft shading with hue-shifted
  shadows and warm highlights, rim light on the fur edge, and
  **coloured selective outlines** (a dark shade of the local colour; pure
  black only at the silhouette's bottom or contact points). Use soft
  dithering only where it reads as fluff. Eyes are big, glossy and
  two-highlight. Avoid flat single-colour blobs, jaggy diagonals, orphan
  pixels and noisy dithering.
* **Contrast.** Everything starts sugary and pastel: HaHaland and the
  Hillvile side are a plush paradise. That makes the invasion land hard:
  grotesque Doom-style flesh, bone, blood, lava, rust and vape-cloud
  demons. The `GORE_CUTE` option keeps even the violence adorable:
  confetti, hearts, plush stuffing.
