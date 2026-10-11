# Dream House: technical map and rendering proposal

Covers DOOMOGACHI LIFE in HELLvile Park! on GBA and PS1: build, decorate and visit homes.

> Read-only survey of the repo at `43af439` ("Docs: GBA/PS1 memory tables after the overlay + EWRAM pass"), plus the uncommitted work of other agents in progress on 2026-10-11. Every `file:line` reference is to that tree. Numbers marked **measured** come from running the existing test binary (`core/build/tests save`, output in a scratch tmp dir) or from `arm-none-eabi-size` on `gba/build/doomogachi.elf`. All other numbers come from the docs cited next to them, or they are estimates and say so.
>
> Companion docs in this scratchpad: `story/comic_timing.md` (timing vocabulary taken from the trailer), `story/story_map.md`, and `custom/wishlist.md` (customisation design: chunks `LOOK`, `OPTX` and `UNL2`). The budgets here leave room for those.

---

## 0. Summary

| | GBA: "superbly cheap but effective isometric" | PS1: "a good 2.5D" |
|---|---|---|
| Look | A 2:1 isometric diorama. Pre-rendered 32x16 floor diamonds and wallpaper strips, with plush furniture sprites made at build time and depth-sorted. Only the 2 back walls are drawn. The view rotates in 4 steps of 90 degrees. | A real 3D room box built with the GTE. Walls and floor have textures. Furniture is low-poly from the same primitive recipes as `model.c`, and small decor is billboards. The camera is a tilted diorama camera with a spring orbit in 90 degree steps (free orbit while L/R is held), 2 zoom levels, and a cutaway for the near walls. |
| Main trick | **The bitmap is static.** The room is composed once into both mode 4 pages. After that, only *dirty rects* are recomposed: walking chibis, the animated items and the HUD bars. The cursor, the placement ghost, emotes and hearts are **OBJ sprites** (OAM is unused today). Squash and stretch, shake, zoom punch, fades and the red pulse come from **free hardware**: affine OBJ, the BG2 affine matrix, BLDY and the BG palette. | The camera is static between orbits, so **per-vertex lighting and projections are cached**. The GTE only runs while the camera moves. Recolouring uses **per-material CLUT rows**, so wallpaper, fabric and floor recolours cost no pixels. Day and night use gouraud vertex colour. |
| Frame cost (estimate) | Steady state 6-8 ms per tick, which is **30 fps**. A full recompose (room load, rotate, light change) takes about 14 ms for 2 frames, so it stays at 30 fps (20 fps worst case with a modal). Target was 20 fps or more. | 500-650 primitives per frame. With the cache, about 12-18 ms in PCSX-Redux, which is **30 fps**. While the camera orbits (12 frames), about 25-33 ms, which is 24-30 fps. |
| New RAM | Room caches go in `gba_big` (17 KB, claimed while the room is up). Scene state goes in `scene_scratch()` (12 KB). Persistent `HOME` state is **at most 1.4 KB EWRAM**. **About 0 new IWRAM**: it reuses the `ow_sprite`/`ow_blit` ARM loops. | Platform `room.c` is at most 8 KB resident. The core scene and catalog go in a new `home` overlay group of about 60-70 KB, which is under the 109 KB slot, so the slot does not grow. The vertex cache uses the slot tail (about 40 KB free while `home` runs). |
| ROM / disc | About 0.45-0.55 MB of sprites and tiles. The ROM is 2.43 MB today and the file is padded to 4 MB, so it still fits in 4 MB. | Recipes and textures are about 40 KB inside `OVL.BIN`. |
| Save | New chunk `'HOME'`: **at most 1,425 B raw** worst case, typically 0.4-0.7 KB compressed. Story save 13.5 KB measured becomes at most 15 KB (cap 40 KB). Free play worst 59.1 KB measured becomes at most 60.5 KB (cap 88 KB). | Same chunk. The story save stays at **2 memory card blocks**. |

---

## 1. What a dream-house system builds on (existing code)

### 1.1 Homes and apartments today

| What | Where | What it means for housing |
|---|---|---|
| 1000 homes = 2 districts x 50 floors x 10 rooms. Slot id maps to floor and room (`floor = slot/10+1`, `room = slot%10+1`) | `core/src/fp_apartments.c:3-6`, `:18-19`, `:43-46` `slot_of()` | **Every citizen already has an address.** A visit is `SCENE_HOME` with arg = resident id. No new addressing is needed. |
| Facade browser: each window shows the occupant's state (asleep, out, possessed glow, ghost, candle for the dead) | `fp_apartments.c:193` `draw_window()`, `:277` `draw_facade()`, `:402` `SCENE_APARTMENTS` | Hook: **A on a window pushes `SCENE_HOME(id)`** instead of the care screen (or add it as a 2nd action). A window can tint itself from the room's wallpaper paint, so a decorated home is visible from the street. |
| `open_room()` sets per-session "seen" bits (`A.seen[1000/8]`, not saved) and grants `UL_FP_HOUSEWARMING` at 25 | `fp_apartments.c:26`, `:115-132` | The `HOME` chunk can make "visited" persistent (125 B) and drive visit achievements. |
| Care screen action "Their home" (`FPA_HOME`) pushes the facade scrolled to their window | `core/src/fp_resident.c:20-24` (labels), `:64-95` `build_actions()` (`:76`, `:80`, `:87`), `:520` | Change it to **push `SCENE_HOME(2+id)`** (visit). Add "Decorate their room" (gift furniture) for friends at level 40 or above. |
| Address label "Towers/Pits F-R" | `fp_resident.c:733-735` | Reuse it as the room's name plate. |
| Story: `PLACE_HOME` (rest and save), `BEAT_C1_HOME`, `CUT_HOME`, the House Key item | `core/include/doomo/story.h:77`, `:101`, `:154`; `core/src/story_places.c:40` (home places map to `ACTLOC_APARTMENTS`), `:273` "Rest & save"; `core/src/story_script.c:474`, `:923` (`ITEM_HOUSE_KEY`) | Story already has *your home*. The `PLACE_HOME` menu gets "Go inside", which opens `SCENE_HOME(-1)` for the player. Chapter 1 moving in becomes the tutorial. |

### 1.2 Items, shop and the 64 limit

| What | Where | What it means |
|---|---|---|
| `enum ITEM_*` (append only, saved as counts) | `core/include/doomo/items.h:8-36` | |
| `DOOMO_STATIC_ASSERT(ITEM_COUNT <= 64)` | `items.h:38` | **`ITEM_COUNT` is 63 (measured by compiling the header).** One slot is left. |
| `uint8_t items[DOOMO_MAX_ITEMS]` (64 counts in `DoomoTown`) | `core/include/doomo/town.h:159`, `types.h:27` | **Furniture must not be items.** It needs its own catalog (`FURN_*`, up to 256) with ownership counts in the `HOME` chunk. |
| `DoomoItemInfo` row layout (name, desc, price, cat, stat deltas, icon) | `items.h:43-53`; rows split into `items_ext.c` and included inside `items.c` | Use it as the model for `DoomoFurn` (section 4.1). |
| Shop: tabs by `ICAT_*`, daily seeded stock (`STOCK_MAX 12`), 30% deal of the day, locked items shown as "???" | `core/src/fp_shop.c:1-13`, `:15-28`, `:53` `price_of`, `:78-106` `build_stock` | Use the same stock rotation and deal logic for a **HomeMart** with furniture tabs. Put that UI in the `home` overlay group so the catalog does not cross PS1 overlay groups (section 3.3). |

### 1.3 Save system (measured sizes)

| What | Where |
|---|---|
| Slot caps: story 40 KB, free play 88 KB | `core/include/doomo/save.h:11-12` |
| Codec work area is the scratch tail: LZ 6,400 B plus **one ext chunk of at most 8,192 B** (`DOOMO_SAVE_EXT_MAX`) | `save.h:38-50`, `:55-62` |
| CRC32 (with combine), LZ (4 KB ring, offset up to 3,840, length up to 130, 1,024 hash heads), bit writer, resident delta vs roster | `core/src/save.c:141-200`, `:441-505`, `:508-672` |
| Ext chunks written after the town as `[tag][ver][len][bytes]`; unknown tags skipped; missing chunks `reset()` | `save.c:942` `write_exts`, `:974` `read_exts`; `core/include/doomo/save_ext.h:1-12`, `:20-27` |
| **All 5 registry slots are taken**: studio `SONG`, extra = creator `NICK` (`creator.c:171`), test, freeplay `FPLY` (`fp_common.c:283-323`), fishcook `FISH` (`fishcook.c:119`) | `core/src/save_ext.c:3-23` |

So `HOME` needs **one additive slot**: `doomo_save_ext_home` in `save_ext.h`, plus `case 5` and a count of 6 in `save_ext.c`. `custom/wishlist.md` plans `LOOK`, `OPTX` and `UNL2` too. The clean fix is to **turn the switch into an array of 12 slots** once, and integration should own that change.

**Measured save sizes** (`core/build/tests save`, 19 tests passing):

| Island | Bytes |
|---|---:|
| empty | 312 |
| random, untouched / lived in | 4,657 / 5,693 |
| 1000 premade, untouched | 6,765 |
| story, all premade, lived in | **13,504** |
| 500 custom + 500 premade changed | 45,617 |
| 1000 random custom + 2048 edges (worst case) | **59,148** |
| song book with 3 songs | +1,295 |

Media:

- **GBA:** 128 KB flash, slot 0 at `0x00000` (40 KB) and slot 1 at `0x0A000` (88 KB). Saves stream through the 17 KB `gba_big`, so no RAM copy exists (`docs/GBA.md` section 8, lines 457-494). The slot layout is fixed, so new chunks only use headroom inside a slot.
- **PS1:** memory card files of 1-12 blocks of 8 KB, plus 640 B of header frames. A save takes about 1 s per block (`docs/PS1.md` section 9, lines 423-458).

### 1.4 Unlocks and progress

| What | Where |
|---|---|
| 1024 unlock bits in `DoomoTown.unlocks`, PROG counters 31 of 32 used | `unlocks.h:23`, `town.h:164-165`, `unlock_ids.h:93-98` |
| Ranges table (one owner per range) | `core/include/doomo/unlock_ids.h:12-25`, `:37-60` |
| **Spare:** foundation 202..239 (38 ids), action 564..607 (44 ids). Everything else is owned. | `unlock_ids.h:14`, `:19` |
| Free play achievements 740..764 (`UL_FP_HOUSEWARMING` is the last), asserted at 770 or below | `core/src/fp_int.h:199-210` |

Plan:

- **Ownership of furniture is not stored in unlock bits.** It is nibble counts in `HOME`.
- **Catalog gating reads existing ids read-only.** A `DoomoFurn.unlock` names an id in any team's range, for example a fishcook recipe unlock gates the matching kitchen item.
- **About 12 home trophies** need new ids. Re-assign **action 564..579 to a "home" team** in `unlock_ids.h` (integration owns it), or use `UNL2` if the customisation chunk lands.
- **No PROG slot is needed.** Home counters live in the chunk.

### 1.5 Activities and scenes

| What | Where |
|---|---|
| `ACTLOC_*` (9 locations incl. `ACTLOC_APARTMENTS`), `ACTF_NEEDS_RESIDENT` | `core/include/doomo/activities.h:15-26`, `:28-32`, `:34-44` |
| **All 6 table slots are taken** (minigames, concert, collections, combat = `levels.c:696`, extra = `fishcook.c:28`, studio) | `activities.c:13-18`, `:27-32` |
| Scene struct and registry, 35 scenes | `core/include/doomo/scene.h:7-37`, `scenes.h` |
| **Shared scene scratch: 12 KB**, owner-tagged, "claim in enter(), re-claim if lost" | `scene.h:39-57`, users listed at `:58-71`; `core/src/scene_scratch.c:10-29` |
| Shared per-frame billboard list (120) | `scene.h:79-81`, `scene_scratch.c:33-35` |

Housing adds:

- New scenes **`SCENE_HOME`** and **`SCENE_HOME_MART`**: an `extern` each in `scenes.h`, both in the PS1 `home` overlay group.
- `doomo_activities_home` (slot 7) with "Decorate my home", "Visit a neighbour" (`ACTF_NEEDS_RESIDENT`) and "HomeMart".
- Scratch tag `'HOME'` added to the users list.

### 1.6 The GBA 2D layer

| What | Where |
|---|---|
| Platform 2D API: `plat_clear/rect/shade_rect/line/text/image/checker/draw_skipped` | `core/include/doomo/platform.h:33-61` |
| `plat_image`: 8bpp, index 0 transparent, integer scale 1..4, flip, silhouette, shadow | `platform.h:43-48` |
| `plat_overworld` (top-down, lit-tile cache) | `platform.h:105-120` |
| Mode 4, 2 pages, flip every 2nd vblank (30 Hz max), catch-up ticks skip drawing (never below 15 fps) | `gba/src/video.c:3`, `:42`; `gba/src/main.c:198-209`; `docs/GBA.md` "Frame pacing" (lines 252-262) |
| **OAM, BG2 affine, MOSAIC and BLEND registers are defined but unused.** `DISPCNT` is `MODE4|BG2` with no OBJ. BG2 affine is set to identity once. The OBJ palette is already loaded with `doomo_palette`. | `gba/src/gba_hw.h:23`, `:37-46`, `:53`; `video.c:24`, `:42`, `:47-48` |
| Overworld ARM loops: `ow_sprite(img, x, y, scale, flags, cm)` (LUT-mapped, clipped to the `owf` viewport), `ow_blit` (stride copy), `ow_shade`, 2-way lit-tile cache in `gba_big` | `gba/src/ow_int.h` (whole file), `gba/src/overworld.c:1-20`, `:28-48` |
| `gba_big` 17 KB, claimed by OW, R3D or SAVE | `gba/src/gba_plat.h:63-70`, `gba/src/bigbuf.c:19`; `docs/GBA.md:212-223` |
| UI span rasteriser (span-merged rects, shape cache) | `core/src/ui_raster.c`; `docs/GBA.md:376-404` |

Measured fps (`docs/GBA.md` tables at lines 353 and 390):

- 2D UI screens run at 22-30 drawn fps (dialogue 24, keyboard 22, most screens 30).
- `plat_overworld` runs at 21.7 fps (36 ms). This is the closest existing analogue, and housing must beat it.
- The raycaster runs at 19-29 fps.

Memory (`arm-none-eabi-size` on the current `doomogachi.elf`, matching `docs/GBA.md:140-157`):

| Region | Used | Free |
|---|---|---|
| ROM | `.text` 870,352 + `.rodata` 1,535,820, about **2.43 MB** (file padded to 4 MB) | |
| IWRAM | 21,672 + 3,868 bss | **6,460 B for a 6 KB stack, so about 316 B spare** |
| EWRAM | 254,464 / 262,144 | **7.5 KB** |

The largest EWRAM user is `doomo_town` at 109,524 B (measured). The memory pass (task #77) is aiming for 24 KB or more free.

**Rules that follow:** no new IWRAM code, at most about 1.5 KB of new resident EWRAM, and everything else goes in `gba_big` or `scene_scratch`.

### 1.7 The PS1 GTE pipeline

| What | Where |
|---|---|
| `r3_camera / r3_begin / r3_quad (textured, auto-subdivide, fog) / r3_tri_g / r3_quad_g / r3_point / r3_billboard / r3_shadow` | `ps1/src/r3d.h:62-115` |
| `plat_view3d` (ray-fan visibility, per-corner smoothed light, fog, billboards) | `ps1/src/view3d.c`; `docs/PS1.md` section 5 (324-373) |
| `plat_overworld` 3D (tilted camera of about 58 degrees, `day_colour()` daylight, glow cells) | `ps1/src/overworld.c:244` `day_colour`, `:258` |
| **Chibi from `DoomoLook` as boxes, blobs and pyramids**, lit, back-face culled, swing animation | `ps1/src/model.c:1-15`, `:78` `box()`, `:102` `blob()`, `:139` `pyramid()`, `:166` `model_set_look()`, `:248` `model_player_draw()` |
| VRAM: 80 world-texture slots (LRU), image cache (5 pages of 256x256 8bpp), master CLUT at y=480, **15 silhouette CLUT rows 481-495** | `docs/PS1.md:132-150`; `ps1/src/vram.c:47` `vram_wtex`, `:206` `vram_image(img, tx, ty, TexRef*)` |
| 2D: `gfx2d.c` TILE merging, SPRT/POLY_FT4 images | `docs/PS1.md` section 4 |

Measured perf in PCSX-Redux (`docs/PS1.md:530-545`, which says Redux runs 1.5-2x slower than a console):

| Scene | Polygons | 3D time |
|---|---:|---|
| overworld | 290-316 | 20-22 ms |
| 3D citizen close-up | 370-550 | 17-28 ms |
| third person | 240-470 | 13-22 ms |

So the budget is roughly **35-50 us per polygon in Redux**.

### 1.8 PS1 overlays and RAM

| What | Where |
|---|---|
| 15 overlay groups by object name regex; `.text`/`.rodata` only; anything two groups reach becomes resident | `ps1/tools/ovl_plan.py:1-33`, `:43-59` |
| Slot = largest group (combat **109 KB**), whole sectors; CD read about 0.3 s per 70 KB | `ovl_plan.py:386-402`; `ps1/src/overlay.c:38`, `:118-134`; `docs/PS1.md:169-199` |
| Game disc ends `0x801dd310`: **139 KB below the stack top** (75 KB above the 64 KB stack reserve). EXE 1,224 KB, BSS 511 KB, `OVL.BIN` 778 KB | `docs/PS1.md:237-251` |
| Save scratch = both display lists (112 KB), never streams | `docs/PS1.md:253-266` |

Plan:

- Add the group `("home", r"home(_\w+)?$|furn(_\w+)?$")` to `GROUPS`.
- Core `home.c`, `home_draw.c`, `home_edit.c`, `home_visit.c`, `home_mart.c` and `furn_data.c` total an estimated **60-70 KB, which is under the 109 KB slot, so RAM does not grow**.
- The PS1 platform `room.c` is resident: **at most 8 KB of the 75 KB spare**.

### 1.9 Art pipeline

| What | Where |
|---|---|
| `artgen`: build-time host tool, **integer-only and seeded so `art_data.c` is byte-identical everywhere** (`make -C core check-gen`) | `core/tools/artgen.c:1-14`; `core/gen/art_data.c` (2.2 MB of source) |
| Fixed 256-colour palette: 1-15 UI, 32 paints x 3 shades (16-111), 8 skins x 3, 15 ramps x 8 (136-255). **Fully allocated.** | `core/include/doomo/palette.h:8-15`, `:35-46` |
| Textures: 48 of 64 used (measured; `textures.h:36` asserts at most 64). Sprites: 54 kinds, at most 224 images | `textures.h`, `sprites.h:77` |
| Procedural citizens at run time: `portrait_render` 48x56, `bodyspr_render` 16x24 (4 directions x 4 walk frames), `figure_render` 48x80; LRU image cache of 18 KB (48 chibis) | `core/include/doomo/portrait.h:1-18`, `:26-53`; `docs/PORTRAIT.md` |
| Colormap and tint tables in ROM (16 light levels, `TINT_*`) | `palette.h`; `docs/GBA.md:193` |
| Trailer toolkit (host only, floats): `tk_ease` and `tk_bounce`, `tk_post_shake`; `tk_chibi` soft-blob 3-tone shading with coloured selective outlines | `trailer/tk.h:140-141`; `trailer/scenes/s02_title.c:878`; `trailer/tk_chibi.c:1-11` |

**What follows for furniture:**

- Write one **integer-only `furngen`** host tool that reads one recipe table. It bakes GBA iso sprites and tiles, and exports the same recipes as PS1 mesh data.
- Port `tk_chibi`'s soft-blob shading and coloured outline to integer maths so furniture matches the citizens.
- Floats are not allowed. They would break `check-gen` determinism (`artgen.c:11`).

---

## 2. GBA: superbly cheap isometric rooms

### 2.1 Geometry and screen layout

- **Tile:** 2:1 isometric diamond, **32x16 px**. Height unit: 1 px = 1/16 tile.
- **Room sizes** (apartment tiers):

  | Tier | Size | Floor diamond | Screen fit |
  |---|---|---|---|
  | Studio | 6x6 | 192x96 | 6x6 floor + 64 px walls = 192x160. Fits with HUD bars once the walls are cropped to 48 px under the top bar. |
  | One-bed | 8x6 | | Pans |
  | Loft | 8x8 | 256x128 | Pans |
  | Penthouse / Hellvile Pit-house | 2 rooms with a door | | |

  The room viewport is 240x136, between a 12 px top HUD and a 12 px bottom HUD. Bigger rooms **pan with the cursor**. A pan is a full recompose (section 2.4), and it uses a spring ease that overshoots, so it lands with a wobble.
- **Walls:** only the 2 back walls are drawn, 64 px tall, wallpaper in 16 px strips. The front edges get a 4 px "cutaway" skirting so the room reads as a diorama.
- **4 rotations:** layout coordinates rotate in the core. Each item has 2 baked facings (front-left and front-right). The back facings use `IMG_FLIPX` of the opposite front, with a "back" variant only where it matters (TV, wardrobe, bed headboard).

### 2.2 What is pre-rendered (ROM)

| Asset | Format | Size each | Count | ROM |
|---|---|---:|---:|---:|
| Floor pattern | 2 variants of the 32x16 diamond, 8bpp, opaque inside the mask | 1 KB | 32 floors | 32 KB |
| Wallpaper | left strip 16x72 + right strip 16x72, 8bpp | 2.3 KB | 32 papers | 74 KB |
| Furniture | 2 facings, span-encoded 8bpp; average box 32x40, about 60% fill; animated items get 2-4 frames | about 2.1 KB with animation | 180-200 items | about 400 KB |
| Light pool / rug shadows | 4 dither masks | 0.5 KB | 4 | 2 KB |
| **Total** | | | | **about 0.5 MB.** The ROM goes from 2.43 MB to about 2.95 MB, still under the 4 MB padded file. |

**Span encoding.** Per row: `[skip u8][run u8][run bytes…]`, ended by `0xFF`. This drives a span-skipping version of the existing `ow_sprite` loop, or decodes once per room into the `gba_big` cache (section 2.5). Transparent pixels cost nothing.

**Paint slots instead of palette rows.** The palette is full (`palette.h:8-15`), so recolouring is done with a **LUT at blit time**:

- Each item is authored in up to 2 marker paints: slot A = `PAINT_BUBBLEGUM` shades, slot B = `PAINT_MINT` shades. These are colours the recipe never uses for anything else, and `furngen` checks that.
- At run time a 256-byte LUT maps those 6 indices to the chosen paints. The LUT is **composed with `doomo_colormap[light]`** (and with `doomo_tintmap` for Hellvile).
- A recolour, the day/night level and the hell tint all cost **the same one LUT read per pixel** that `ow_sprite` already does with `cm` (`ow_int.h`).
- LUTs are built when a paint or light changes (256 B, about 1.5k cycles) and cached per (paint A, paint B, light) in `gba_big`.

### 2.3 Hardware it uses (all free per pixel)

| Effect | GBA feature | Notes |
|---|---|---|
| Walking-chibi cursor, placement **ghost**, emote pops, hearts, "!" | **OBJ sprites, 8bpp 1D, char base `0x06014000`.** In mode 4 only the top 16 KB of OBJ VRAM is usable, which is 256 8bpp tiles. | Ghost up to 64x64 is 4 KB; cursor 0.5 KB; 8 emotes 4 KB. Needs `DCNT_OBJ` and an OAM shadow (1 KB, in `gba_big`) copied at vblank. |
| **Elastic squash and stretch** of a dropped item, wobble on "nope" | **Affine OBJ** (32 matrices, double-size). The ghost lands, squashes to 130/70%, rebounds to 90/115%, settles, *then* bakes into the bitmap. | The trailer's BOING timing in hardware (`s01_splash.c:4`, `s02_title.c:6`). |
| Screen shake, **zoom punch** on a comic beat, iris "door" transitions | **BG2 affine matrix and reference point** (mode 4 BG2 is an affine bitmap): `BG2X/Y` give shake of ±1-3 px; `PA/PD` = 0x0E0 is a 1.14x punch. | Zero pixels redrawn. It also scales the HUD for those 2-6 frames, which is part of the joke. Expose it as `plat_screen_fx(dx, dy, zoom_q8, fade)` with a no-op weak default (native/PS1: draw offset). |
| Fade to black / flash white (scares, smash cuts) | **BLDCNT darken/brighten + BLDY 0..16** on BG2 (and OBJ). | One register write per frame. |
| Possession **red pulse**, lights-flicker scare | **BG palette swap**: DMA a 512 B `tintmap`-applied palette at vblank. The OBJ palette stays clean, so the cursor and emotes stay readable. | Per-pixel cost 0. Restore the palette after. |
| Ghost visitor / see-through furniture while placing | OBJ **semi-transparent mode** + `BLDALPHA`. | |
| Glitch on "the room rearranged itself" | `REG_MOSAIC` on BG2 for 6 frames. | |

**Why chibis go in the bitmap and not in OBJ.** Walking chibis are drawn **into the bitmap** through dirty rects, not as OBJ. This gives correct occlusion: the chibi walks *behind* the wardrobe. OBJ always draws above BG2, so a sprite chibi would always sit in front of furniture. OBJ is kept for things that are always on top.

### 2.4 Renderer: static bitmap + dirty rects

The new platform entry point (section 4.4) is `plat_room(const DoomoRoomView *v)`. It is called once per frame, like `plat_overworld`. On the GBA it keeps state between calls.

1. **Full recompose** of the back page happens when `v->gen` (layout generation counter), the rotation, the pan, the light level or the paint changes. Draw order:
   - fill the background (DMA);
   - left and right wall strips from the baked cache (`ow_blit` rows, word copies);
   - floor diamonds back-to-front (`ow_blit` with a diamond mask: two runs per row);
   - rugs and floor decals;
   - **depth-sorted** items and chibis.

   Items are sorted by key `(x+y)*16 + z_layer`, with multi-tile items split into 1-tile-wide columns (the classic iso fix for long sofas). The sorted list is cached and rebuilt only when `gen` changes.

   It marks *both* pages dirty: page A now, page B on the next frame.
2. **Steady state:** for each moving or animated thing (up to 4 chibis, animated items such as a TV flicker, candle, fish tank or ghost), the dirty rect is the union of the old and new bounds. Each rect is pushed to *both* pages' dirty lists, so it is redrawn now and again after the flip.

   **To redraw a rect:** set `owf.ox/oy/ow/oh` to the rect, which gives free clipping in `ow_sprite`. Then redraw the wall and floor from the cache, then every item and chibi whose bounds overlap, in sorted order. Rects that overlap or touch are merged, with at most 12 per page; when there are more, the whole room is recomposed.
3. **The HUD and UI never fight the cache.**
   - The top and bottom HUD bars sit outside the room viewport and are fully redrawn by the core every frame (DMA fills plus text, under 1 ms).
   - While a modal or the catalog panel is open, the **platform records the bounds of all 2D drawn after `plat_room`** in a frame (a bbox accumulator in `plat_rect/image/text`, which is a few instructions).
   - Those bounds go into the *next* frame's dirty list for that page. Closing a panel then restores the room automatically.
4. **Night:** a light level change (once per in-game hour, or a lamp toggle) triggers a full recompose with new LUTs. Lamps (`FF_LIGHT`) blit fullbright and stamp a dithered **light pool** through `doomo_art_lighter` on the floor. Windows have "glass" marker pixels filled with the sky colour for the current daylight.

### 2.5 Memory (GBA)

| Where | What | Bytes |
|---|---|---:|
| `gba_big` (claim `GBA_BIG_BY_ROOM`; it invalidates OW/R3D caches, which rebuild in a frame) | floor: 2 patterns x 2 variants x 512 | 2,048 |
| | wall strips: 2 sides x 2 papers x 1,152 (the paper can change mid-wall) | 4,608 |
| | LUT cache: 8 x 256 | 2,048 |
| | sorted draw list: 96 x 8 | 768 |
| | dirty lists: 2 pages x 12 x 8 | 192 |
| | OAM shadow | 1,024 |
| | animated-item state and bounds: 32 x 12 | 384 |
| | **Total (of 17,408)** | **11,072** |
| `scene_scratch('HOME')` | layout working copy (the edited room, 48 x 5 B) | 240 |
| | undo ring: 32 x 6 B | 192 |
| | catalog filter list: 256 x 1 | 256 |
| | occupancy and path grid: 16x16 + BFS queue | 512 |
| | visitors: 4 actors x 32 | 128 |
| | generated visit room (section 4.3) | 300 |
| | menu and strings | about 1,500 |
| | **Total (of 12,288)** | **about 3.2 KB** |
| Resident EWRAM (`.bss`) | `HOME` persistent state (section 4.2) | **at most 1,425** |
| IWRAM | **0 new.** Reuses `ow_sprite`, `ow_blit`, `ow_shade` (ARM, already in IWRAM). The span-skip variant of `ow_sprite` goes in ROM THUMB, or replaces `ow_sprite`'s inner loop if it fits the 316 B spare. | 0 |
| OBJ VRAM | ghost 4 KB, cursor 0.5 KB, emotes 4 KB, hearts and FX 2 KB | 10.5 / 16 KB |

### 2.6 Frame cost at 240x160 (estimates)

The arithmetic: 16.78 MHz, so a 30 Hz tick is 559k cycles and a 20 fps frame is 839k cycles. Per-pixel costs are based on the measured `ow_*` loops:

- cache copy: about 3 cycles per pixel (halfword pairs);
- LUT transparent blit from ROM: about 7-8 cycles per pixel.

| Case | Work | Cycles | ms |
|---|---|---:|---:|
| **Full recompose** (6x6, 20 items) | bg DMA 38.4k px (about 40k), walls 12k px x 3 (36k), floor 9.2k px x 3 (28k), items 16k px x 8 (128k), sort and LUTs (10k) | about 242k | **14.4** |
| Steady: 4 walking chibis + 3 animated items | 7 rects of about 560 px recomposed with about 1.5x overlap: 3.9k px x (3 + 8 x 1.5), done twice (both pages) | about 120k | **7** |
| | chibi images from the portrait cache (hits), 4 x 384 B | about 5k | 0.3 |
| HUD bars + text | about 5.8k px DMA + about 60 glyphs | about 15k | 0.9 |
| Core logic (sim tick, path, input) | | about 40-80k | 2.5-5 |
| **Steady total** | | **about 180-220k** | **11-13**, so **30 fps** |
| Rotate / pan / light change frame | recompose + HUD + logic | about 330k | about 20, so 30 fps (2 frames) |
| Catalog modal open | UI kit (measured modals 26.8-30 fps) + bbox restore of the panel rect on close | | 27-30 fps |

The 20 fps target is met with about 2x margin. The worst case is a full recompose under a modal, about 600k cycles, which is 20 fps for 2 frames.

### 2.7 Visiting chibis on the GBA

- `bodyspr_render` 16x24 with 4 directions x 4 frames goes through the existing portrait cache (`portrait.h:43-47`, 18 KB pool, 48 chibis).
- Iso movement maps to the existing directions: NE/NW use "up", SE/SW use "down", and the side is picked with `IMG_FLIPX`. Draw at scale 1 at 240x160.
- **Doubled "hero" size** (scale 2, 32x48) is available for the visit close-up (Talk), as a separate full recompose state.
- Sitting, sleeping and eating poses use the existing `EXPR_*` and frames, shifted down onto the seat. The seat offset comes from `DoomoFurn.seat_dz`.

---

## 3. PS1: a good 2.5D room

### 3.1 Scene

- **Room box from the same layout grid.**
  - Floor: one `r3_quad` per tile (64 world units per tile, the same units as map cells), textured with the floor pattern image (64x64, `vram_image`). `r3_quad` already subdivides for affine error.
  - Walls: 2-4 quads per wall tile column (128 units tall).
  - Skirting and crown moulding: thin gouraud quads.
  - Windows: a textured quad on the wall with a semi-transparent sky-coloured glass quad (`R3F_SEMI`) and a light shaft (additive semi quad) in the daytime.
- **Cutaway:** the 2 walls whose outward normal faces the camera drop to a 12-unit stub, The Sims style. When the camera yaw crosses a quadrant during an orbit, the stub swap uses a 4-frame drop-and-pop animation (walls *fall* and *bounce*, which is the elastic gag).
- **Furniture = recipes.** The same `FurnPrim` list the GBA baker uses (section 4.1): boxes, blobs, cylinders and pyramids, emitted at run time with `model.c`-style helpers. Factor out `box()`, `blob()` and `pyramid()` from `model.c:78-158` into `ps1/src/prims.c`, which both the model and the room use.
  - Back-face culled, gouraud lit.
  - Optional per-face texture from a small furniture atlas (cushion fabric, book spines, screen, poster art) through `vram_image`.
  - **Small decor** (plushies, plants, candles, skulls, food) are **billboards** of the GBA-style front sprites (`r3_billboard` + `r3_shadow`). Paper-doll charm is the "2.5" part.
- **Characters.**
  - Player: the real 3D chibi (`model_player_draw`, `model.c:248`).
  - Visitors: billboards of `figure_render` 48x80 (`portrait.h:49-53`) with **independent x/y scale for squash and stretch**. `DoomoRoomActor` carries `sx`, `sy`. Note that `DoomoBillboard` has only `scale`, so the room API carries its own.
  - Up to 3 visitors plus the player.
- **Camera.** It looks at the room centre.
  - Pitch fixed at 35 degrees, yaw = 45 + k x 90 degrees.
  - **Narrow FOV, long distance** for a tilt-shift diorama look (near-orthographic; `R3Cam.H` large).
  - Orbit runs in 90 degree steps on L/R taps. It uses a **damped spring**: overshoot about 8%, settles in about 10-12 frames, with a tiny camera bob on settle. Holding L+R gives free orbit with an analogue-like ramp.
  - Zoom has 2 levels: room overview and a close "visit" shot. A **zoom punch** of +12% for 3 frames plays on comic beats.

### 3.2 Lighting and colour

- **Ambient = daylight colour.** Reuse `day_colour()` (`ps1/src/overworld.c:244`): warm sunrise and sunset, blue night.
- **Sun or moon through the window:** one directional light. Boxes have only 5 face normals, so this is **5 shade values per light setup**, computed once.
- **Lamps:** up to 4 point lights. Per-vertex `1/(1+d²/r²)` from a 64-entry table, warm colour.
  - The floor grid has a vertex at every tile corner (49 vertices for 6x6), so lamp pools show as gouraud blooms.
  - Night with lamps is **cozy**. Night without lamps is **scary**: blue, with dither on (`docs/PS1.md`: the GPU dithers).
- **Contact shadows:** soft subtractive blob under each item (`r3_shadow`) plus darkened floor vertices along the wall seam (cheap AO).
- **Recolouring through CLUTs (the PS1 palette trick):**
  - Wallpaper, floor and fabric textures are 8bpp. Each material gets its **own 256-entry CLUT row**: a copy of the master CLUT with the 6 marker paint entries swapped. That is 512 B per row, uploaded once on change.
  - Borrow 4 of the 15 silhouette CLUT rows (y 481-495, LRU in `vram.c:298`) while the room is up, or reserve 4 more rows under the image cache.
  - **Zero per-pixel cost, unlimited recolours.**
- **Hellvile and possession:** a red additive or subtractive full-view TILE tint (already in the PS1 3D path, "Tint", `docs/PS1.md:369-372`) with flicker timing. A possessed item gets per-vertex jitter of ±1-2 units at 15 Hz and a red emissive.

### 3.3 Cost and memory (PS1)

**Primitive budget per frame** (6x6 room, 20 items, 3 visitors):

| Part | Primitives |
|---|---:|
| floor (36 tiles, about 2x subdivision near the camera) | about 80 GT4 |
| 2 walls + 2 stubs + trims | about 70 |
| windows and shafts | 10 |
| 20 items x about 6 primitives x about 3 visible faces | about 360 |
| 10 decor billboards + shadows | 20 |
| player model | about 150 |
| 3 visitor billboards + shadows | 6 |
| **Total** | **about 700** |

That is about 36 KB of display list at an average of about 52 B, inside the 56 KB per-frame list.

**Time:**

- Without caching, 700 primitives is about 28-35 ms in Redux, which is borderline 30 fps.
- **Vertex cache:** while the camera is still, store each static vertex's `sxy`, `sz` and lit colour; static geometry then skips RTPT and lighting and only does OT insertion and the primitive fill. Measured costs suggest GTE and lighting are well over half of the per-primitive work, so static geometry gets about 2-3x cheaper. The estimate is **12-18 ms in Redux, so 30 fps**.
- **Cache size:** about 1,500 vertices x 12 B = 18 KB. It lives in the **unused tail of the overlay slot**: the slot is 109 KB and `home` is about 60-70 KB, so about 40 KB is free. Expose it with `overlay_spare(&cap)` from `ps1/src/overlay.c`. If it is not available, fall back to the uncached path.
- **While orbiting** (12 frames), use the full path: about 25-33 ms in Redux, which is 24-30 fps. That is acceptable during a whoosh.
- **Decimate during an orbit:** decor billboards and the furniture atlas textures stay; tiny items (fewer than 4 primitives) can be culled by distance.

**RAM:**

- `ps1/src/room.c` + `prims.c` is at most 8 KB resident, against 75 KB above the stack reserve.
- Core `home*` + `furn_data.c` (recipes about 13 KB, catalog about 6 KB, strings) go in the overlay.
- **Shop UI in the same group:** `ovl_plan.py` makes cross-group data resident (`ovl_plan.py:22-29`), so the catalog must only be referenced from `home*` objects. Put HomeMart inside the `home` group, and give the free play shop a door into it rather than a tab.

**VRAM:**

- room textures: about 6 images of 64x64 in the image cache;
- furniture atlas: one 128x128 strip;
- decor sprites: 32x32 each, about 20 of them.

All of it fits in the existing image cache pages (`docs/PS1.md:141-145`). There is no `TEX_COUNT` pressure, because these are images, not `TEX_*` ids.

---

## 4. Data formats

### 4.1 Furniture catalog (ROM, `core/gen/furn_data.c`, generated by `furngen` from `core/data/furn/*.txt`)

```c
/* furn.h - append-only ids (saved). */
#define FURN_MAX 256                              /* ids fit a byte; DOOMO_STATIC_ASSERT */
enum { FCAT_SEAT, FCAT_TABLE, FCAT_BED, FCAT_STORAGE, FCAT_LIGHT, FCAT_DECOR,
       FCAT_WALL, FCAT_RUG, FCAT_KITCHEN, FCAT_FUN, FCAT_PLANT, FCAT_HAUNTED,
       FCAT_COUNT };
enum { FF_WALL = 1,      /* hangs on a back wall (x = along wall, z = height)   */
       FF_RUG = 2,       /* floor layer under everything, walkable             */
       FF_SURFACE = 4,   /* small items may sit on top (z = top height)         */
       FF_SMALL = 8,     /* may go on a SURFACE                                 */
       FF_LIGHT = 16,    /* emits light (radius in light_r)                     */
       FF_ANIM = 32,     /* animated (frames in GBA sprite, wobble on PS1)      */
       FF_HELL = 64,     /* Hellvile theme (black market only)                  */
       FF_BILLBOARD = 128 }; /* PS1 draws it as a billboard sprite              */
enum { FI_NONE, FI_SIT, FI_SLEEP, FI_EAT, FI_COOK, FI_PLAY, FI_WATCH, FI_MUSIC,
       FI_READ, FI_BATHE, FI_PRAY, FI_SUMMON /* the haunted ones */ };

typedef struct DoomoFurn {            /* 28 B + strings                          */
    const char *name, *desc;          /* "Bunny Beanbag", <= 32 chars            */
    uint16_t price;                   /* H$ (0 = gift/story only)                */
    uint16_t unlock;                  /* existing unlock id gate, 0xFFFF = open  */
    uint8_t  cat, flags, interact;    /* FCAT_*, FF_*, FI_*                       */
    uint8_t  fw, fd, fh;              /* footprint tiles (1..4), height px (/16)  */
    uint8_t  paint_a, paint_b;        /* default paints (PAINT_*), 0xFF = none    */
    int8_t   cute, spooky, comfy;     /* room-score contributions                 */
    uint8_t  need;                    /* sim need it restores (STAT_*), 0xFF none */
    uint8_t  seat_dz, light_r;        /* sit/sleep height, light radius (tiles)   */
    uint16_t prim0; uint8_t nprim;    /* recipe in doomo_furn_prims[]             */
    uint8_t  frames;                  /* anim frames (GBA)                        */
} DoomoFurn;

typedef struct FurnPrim {             /* 8 B, shared GBA-bake / PS1-runtime       */
    uint8_t kind;                     /* FP_BOX, FP_BLOB, FP_CYL, FP_PYR, FP_PANEL,
                                         FP_DECAL (flat textured), FP_SPRITE     */
    int8_t  x, y; uint8_t z;          /* centre, 1/16 tile                        */
    uint8_t sx, sy, sz;               /* half extents, 1/16 tile                  */
    uint8_t mat;                      /* hi nibble: paint slot (A/B/fixed),
                                         lo: shade style / texture id             */
} FurnPrim;

/* GBA/native 2D only (dropped by --gc-sections on PS1): */
typedef struct DoomoFurnSpr { const uint8_t *span[2][4]; uint8_t w, h, ox, oy; } DoomoFurnSpr;
extern const DoomoFurn    doomo_furn[FURN_COUNT];
extern const FurnPrim     doomo_furn_prims[];
extern const DoomoFurnSpr doomo_furn_spr[FURN_COUNT];   /* referenced by plat_room 2D paths only */
```

**Sizes:**

- 200 items x 28 B = 5.6 KB;
- names and descriptions about 7 KB;
- prims at an average of 8 per item, x 8 B = 12.8 KB;
- GBA sprites and tiles about 0.5 MB (section 2.2).

**Content target: about 180 items in 12 categories and 4 themes.**

- **Hillvile pastel plush.** Bunny beanbag, cloud bed, cupcake lamp.
- **Hellvile goth-cute.** Coffin bed, pentagram rug (plush), lava lamp that is actually lava, bone chandelier.
- **HaHaland park.** Picnic set, swing chair.
- **Haunted (story-only, `FCAT_HAUNTED`).** Items that move at night; the doll that is always facing you.

The kitchen items are gated by fishcook recipe unlocks. The music items are gated by studio unlocks.

### 4.2 Save chunk `'HOME'` (version 1)

```
u8   ver_flags            bit0 story home moved in, bit1-2 apartment tier (0..3)
u8   active_room          0..3
u16  house_points         (decor score, best)
4 x Room {
  u8  wallpaper, floor            ids (32 each, 0xFF = default)
  u8  wall_paint, floor_paint     PAINT_* recolours (slot A of the pattern)
  u8  size                        lo nibble w-1, hi nibble d-1 (max 16x16)
  u8  theme_flags                 lights on, curtains, haunted...
  u8  nobj                        0..48
  u8  name                        index into a room-name table
  nobj x Obj (5 B) {
    u8  furn                      FURN id
    u8  pos                       lo nibble x, hi nibble y (tile)
    u8  rzl                       bits0-1 rot, bit2 on-surface, bits3-7 z (wall height / 4)
    u8  paint                     lo 5 bits paint A (0x1F = default)
    u8  paint2_anim               lo 5 bits paint B, hi 3 bits per-item state (lamp on, door open)
  }
}
u8   owned[128]           nibble counts, 2 per byte (FURN_MAX 256, max 15 each in storage)
u8   seen_new[32]         catalog "NEW!" bits
32 x Gift (4 B) { u16 resident(10 bits) | slot(6 bits); u8 furn; u8 pos } gifts you placed in others' homes
u8   visited[125]         1000 bits: homes you visited (persistent "seen")
u8   haunt[16]            story: night-rearrangement seed/state, possessed-item bits
```

**Worst case:** 4 + 4 x (8 + 48 x 5) + 128 + 32 + 128 + 125 + 16 = **1,425 B raw**.

- That is far under `DOOMO_SAVE_EXT_MAX` (8,192).
- Typical (1 room, 20 items, a few gifts) is about 400 B raw. The LZ in `save.c` compresses that to about 200-300 B.
- The visited bits compress well while they are sparse.

**Slot impact (worst case):**

| Slot | Today (measured) | With `HOME` | Cap |
|---|---:|---:|---:|
| story | 13,504 B | 14.9 KB | 40 KB |
| free play | 59,148 B | 60.5 KB | 88 KB |

Even with `custom/wishlist.md`'s `LOOK` (up to 8 KB on GBA) the free play worst case is about 68.5 KB, still under 88 KB.

PS1 memory card: the story save plus 640 B of header frames plus `HOME` worst case is 15.6 KB, which **stays at 2 blocks**.

### 4.3 Other citizens' homes: generated, not saved

`home_visit_gen(id, Room *out)` is deterministic. It is seeded by `town.seed ^ id`, the roster record, and the citizen's state:

| Input | Effect on the generated room |
|---|---|
| `like_color` | wall and floor paints |
| `like_hobby` | 1-2 hobby items (guitar, easel, console, aquarium, weights) |
| `like_food` | a fridge magnet or snack item |
| `fear` | **never** includes its trigger item; maybe a protective item (nightlight for the dark) |
| district | Hillvile set or Hellvile set |
| `RES_POSSESSED` | corrupted variant: tilted frames, bleeding wallpaper decal, items face the wall |
| `RES_GHOST` | dust sheets, cold-blue light, everything faintly translucent |
| `RES_DEAD` | sealed room: candles, flowers you left (from graves), their favourite item on the bed. This is the heartfelt beat. |
| `RF_TRAVELER` / away | empty bed, a note on the table |

On top of that, apply up to 32 **Gift** overrides from `HOME`. Cost: 0 save bytes for 1000 homes, about 3 KB of overlay code, and about 300 B of scratch.

### 4.4 Platform API (additive, `platform.h`)

```c
typedef struct DoomoRoomObj {          /* resolved, render-ready (core fills per frame) */
    uint8_t furn, x, y, z;             /* tile, tile, height px                         */
    uint8_t rot, paint_a, paint_b;     /* 0..3                                          */
    uint8_t anim;                      /* frame / state                                 */
    int8_t  sq_x, sq_y;                /* squash/stretch, -64..63 (/128) for drops       */
    int8_t  jitter;                    /* possessed shake amplitude                      */
    uint8_t flags;                     /* RO_GHOST (placement preview), RO_INVALID,
                                          RO_FULLBRIGHT, RO_SEMI                          */
} DoomoRoomObj;
typedef struct DoomoRoomActor {
    const DoomoImage *img;             /* chibi (GBA) / figure (PS1) from portrait cache */
    fix16 x, y; int16_t z;             /* tile space 16.16 */
    int16_t sx, sy;                    /* 8.8 squash/stretch (PS1, native; GBA ignores)  */
    uint8_t flags;                     /* BB_FLIPX, BB_TRANSLUCENT, BB_SHADOW, RA_PLAYER  */
} DoomoRoomActor;
typedef struct DoomoRoomView {
    uint8_t  w, d, wallpaper, floor, wall_paint, floor_paint;
    const DoomoRoomObj *objs; int nobj;
    const DoomoRoomActor *actors; int nact;
    uint16_t gen;                      /* bump when layout/paint changes (GBA cache key) */
    int16_t  yaw;                      /* 0..4095; GBA snaps to 4 views, PS1 orbits      */
    uint8_t  zoom;                     /* 0 overview, 1 close; + punch q8 below          */
    int16_t  punch_q8;                 /* 256 = none                                     */
    int8_t   pan_x, pan_y;             /* tiles*16, big rooms                            */
    uint8_t  daylight, tint, tint_level;
    int8_t   cursor_x, cursor_y;       /* tile, -1 = hidden (GBA: OBJ)                   */
    uint16_t time;
    int16_t  vx, vy, vw, vh;
} DoomoRoomView;
void plat_room(const DoomoRoomView *v);
/* optional free screen effects for every scene (weak no-op default) */
void plat_screen_fx(int dx, int dy, int zoom_q8, int fade /* -16..16 */);
```

The core gets a weak reference implementation `core/src/room_ref.c`: iso drawn with `plat_image` and `plat_rect`, full redraw each frame. Native tests and screenshots use it, and the GBA and PS1 override it. A parity test compares the native output of the reference with a GBA harness screenshot of the same layout, like the existing render goldens.

---

## 5. Elastic and comedic timing hooks (from the trailer)

`trailer/` uses `tk_bounce` and `tk_ease` (floats, host only; `tk.h:140-141`), `tk_post_shake` (`s02_title.c:878`), and freeze-then-snap gags (`s01_splash.c:7`: "the heart freezes… side-eyes"). The game needs a **fixed-point** equivalent: `core/src/spring.c`, 3 functions of about 1 KB, resident.

```c
int spring_step(Spring *s, int target_q8);   /* critically under-damped: k, d per preset */
int ease_bounce_q8(int t_q8);                /* 0..256 with 1 overshoot + 1 rebound       */
int squash_q8(int t, int impact_t);          /* 130/70 -> 90/115 -> 100 over 7 frames      */
```

| Beat | Frames | GBA | PS1 |
|---|---|---|---|
| Item drop | fall 4, squash 2, stretch 2, settle 3; dust puff; `SFX_BOING`-like | affine OBJ ghost, then bake | vertex scale about the base |
| Invalid placement | 3 wiggles x 2 frames, no sound for 4 frames, then a tiny "hnn" | OBJ matrix rotate ±8 degrees | yaw jitter |
| Rotate camera | 10-12 frames, 8% overshoot | full recompose at the end of the snap, plus a 2-frame BG2 shear wobble | spring yaw |
| Walls drop in cutaway | 4 | (n/a) | stub height spring |
| Scare: light out | hold 18 frames dead still, BLDY to 16, then 1 frame red flash | BLDY + BG palette swap | black TILE + red tint |
| Smash cut back to cute | 0-frame cut, then a 3-frame zoom punch | `BG2PA/PD` | camera zoom spring |
| Visitor enters | 4 hops with squash (`s07_vape.c:581`: "four hops in…") | bitmap chibi + y offset | billboard `sx`/`sy` |

---

## 6. Build plan (ordering and owners)

1. **Core contracts.**
   - `furn.h`, `home.h`;
   - `platform.h` `plat_room` + `plat_screen_fx`;
   - save-ext registry made into an array (adds the `HOME` slot);
   - activities slot `doomo_activities_home`;
   - `unlock_ids.h` re-assigns action 564..579 to "home".
2. **`furngen`** (integer-only host tool; recipes in `core/data/furn/*.txt` produce `core/gen/furn_data.c`; `check-gen` byte-identical), with a contact sheet like `artgen --sheet`.
3. **Core scenes:** `home.c` (state, build mode, undo), `home_draw.c`, `home_visit.c` (generator), `home_mart.c`, plus `room_ref.c`. Tests: layout validity, save round trip, generator determinism, 1000-home generation time.
4. **GBA:** `gba/src/room.c` (THUMB) using the `ow_*` loops, an OAM module and `plat_screen_fx`. Harness `fps` on 3 rooms; target 30 fps steady and at least 20 fps worst case.
5. **PS1:** `ps1/src/prims.c` (from `model.c`), `ps1/src/room.c`, CLUT rows, `overlay_spare()`, and the `ovl_plan.py` group `home`. Target 30 fps in Redux with the cache.
6. **Integration:**
   - the "Visit" action from the apartments facade and the care screen (`fp_apartments.c:115`, `fp_resident.c:520`);
   - the story `PLACE_HOME` "Go inside" (`story_places.c:273`);
   - `docs/GBA.md` and `docs/PS1.md` memory tables.

**Risks:**

- GBA EWRAM is at 7.5 KB free. The `HOME` state of at most 1.4 KB fits, but **only if nothing else lands first**. Task #77 (24 KB or more free) should land before this.
- GBA IWRAM has no room. Any new ARM loop must replace something, or run as THUMB from ROM.
- The PS1 `home` group must stay at or under 109 KB, or the slot grows. Watch `ovl_plan.py --report`.
- The flat 2:1 iso on the GBA will not match the PS1 perspective exactly. That is intentional: same layout and art identity, platform-native look.
