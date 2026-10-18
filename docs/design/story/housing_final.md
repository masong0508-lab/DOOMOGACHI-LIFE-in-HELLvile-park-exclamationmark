# The Dream House: final design (GBA + PS1)

DOOMOGACHI LIFE in HELLvile Park! - build, decorate, visit and defend homes.

> Status: design, not code. Nothing in the repo was touched. This document merges `story/housing_tech.md` (measured sizes, rendering proposals), `story/comic_timing.md` (the trailer's timing vocabulary) and `story/bible_heart.md` section 3 (the house as the story's anchor) into one buildable spec. Every number marked **measured** comes from those docs (they were measured against the tree at `43af439` plus the uncommitted work of 2026-10-11). Everything about the new house screens is an **estimate**, because none of it exists yet, and it says so where it matters.
>
> Reading order: section 0 is the whole thing in one page. Sections 1-5 are what the player does. Sections 6-9 are how it is drawn and stored. Sections 10-12 are story, tests and build order.

---

## 0. One page

| | What the player gets |
|---|---|
| **Your house** | A cottage at Hillvile Lane (4,7) with up to **6 rooms** unlocked over the story or bought in free play: Living Room, Bedroom, Kitchen, Hell-Cellar, Garden, Roof Annex. Each room is a 6x6 to 10x8 tile grid with 2 back walls (the Garden has none; it has sky). |
| **Decorating** | Place, pick up, rotate, stack on surfaces, recolour (2 paint slots per piece), wallpaper, floor, ceiling tint, windows and doors as wall pieces, lamps with real light pools, 10 themes, a snap grid with half-tile for small items, rubber-band multi-select (up to 8), a 32-step undo ring, 12 ROM blueprints plus 2 saved ones, photo mode with 16 photo recipes that re-render in the Library's gallery. |
| **The house grades you** | Four live stats: **Comfort, Spook, Style, Light**, plus **Barricade** during Ch3. They feed the sim (sleep, pet holiness, visitor moods, night raids) and the story gates (COSY, LIT, HELD, HOME). A star rating from visitors post-game, delivered as a rule of three. |
| **Visiting** | Every one of the 1000 citizens has an address today. Their rooms are **generated** from personality, likes, fears, district and status (possessed, ghost, dead, away): 0 save bytes. You can leave up to 32 gifts in other homes. Citizens react to decor by personality with double-takes, deadpans, hearts and flees. |
| **Catalog** | **306 pieces** in 13 categories (`FURN_MAX` 320): pastel plush, gothic, hellish, plushies and toys, kitchen, tech, garden, lighting, walls/windows/doors, rugs, trophies, seasonal, story and gag merchandise. Sources: HomeMart, the Hellvile Black Market, 84 side missions (the heart bible's `F:` rewards), Workshop crafting, minigame/level/fishing/concert trophies, friendship gifts, Breach drops, the calendar. |
| **GBA** | 2:1 isometric in mode 4. 32x16 floor diamonds, 16 px wallpaper strips and pre-baked span-encoded furniture sprites, depth-sorted, 4 view rotations. **The bitmap is static; only dirty rects are recomposed.** Cursor, held item, emotes and "!" are OBJ sprites (unused today); squash and stretch is affine OBJ; shake, zoom punch, fades and the red pulse are BG2 affine, BLDY and palette DMA. Estimate: **11-13 ms per tick steady (30 fps), 20 ms on a rotate, 20 fps worst case under a modal.** |
| **PS1** | A GTE room box with near walls cut away, furniture built live from the same primitive recipes the GBA baked, small decor as billboards, a tilted narrow-FOV diorama camera that orbits in 90-degree steps on a spring, daylight plus sun/moon plus 4 lamp pools in gouraud, recolours through per-material CLUT rows. About **700 primitives**; with a static-vertex cache **12-18 ms in PCSX-Redux (30 fps), 24-30 fps while orbiting.** |
| **Memory** | GBA: `HOME` persistent state **2,609 B worst case** of `.bss`, caches in `gba_big` (11.1 KB of 17 KB), scene state in `scene_scratch` (about 3.8 KB of 12 KB), **0 new IWRAM**, about 0.8 MB more ROM (2.43 -> 3.25 MB of the 4 MB file). PS1: a new `home` overlay group of about **95 KB** in the 109 KB slot, `room.c` + `prims.c` at most 8 KB resident, the vertex cache in the core's scene scratch (PS1 scratch is 32 KB). |
| **Save** | One new chunk `'HOME'`, 2,609 B raw worst case, typically 350-600 B compressed. Story save 13,504 B measured -> at most 16.1 KB (cap 40 KB). Free play worst 59,148 B measured -> at most 61.8 KB (cap 88 KB). PS1 story save stays at 2 memory card blocks for any realistic house (section 9.4 has the one edge case). |
| **Timing** | Every interaction uses the trailer's three entrances (DROP / POP / SLAM), the beat ladder (2, 3, 5, 8, 13, 21), the silence map, the rule of three with constant interval and escalating magnitude, 3-4 frame whiplash inserts with one persistent change, and buttons 12-25 frames after the punch. Section 5 is the table. |
| **Story** | The house is the one space that changes every chapter: Cosy (Ch1) -> Haunted (Ch2) -> Barricaded (Ch3, the siege of Ep 3.5) -> Open (Ch4, the Open House) -> Yours (post-game). The Ch3 **Hold the Door** defence runs inside the room view: furniture is barricade HP, windows take welded plates, the toaster turret gets a slot, 3 waves, 4 guests to keep safe. |

---

## 1. The house: rooms, tiers and expansion

### 1.1 The six rooms

| # | Room | Grid | Walls | Unlock (story) | Unlock (free play) | What is special |
|---|---|---|---|---|---|---|
| 0 | **Living Room** | 6x6, grows to 8x6 at tier 2 | 2 back walls, pink door on the south edge, fireplace slot on the back-left wall, 2 window slots | Ep 1.2 (the key) | new island | The tutorial room. The fireplace is the Ch3 chimney (the pet leaves through it) and Kevin's rule applies: nothing may block it. |
| 1 | **Bedroom** | 6x6 | 2 back walls, 1 window | Ep 1.5 (Housewarming: Nana "you can't sleep on a CRATE") | 400 H$ at Town Hall "Expand" | Sleeping here is the story's save + heal. Bed orientation is scored (section 4.3). The egg nest moves here after the hatch if you want. |
| 2 | **Kitchen** | 8x6 | 2 back walls, 1 window, a counter rail along the back-right wall | Ep 2.1 (Nana's recipe cards) | 600 H$ | Kitchen pieces restore the player's Hunger in story mode; cooking recipes from Brimstone Bites unlock the matching appliances. Fridge magnets are stackable `FF_SMALL` wall items. |
| 3 | **Hell-Cellar** | 8x8 | 2 back walls of rock, no windows, a hatch in the floor | Ep 2.6 (the Cellar Key, `ITEM_CELLAR_KEY` already exists) | 900 H$, only after a Hellvile visit | Permanently dim (light level 6 of 16 before lamps). The only room where `FF_HELL` pieces have no Spook penalty to Comfort. Lava lamps that are actually lava. In Ch2 this is where the haunting starts. |
| 4 | **Garden** | 8x6, open | **No walls.** A fence ring (wall-slot pieces) and sky | Ep 2.4 (balloon letters land in it) | 500 H$ | Sky gradient by daylight, weather from `OPT_WEATHER`, plants grow over in-game days (3 stages, saved as the item's 3-bit state), the DAYS SINCE LAST FIRE sign's natural home. The Ch4 "second garden" is the same room with the south fence opened onto Hellvile. |
| 5 | **Roof Annex** | 6x6 | 2 back walls, Hellvile style (bone trim) | Ep 4.3 (Rivet welds it on; Hellvile neighbours' housewarming) | 1,200 H$ after the ending | The Open House's "Annex furnished" condition. Mixed-style bonus is scored here first. Post-game: Grumbleguts' Big Chair wants to live here. |

Rooms are entered through doors drawn on the back walls (a door is an `FF_WALL | FF_PORTAL` piece with a fixed slot per room; it cannot be moved, only re-skinned). Walking the player chibi into a door swaps rooms with a 5-frame heart iris out, 2 frames black, 6-frame iris in (section 5). On the GBA a room swap is one full recompose (about 14 ms, section 6.6).

**Tiers.** `HOME.ver_flags` bits 1-3 count unlocked rooms (1..6). Tier also sets the Living Room's size (6x6 at 1-2 rooms, 8x6 from 3 rooms) with a wall-crumble expansion animation (section 5.4). The free-play apartment facade (`fp_apartments.c`) shows the player's cottage as a separate building on the Hillvile side once any room is decorated, so the house is visible from the street.

### 1.2 What a room holds

- Up to **40 objects** per room (the save is sized for it; the GBA draw list is sized for 96 sprites because multi-tile pieces split into columns).
- Up to **4 live actors**: the player chibi plus 3 visitors (GBA bitmap chibis; PS1 player model plus billboard figures).
- Up to **4 lamps** that cast pools (more lamps are fullbright but cast nothing; the UI says "the room is as bright as it gets").
- One wallpaper (32 patterns) with a paint, one floor (32 patterns) with a paint, one ceiling tint (8; on GBA it is the colour of the 4 px cornice strip and the light's warmth; on PS1 the room box has a real ceiling plane that only shows while orbiting and in photo mode).
- Window slots: Living 2, Bedroom 1, Kitchen 1, Annex 1 (the Cellar none, the Garden a gate instead). A window is a wall piece; its glass pixels are filled with the sky colour; in Ch3 it takes a Welded Plate (section 10.3).

---

## 2. The editor

### 2.1 Modes and controls

The editor is one scene, `SCENE_HOME`, with three modes: **Live** (walk, talk, sit, sleep, examine), **Build** (cursor on the grid) and **Photo**. The HUD is a 12 px top bar (room name, clock, H$, the four stat pips) and a 12 px bottom bar (context hints), so the room viewport is 240x136 on GBA and 320x216 on PS1.

| Action | GBA | PS1 | Notes |
|---|---|---|---|
| Move cursor / walk | D-pad | D-pad or left stick | In Build the cursor moves one tile per press, repeats at 6 then 3 frames; it glides with `TW_FOLLOW` (lazy follow, no overshoot) so it never bounces |
| Place / pick up / confirm | A | Cross | Pick-up has a 2-frame anticipation squash (section 5.1) |
| Cancel / put back | B | Triangle | Puts the held piece back where it was with a reverse DROP |
| Rotate held piece | L / R | L1 / R1 | Squash-flip through the edge-on frame (section 5.1) |
| Rotate the view (empty hand) | L / R | L1 / R1 (tap); L2 / R2 held = free orbit | GBA snaps to 4 views; PS1 springs through 90 degrees |
| Catalog / wallpaper / blueprint / theme / photo / score | START menu | START menu, Square = catalog shortcut | The menu is a `ui_*` modal; it takes no scratch (section 6.5) |
| Recolour | hold A on a placed piece, then L / R per paint slot | Circle, then L1 / R1 | The LUT (GBA) or CLUT row (PS1) rebuilds on release, not per press |
| Multi-select | hold SELECT and move: rubber band | hold R2 and move | Up to 8 pieces; move, rotate (about the band's centre), copy to the clipboard, delete to storage |
| Undo / redo | SELECT tap / SELECT + B | Select / Select + Triangle | 32-step ring, 8 B per step (section 9.2) |
| Zoom | none (the GBA view is already a close diorama) | R3 or Square held: overview / close | The close shot is the Talk shot for visits |
| Talk / sit / use | A on an actor or a piece with `interact != FI_NONE` in Live mode | Cross | Sitting and sleeping use `seat_dz` and the existing `EXPR_*` frames |
| Leave | B at the door, or START > Leave | same | Autosave prompt if the layout changed |

### 2.2 Placement rules

- **Snap grid.** Full-tile snap for everything except `FF_SMALL` pieces on a `FF_SURFACE`, which snap to half tiles (the Obj stores `pos` as a tile and a 2-bit sub-position). Wall pieces snap to 1 px along the wall and 4 px in height (`Obj.z`).
- **Footprint.** `fw x fd` tiles rotated with the piece. A tile may hold one floor piece plus one rug under it plus any number of `FF_SMALL` pieces on a surface whose `z` tops are distinct. Rugs (`FF_RUG`) are walkable and sit under everything.
- **Stacking.** A `FF_SMALL` piece on a `FF_SURFACE` gets `z = surface.fh` and the `on_surface` bit; it moves with the surface when the surface is picked up (the editor collects stackers into the hand as a group: this is why multi-select and "held group" share one code path).
- **Walkability.** The room keeps a 16x16 occupancy grid and a BFS path grid in scratch (section 6.5). A placement that disconnects the door from the bed or from any seated guest is **allowed but scored** (Comfort -8, "Nobody can reach the bed"); a placement over an actor is a "nope".
- **The "nope".** Invalid tiles (occupied, off-grid, a wall piece on the floor, a floor piece on a wall, anything blocking the fireplace) shake the ghost x +2 -2 +2 -2 over 4 frames with a sad squash 0.94/1.06 and `SFX_ERROR`. **No red flash** (the trailer rule: stay cute; red is for hell).
- **Windows and doors** are pieces, but their slots are fixed per room. Changing a window changes the piece in the slot (wood frame, round porthole, stained glass, bone-rimmed, boarded). The pink front door has 6 skins; the door is never removed.

### 2.3 Themes, blueprints and presets

- **10 themes:** Pastel, Cloud, Candy, Cottage, Goth, Bone, Lava, Vapor, Picnic, Workshop. Each is a 6-byte ROM record `{wallpaper, floor, paint_a, paint_b, ceiling, accent}`. "Apply theme" repaints the walls and floor and recolours every piece that has a free paint slot to the theme's two paints; it is undoable as a single step (the undo ring stores a `OP_THEME` entry that holds the previous 6 bytes plus a 40-bit "which pieces were recoloured" mask, 12 B total, 2 ring entries).
- **Blueprints:** 12 in ROM (one per theme, plus "Nana's Starter", "Shelter" for Ch3), 2 player slots in the save (one room each, 250 B raw). Applying a blueprint places only the pieces you own; missing pieces appear as grey ghost outlines with a "Buy at HomeMart" hint, so a blueprint doubles as a shopping list. Storage overflow (more than 15 of a piece) is refused with the counter squash.
- **Photo mode:** hides the HUD, frees the camera (PS1: full orbit and 2 zooms; GBA: 4 views and a 2-step BG2 zoom of 1.0x and 1.14x), applies `OPT_CAMERA_FILTER`, lets up to 3 stickers (hearts, "it's fine.", a horned poop) be dragged on, and "takes" a photo: a 1-frame white flash at 7/16 and `SFX_TIK`. **A photo is an 8-byte recipe, not pixels:** `{room 3b, yaw 2b, zoom 1b, filter 4b, daylight 4b, stickers 3 x 6b, hash16}` where `hash16` is the layout generation's CRC so the gallery can say "(the room has changed since)". The Library's gallery wing re-renders photos through the same `plat_room` path. 16 slots = 128 B of save. Taking a photo of a visitor's room stores their id in place of the room bits (the recipe has 10 spare bits).

---

## 3. Visiting and gifting

### 3.1 Getting there

- **Free play:** the apartments facade (`fp_apartments.c`) - A on a window pushes `SCENE_HOME(2 + id)`; the care screen's "Their home" (`FPA_HOME`) does the same instead of only scrolling the facade. The address label "Towers F-R" / "Pits F-R" becomes the room's name plate.
- **Story:** knock on an apartment door on the overworld (the story maps already tag doors with `STORY_TAG_DOOR + PLACE_APARTMENTS`); the questlog's Friends tab lists "Visit" for friends at level 20+.
- **Your own house:** `PLACE_HOME`'s menu gains "Go inside" (`SCENE_HOME(-1)`); "Rest & save" stays and now plays the sleep beat in the Bedroom (section 5.5).

Visiting is a scene push, so the overworld's `OWLD` scratch is lost and rebuilt on return (`ow_scratch_ensure` already does this).

### 3.2 Generated rooms (0 save bytes for 1000 homes)

`home_visit_gen(id, Room *out)` is deterministic: seed `town.seed ^ (id * 0x9E3779B1)`, the roster record and the citizen's live state. It writes one 6x6 room (Hillvile) or 6x6 cellar-style room (Hellvile) with 8-14 pieces:

| Input | Result |
|---|---|
| `like_color` | wall paint; floor paint is its complementary from a 32-entry table |
| `like_hobby` (32) | 1-2 hobby pieces: easel, guitar stand, console, aquarium, dumbbells, telescope, sewing table, chess set... (a 32 x 2 table of `FURN` ids) |
| `like_food` (48) | a fridge magnet or a snack on the table (48 -> 12 snack pieces) |
| `like_music` (16) | a poster from the 16 jukebox posters |
| `fear` (24) | never the trigger piece (dark: no room without a nightlight; clowns: no Hattie poster; heights: nothing on the top shelf); sometimes the protective piece |
| `persona_type()` (16) | the layout template: tidy types align to walls, chaotic types scatter, shy types push the bed to the corner, show-offs centre the biggest piece |
| district | Hillvile pastel set or Hellvile goth set; `RF_HELLBORN` living in Hillvile gets a mix (the story's point) |
| `RES_POSSESSED` | corrupted variant: frames tilt 0.14 rad, a bleeding-hearts decal on the wallpaper, every piece faces the wall, `jitter = 2` on all objects, the lamp is on but the pool is red |
| `RES_GHOST` | dust sheets (a grey recolour of every piece), cold blue daylight, all actors translucent |
| `RES_DEAD` | **the sealed room:** candles, the flowers you left at the grave (from `FPLY`'s graveyard flowers), their favourite hobby piece on the bed, no music. The heartfelt beat: no gag, no button. |
| `RF_TRAVELER` / away | the bed empty, a note on the table ("back soon" in their catchphrase's register) |
| relationships | a framed photo per `REL_MARRIED` / `REL_BEST_FRIEND` edge (up to 2), a dartboard with the nemesis's portrait (`REL_NEMESIS`) |

Then up to 32 **gifts** from `HOME.gifts` override tiles. Cost: about 3 KB of overlay code, 250 B of scratch for the generated room, and about 60k cycles on GBA (3.5 ms, once per visit; the facade never generates rooms, it reads the paint only).

### 3.3 Tomodachi-style wants

Each citizen has 3 seeded **room wants** ("I wish I had somewhere to sit", "something pink", "a plant"), shown as a thought bubble on the facade window and in the care screen. Fulfilling one with a gift gives friendship +8 and a reaction beat (section 5.3); all three give the piece back as a "thank you" in their style (the only way to get the 24 Hillvile/Hellvile citizen-crafted pieces, section 8.4). Wants are recomputed weekly from `(town.day / 7) ^ id`.

### 3.4 Reactions by personality

Reactions use the `PERS_*` sliders (0..15) already on every resident:

| Slider | Low | High |
|---|---|---|
| `PERS_EXPRESSION` | deadpan: no bounce, idle clock 0, tiny 1x text that appears ("oh. a lamp.") | the full double-take: look (8f), shock (16f), stare at the lens (8f), happy (section 5.3) |
| `PERS_MOVEMENT` | reacts from where they stand | hops to the piece in 4 hops (the s07 hop cycle) |
| `PERS_ATTITUDE` | serious: inspects, nods, "acceptable." | easygoing: sits on it immediately, even if it is a lamp |
| `PERS_QUIRK` | normal | unhinged: reacts to the wrong piece |
| `PERS_SPEECH` | polite: "how... bold." | blunt: "no." |

Plus the likes: `like_color` match -> hearts (POP burst, ends 1.6x and keeps breathing); hobby piece -> "!" bang; `fear` trigger -> SHOCK and a 4-hop flee out of the door, friendship -3, and the game tells you why ("Pip is scared of the dark. The cellar has no lamp."); `hate_food` on the table -> the "ew" squash 1.10/0.90 and a sweat drop sliding 1 px every 2 frames.

---

## 4. The house score

### 4.1 The four stats (plus one)

All integer, recomputed when `gen` changes (not per frame), each 0..100:

```
Comfort  = clamp( sum(comfy) + 10*has_bed + 4*min(seats,6) + light_cov/4 - clutter - path_penalty + fengshui, 0, 100 )
Spook    = clamp( sum(spooky) + 15*haunted_items + (night && light_cov < 25 ? 20 : 0) + cellar_bonus, 0, 100 )
Style    = (n == 0) ? 0 : 100 * count(theme == dominant_theme) / n          (Ch1-3)
         = (n == 0) ? 0 : 100 * min(hill, hell) * 2 / n + same_theme_bonus   (Ch4+, "Harmony": mixing is the point)
Light    = 100 * floor_tiles_within_light_r_of_a_lit_lamp / floor_tiles      (daylight counts as 50 from windows)
Barricade= sum over pieces within 1 tile of a door or window of min(fw*fd*fh/16, 20) + 25*plates + 40*turret_placed   (Ch3 only)
clutter  = (occupied_tiles * 100 / tiles > 60) ? (occupied% - 60) : 0
```

`comfy`, `spooky` and `cute` are per-piece `int8_t` in `DoomoFurn`. `light_cov` uses the 16x16 grid and each lamp's `light_r` (tiles) with a Manhattan check (no multiplies).

### 4.2 Feng-shui rules (small, named, explained in the Score panel)

| Rule | Effect | Who taught it |
|---|---|---|
| Nothing blocks the fireplace | required for every grade; the editor refuses the placement with a nope | Kevin ("the chimney has to breathe. trust me.") |
| Bed does not face the door | Comfort +5 if true, -5 if it does | Nana |
| A lamp within 2 tiles of the bed | Comfort +5, Spook -5 | Miss Lulla |
| A plushie on the bed | Comfort +3; the pet sleeps there if it exists | Pip |
| A mirror facing a window | Light +10, Style +3 | the HomeMart clerk |
| Pentagram rug under the bed | Comfort -10, Spook +15, pet holiness drifts -1/day | Vex, unhelpfully |
| Two tables pushed together | Harmony +10 (Ch4) | the Open House |
| Hellvile piece next to a Hillvile piece | Harmony +2 per pair, up to +20 (Ch4) | the ending |
| Kitchen piece outside the Kitchen | Style -3 each ("a fridge in the bedroom. sure.") | - |
| The Plain Human figurine anywhere | no effect; the panel says "it does nothing. it never has." | - |

### 4.3 What the stats do

| Stat | Sim / story effect |
|---|---|
| Comfort | Sleeping at home restores player Energy (story) by `40 + Comfort/2` and HP by `Comfort/4` per night. Visitors' `STAT_HAPPY` +`Comfort/10` per visit. Grade COSY at Comfort 40 with a bed and 6 pieces (Ep 1.3: the egg only hatches in a cosy room). |
| Spook | The pet's `holiness` drifts toward demon by 1/day above 60 and toward angel by 1/day below 20 (Doomogachi players will decorate on purpose). Ch2 haunting intensity (section 10.2). Free play: the nightly "Night Raid" chance is `Spook/4`% when Hellvile neighbours are agitated. Hellborn visitors like it (+happy), timid ones (`fear` dark/ghosts) flee. |
| Style / Harmony | HomeMart deal-of-the-day leans toward your dominant theme. Harmony >= 60 in Ch4 is a condition of HOME. Post-game visitor ratings weight it. |
| Light | Grade LIT at 60 (3 lamps; candles count double). Ch2: whispers per night shrink per lamp; at LIT the wallpaper eye stops blinking. Night Comfort uses it. |
| Barricade | Door HP in Hold the Door (section 10.3); HELD needs >= 100 and a turret. |

### 4.4 The grade stamp and the rating

Checking the score (START > Score, or automatically when a goal is met) plays the trailer's counter + SLAM: the number rolls `v = N * (a/22)^2.2` (fix16: `t^2 * (0.8 + 0.2t)`, under 3% error), then the grade stamp lands with `TW_SLAM_STAMP {614, 410, 236, 266, 256}` and `SHK_STAMP`. Post-game weekly ratings are **three visitors, 15 frames apart, escalating** (a nod, a "!" with hearts, a full double-take with a 1.2x stamp and a 3-note fanfare), then a topper one R13 later: the starer at the window, perfectly still, holding up a card with a number on it.

---

## 5. Animating everything (the timing bible applied)

Units: 1 frame = 1 tick = 1/30 s. Pixel amplitudes are GBA pixels; PS1 scales x by 4/3 and y by 3/2. Tables are Q8.8 (`256 = 1.0`); springs are Q4 positions with Q14 coefficients (`comic_timing.md` section 3.2). Everything below goes through `tween.h` / `beat.h` / `camfx.h` from the bible's section 7, so the housing code adds no curves of its own.

### 5.1 Build mode

| Action | Entrance | Exact numbers | Sound (frame 0 unless said) |
|---|---|---|---|
| Enter the house | heart iris in | 6 frames, `r = 6 + 250*smooth(a/6)^2`; the door sprite plays `TW_JELLY` | SHWIP (sine 96 -> 72 over 0.16 s) |
| Pick up a piece | anticipation | 2 frames squash 1.10/0.90 at the feet, then a 3-frame lift stretched 0.92/1.10 while the shadow shrinks to 60%; stackers lift 2 frames later (ripple) | POP at the lift frame |
| Carry (held ghost) | hover | the ghost bobs `2*sin(0.16t)` px (the breathing period, 39.3 f) at 50% alpha (GBA: OBJ semi-transparent; PS1: `R3F_SEMI`); the cursor glides with `TW_FOLLOW` | - |
| Place a piece | **DROP soft** (the s14 handheld table) | fall 6 frames `t^2` from 24 px (GBA) / 32 px (PS1), stretched 0.85/1.18 while falling; impact `SX {312, 282, 243, 251, 261, 256}`, `SY {200, 230, 274, 264, 253, 256}`, `DY {6, 3, -3, -2, 0, 0}`; 2-frame dust puffs at the footprint corners; on GBA the affine OBJ does frames 0-5 and the bitmap bakes the piece on frame 6 | THUD + BOING on the impact frame |
| Neighbours react | secondary motion | every piece sharing an edge plays `TW_JELLY` at 0.4 amplitude **2 frames later**; pieces 2 tiles away at 0.15, 4 frames later (a ripple) | - |
| Invalid spot | the nope | x shake +2, -2, +2, -2 (4 f), squash 0.94/1.06, no red | SFX_ERROR (small) |
| Rotate held piece | squash-flip | `sx {256, 128, 0, -128, -266, -256}` over 5 frames with the facing sprite swapped on the edge-on frame (3); PS1 rotates the mesh 90 degrees with a `TW_SLAM4` overshoot on yaw instead | a soft "fwip" (WHOOSH at 40%) |
| Rotate the view | brake + settle | GBA: full recompose at the end of a 10-frame hold during which BG2 shears `PB = +/-0x08` for 2 frames (a wobble), then the new view; PS1: `TW_BOING` spring on yaw (11-frame period, zeta 0.71), about 5% overshoot, settles by frame 12, with a 1 px camera bob on settle; walls that change stub drop-and-pop over 4 frames | WHOOSH, then a THUD at 30% on settle |
| Recolour | covered cut | the piece is hidden by a paint-splat POP (`TW_POP_BUBBLE`) for 3 frames; the LUT/CLUT swaps behind it | a wet "plip" (POP, lower) |
| Wallpaper / floor / theme | covered cut | **a citizen runs across with a paint roller** (a 4-frame flee curve `x = dir*(2a + 0.35a^2)`, 16 frames edge to edge) and the swap happens behind them; or, if nobody is home, heart iris 5 out, 2 black, 5 in | WHOOSH on entry, a "squeak-squeak" every 4 frames, THUD when they leave |
| Multi-select move | group DROP | pieces land in reading order 2 frames apart (the s08 polaroid stagger at 1/4), each with the soft table; the last one gets the full dust | THUD per landing with falling volume |
| Undo | reverse DROP | the piece stretches 0.92/1.10 for 2 frames and pops **up** 24 px over 4 frames `(1-t)^2` into nothing; redo is the normal DROP | SHWIP reversed (72 -> 96) |
| Apply blueprint | the s09 weld montage | pieces land with **accelerating gaps 10, 9, 8, 8, 6, then 4 every piece**; owned pieces DROP; missing ones POP in as grey ghosts with `TW_POP_MENU`; a final SLAM stamp "BLUEPRINT" | THUD per landing; FANFARE on the stamp |
| Room expansion | `SHK_RUMBLE` | the wall crumbles over 12 frames (3 px constant shake, dust every 2 frames); 8 frames of silence; the new floor tiles DROP in diagonally from the far corner, gaps of 3 frames; the new wall strips slide down `TW_SLAM8` | RUMBLE (noise sweep), silence, then THUD per tile at 40%, FANFARE |
| Grade stamp | counter + SLAM | section 4.4 | the counter ticks every 2 frames, KAZOO sting if the grade fell, FANFARE if it rose |
| Photo | shutter | 1 frame white at 7/16, then the HUD pops back with `TW_POP_BUBBLE` | TIK |

### 5.2 Live mode and ambient life (the baseline: nothing is ever static)

- Every actor idles on the chibi's incommensurate periods: breathing 39.3 frames, tail 22.4, ear twitch on a 97-frame cycle, blink 3 frames every 83, each phased by `id * 9`. Possessed visitors twitch ears at 1.7 rad per frame with `bob = 0`.
- **Animated pieces** (`FF_ANIM`): TV flicker every 2 frames between 2 frames, candle 3 frames at 4-5-6 frame intervals (co-prime with the breath), fish tank bubbles every 11 frames, the cuckoo clock once per in-game hour (it is out; it stays out), the fan 4 frames at 2 each. On GBA each animated piece is a dirty rect (at most 8 animated pieces per room before the renderer falls back to a full recompose).
- **Gag pieces** (section 8.3): the Plain Human figurine never animates (and is the only piece without `FF_ANIM` that the Score panel calls out); Kevin's bust blinks its third eye only, every 83+ frames; the DAYS SINCE LAST FIRE sign flips 7 -> "-" (2 frames with jitter) -> 0 in red pulsing every 4 frames when Rivet visits; the deflated Vapula plush sighs (a 20-frame slow squash to 0.9 and back) when sat on; the crooked crown lands crooked (0.25 rad) however you place it, and if you rotate it, it lands crooked the other way; the Big Chair creaks (`SFX_CREAK` 6-frame head-tilt timing) and a citizen who sits in it slides off over 8 frames `t^2`.
- The pet, if hatched, lives in the house: it hops between the lamp, the bed plushie and the nest with the s07 hop cycle (7 frames, 12 px arc, landing squash 1.22/0.80), sits in the light, and says "beep." once per visit at the worst possible moment (during a sincere line, with a duck to 0.55 for 6 frames).

### 5.3 Visitors: arrivals, double-takes, deadpans

| Beat | Numbers |
|---|---|
| A visitor arrives | the door `TW_JELLY`, then 4 hops in (7 frames each, 12 px arc, 1.22/0.80 landing squash); with several guests, 8 frames apart (Ep 1.5's party is nine citizens at 8-frame intervals, each DROPping their gift on the last hop) |
| The classic double-take (high `PERS_EXPRESSION`) | look at the piece (8 f), SHOCK with +/-1 px jitter (16 f), stare **at the lens** (8 f), happy (denial), then the reaction bubble pops `TW_POP_BUBBLE` with the line; a "tik" at the start of the stare |
| The dawning take (mid) | stop (R3), head turn over 2 frames, "!" bang (`bounce(a/4)`, 12 px), emote POP `{102, 307, 236, 266, 256}`, "tik" |
| The deadpan (low) | `bob = 0` for the actor, music ducked to 0.5, the caption slides up 8 px `{8, 6, 4, 2, 0}` with no bounce; tiny 1x text if the line is 3 words or fewer; hold 22 frames; the button is one blink |
| Group reactions | staggered by `id % 4` frames (a ripple, never a switch); hearts burst `TW_POP_BURST` and keep breathing |
| A frightened guest | SHOCK, a sweat drop every 3rd frame behind them, 4-hop flee out of the door with `x = dir*(2a + 0.35a^2)` (never eases out), the door swings `TW_SLAM4` |
| Sitting | a 2-frame plop squash 1.08/0.92 then `TW_WOBBLE` at 0.2 on the seat (the cushion gives), hearts if `comfy >= 6` |

### 5.4 Scares and whiplash (Ch2, the Cellar, possessed visitors, cursed pieces)

- **A cursed piece placed** (pentagram rug, Vapula's crown trophy, the Widow's plushie, the always-facing doll): a **4-frame hell insert** of the room (the GBA DMA's the hell palette and sets `MOSAIC` 2x2; PS1 swaps the master CLUT and adds a red subtractive TILE), with `SHK_IMPACT` and `SFX_GLITCH`, the music muted for exactly those 4 frames; then back to the same shot with **one** persistent change (the lamp grew horns, a frame tilted, one heart on the wallpaper is now an eye); 2 frames of nothing; a visitor's "...it's fine." appears (no pop) at R8.
- **The 3 AM move** (Ch2 nights): at exactly 03:00 all actor clocks freeze (`BOP_FREEZE -1`), 14 frames of silence, one piece slides one tile with no easing on a single frame, one "tik", 13 frames, then the clocks thaw. The player sees it only if awake (section 10.2).
- **Lights out**: hold dead still 18 frames, `BLDY` to 16 over 4 frames (PS1: a black TILE), 1 frame of red at 10/16, back to the room with the lamp off and every piece turned to face the wall. The deadpan line comes from the pet: "bep?"
- **Rules kept:** inserts are 3-4 frames; the insert owns the soundtrack for its length; never show a cute character harmed in the cute half; `OPT_REDUCE_FLASH` shows the insert darkened without strobing and turns the red frame into a darken pulse.

### 5.5 Heartfelt (the button withheld)

- **Sleep**: no gag. A 30-60 frame fade, the room by moonlight for 21 frames, black. The button is the next morning: 1 in 20 mornings the starer is at the window (post-game only), still, and waves if you stand still for R21.
- **The sealed room** (`RES_DEAD`): no music, no idle on the pieces, the candles' flames are the only motion. No reaction bubbles. Leaving plays the circle iris, not the heart.
- **Interlude I-C**, the empty house: every piece you placed has one sad examine line; the candle can be lit; lighting it is the trigger. The crayon style bleeds into the room's edges for 2 frames every R21 during the Mayor's confession (a palette lerp on GBA, a vertex-colour lerp on PS1).
- Rule from the bible: undercut at most half the sincere beats, and never the final one.

### 5.6 Music and silence in the house

The house has its own vamp (a music-box variant of the HaHaland theme) that **ducks to 0.6 under every reaction bubble**, mutes for inserts, and tape-stops (15 semitones over 5 frames) when a cursed piece lands, coming back on the grid as if it never stopped. Inside the Cellar the vamp is replaced by a low diminished pad that holds; in the Garden it is wind and the sign creaking. The sealed room and Interlude I-C have none. Budget: 20-25% silence inside scripted house beats, 0% in free decorating (the player makes the rhythm there).

---

## 6. GBA rendering: superbly cheap, effective isometric

This section is the exact technique. Everything reuses the overworld's ARM loops in IWRAM (`ow_blit`, `ow_sprite`, `ow_shade`, `gba/src/ow_int.h`); **no new IWRAM code** (316 B spare is not enough for anything, and the memory pass wants the whole 6 KB stack).

### 6.1 Projection and geometry

- Tile: 2:1 diamond, **32x16 px**. Height unit: 1 px = 1/16 tile. Wall height 64 px.
- Room coordinates `(x, y, z)` in tiles and px; view rotation `k = 0..3` rotates `(x, y)` in the core before projection (`(x, y) -> (W-1-y, x)` per step), so the platform only ever draws one orientation.
- Screen: `sx = (x - y) * 16 + ox`, `sy = (x + y) * 8 - z + oy`, with `(ox, oy)` the room origin (centred in the 240x136 viewport, panned for 8-wide rooms).
- A piece's hotspot is the bottom-centre of its footprint's front corner; multi-tile pieces are **split into 1-tile-wide columns** at bake time (each column is its own sprite with its own hotspot), which is the classic fix for a long sofa drawn in front of a chair that is actually behind its far end.
- Depth key: `(x + y) * 16 + layer`, layer = 0 rugs, 1 floor pieces, 2 stackers, 3 actors (an actor on tile `(x,y)` sorts after the piece on that tile and before the piece on `(x+1, y)`), 4 wall pieces by `z`. Sorted with an insertion sort over at most 96 entries, rebuilt only when `gen` changes (about 10k cycles).
- Walls: the two back walls only, 64 px tall, drawn from 16 px wallpaper strips; a 4 px cornice strip in the ceiling tint; the front edges get a 4 px skirting so the room reads as a diorama. The Garden draws a sky gradient instead (one DMA per 2 rows from a 64-entry daylight table) and a fence ring as wall pieces.

### 6.2 Pre-rendered art (ROM), generated by `furngen`

`furngen` is a new **integer-only** host tool (`core/tools/furngen.c`, like `artgen`) reading `core/data/furn/*.txt` recipes and writing `core/gen/furn_data.c`, byte-identical on every host (`make -C core check-gen`). For each piece it:

1. Projects each `FurnPrim` (box, blob, cylinder, pyramid, panel, decal, sprite) into iso with integer painter's order (prims sorted by `x + y + z/16`).
2. Shades boxes with the 3-tone rule (top = light shade, left face = mid, right face = dark) in the material's paint ramp; blobs with 2 tones plus a highlight ellipse at (-1/3, -1/3); cylinders with a 4-band ramp.
3. Draws the **coloured selective outline** (the ramp's darkest shade; pure black only on the bottom contact row) and a 1 px rim light on the top-left silhouette.
4. Dithers only where the recipe marks fluff (`mat` style bit), never elsewhere.
5. Writes two facings (front-right, front-left) as **span-encoded 8bpp**: per row `[skip u8][run u8][run bytes...]`, terminated by `0xFF`; back facings use `IMG_FLIPX` of the opposite front, with an explicit back variant only where `back_variant` is set in the recipe (TV, wardrobe, headboard, the Big Chair).
6. Emits the same prims for the PS1 unchanged, and a 1-bit 16x16 footprint mask for the editor.

Marker paints: slot A is authored in `PAINT_BUBBLEGUM`'s 3 shades, slot B in `PAINT_MINT`'s; `furngen` fails the build if a recipe uses those indices for anything else.

| Asset | Each | Count | ROM |
|---|---:|---:|---:|
| floor diamond pattern (2 variants, masked 32x16) | 1 KB | 32 | 32 KB |
| wallpaper strips (left 16x72 + right 16x72) | 2.3 KB | 32 | 74 KB |
| furniture, 2 facings, span-encoded, average 32x40 at 60% fill (0.9 KB per facing), 25% animated with 2-4 frames | about 2.1 KB | 306 | about 643 KB |
| back variants | 0.9 KB | about 40 | 36 KB |
| light pools, rug shadows, dust, sky table, fence | | | about 12 KB |
| **Total** | | | **about 0.8 MB**: ROM 2.43 MB -> about 3.25 MB, still inside the 4 MB padded file (carts go to 32 MB; the padding is a choice) |

### 6.3 Hardware used (free per pixel)

| Effect | Register | Use |
|---|---|---|
| cursor, held ghost, emotes, "!", hearts, the photo stickers | **OBJ** 8bpp 1D, char base `0x06014000` (mode 4 leaves the top 16 KB = 256 tiles). Needs `DCNT_OBJ` and a 1 KB OAM shadow copied at vblank | ghost up to 64x64 (4 KB), cursor 0.5 KB, 8 emotes 4 KB, FX 2 KB = 10.5 of 16 KB |
| the DROP squash, the nope wobble, the squash-flip | **affine OBJ** (32 matrices, double-size) on the held ghost | the whole section 5.1 table in hardware for the piece in hand; placed pieces animate through dirty rects (section 6.4) |
| shake, zoom punch, the shear wobble on rotate | **BG2 affine** (`BG2X/Y`, `PA..PD`) written in `video_flip` so it matches the page shown | shake +/-1..3 px shows backdrop colour at the edge for 2-3 frames (reads as impact); punch `PA = PD = 0xE0` is 1.14x |
| fades, white flash, the 18-frame lights-out | `BLDCNT` + `BLDY` 0..16 | matches the trailer's 0..16 levels and is smoother than its dither |
| red pulse, hell insert, the crayon bleed | BG palette DMA (512 B) at vblank, OBJ palette untouched | the cursor and emotes stay readable through a scare |
| glitch | `MOSAIC` on BG2 | the "room rearranged itself" and the insert |
| see-through ghost, translucent ghosts of the dead | OBJ semi-transparent + `BLDALPHA` | |

Chibis go **in the bitmap**, not OBJ, so they walk behind the wardrobe (OBJ always draws over BG2). OBJ is only for things that are always on top.

### 6.4 Static bitmap plus dirty rects

`plat_room(const DoomoRoomView *v)` is called once per frame, like `plat_overworld`, and keeps state between calls.

1. **Full recompose** when `v->gen`, the rotation, the pan, the light level or a paint changes: background fill (DMA), both wall strips from the `gba_big` cache (`ow_blit` rows, word copies), floor diamonds back to front (`ow_blit` with the diamond mask: two runs per row), rugs and floor decals, then the sorted pieces and actors through the span-skipping sprite path with the LUT. It marks both pages dirty (page A now, page B after the flip).
2. **Steady state:** for each moving or animated thing (up to 4 actors, up to 8 animated pieces, the turret during defence), the dirty rect is the union of its old and new bounds, pushed to both pages' lists. To redraw a rect: set `owf.ox/oy/ow/oh` to it (free clipping in `ow_sprite`), redraw wall and floor from the cache, then every piece and actor whose bounds overlap, in sorted order. Rects that touch are merged; at most 12 per page, beyond which the frame falls back to a full recompose (still 30 fps, section 6.6).
3. **UI over the room:** the platform accumulates a bbox of all 2D drawn after `plat_room` in a frame (a few instructions in `plat_rect/image/text`) and pushes it to the next frame's dirty list, so closing a menu restores the room without the core knowing.
4. **Recolour, light, tint = one LUT read per pixel**, the cost `ow_sprite` already pays with `cm`: the LUT maps the 6 marker indices to the chosen paints and is composed with `doomo_colormap[light]` and, in the Cellar or during possession, `doomo_tintmap`. LUTs are built on change (256 B, about 1.5k cycles) and cached per `(paint A, paint B, light)` in `gba_big` (8 entries).
5. **Night:** lamps blit fullbright and stamp a dithered light pool through `doomo_art_lighter` on the floor; window glass pixels take the daylight colour.

### 6.5 GBA memory

| Where | What | Bytes |
|---|---|---:|
| **`gba_big`** (claim `GBA_BIG_ROOM`; the OW/R3D caches rebuild in a frame when the room leaves) | floor: 2 patterns x 2 variants x 512 | 2,048 |
| | wall strips: 2 sides x 2 papers x 1,152 | 4,608 |
| | LUT cache 8 x 256 | 2,048 |
| | sorted draw list 96 x 8 | 768 |
| | dirty lists 2 pages x 12 x 8 | 192 |
| | OAM shadow | 1,024 |
| | animated-piece state and bounds 32 x 12 | 384 |
| | sky table (Garden) | 64 |
| | **total of 17,408** | **11,136** |
| **`scene_scratch('HOME')`** | active room, unpacked: 40 objs x 8 B | 320 |
| | undo ring 32 x 8 | 256 |
| | selection list 8 + clipboard 8 x 8 | 72 |
| | catalog filter (320 bits) + sort order 320 x 1 | 360 |
| | occupancy 16x16, height 16x16, BFS queue 256 | 768 |
| | actors 4 x 40 | 160 |
| | generated visit room (packed 250 + unpacked 320) | 570 |
| | defence state: 8 demons x 12, door/window HP 8 x 2, wave timers | 136 |
| | HomeMart stock 12 x 4 | 48 |
| | menu state and strings | about 1,200 |
| | **total of 12,288** | **about 3.9 KB** (8 KB spare: the platform may use it through `v->plat_cache`, section 7.3) |
| **resident `.bss`** | `HOME` persistent state (section 9.2) | **2,609 worst case** |
| **IWRAM** | none | 0 |
| **OBJ VRAM** | | 10.5 of 16 KB |

EWRAM today (measured) is 7,680 B free. 2,609 B of `HOME` leaves 5 KB, which is why **the memory pass (task #77, target 24 KB free) must merge before `HOME` lands**. If it does not, the fallbacks in order are: drop the 2 saved blueprint slots to 1 (-250 B), store `visited` as 500 bits per district with the other 500 regenerated as "seen if friend level > 0" (-62 B), and cap rooms at 32 objects (-288 B).

### 6.6 Frame cost (estimates; 559k cycles per 30 Hz tick, 839k per 20 fps frame)

Per-pixel costs from the measured `ow_*` loops: cache copy about 3 cycles per px, LUT transparent blit from ROM about 7-8 per px.

| Case | Work | Cycles | ms |
|---|---|---:|---:|
| Full recompose, 6x6, 20 pieces | bg DMA 38.4k px (40k), walls 12k px x 3 (36k), floor 9.2k px x 3 (28k), pieces 16k px x 8 (128k), sort + LUTs (10k) | about 242k | **14.4** |
| Full recompose, 8x8 Cellar, 40 pieces | walls 16k x 3, floor 16k x 3, pieces 32k x 8 | about 400k | **24** (still one 33 ms tick) |
| Steady: 4 chibis + 3 animated pieces | 7 rects of about 560 px with 1.5x overlap: 3.9k px x (3 + 8 x 1.5), twice (both pages) | about 120k | 7 |
| HUD + text | 5.8k px DMA + 60 glyphs | about 15k | 0.9 |
| Core logic (input, path, sim tick, score on change) | | 40-80k | 2.5-5 |
| **Steady total** | | **180-220k** | **11-13 -> 30 fps** |
| Rotate / pan / light change | recompose + HUD + logic | about 330k | about 20 -> 30 fps |
| Hold the Door, wave 3: 8 demons + 6 guests + turret (over 12 rects -> recompose every frame) | 242k + logic 80k + HUD | about 340k | about 20 -> 30 fps |
| Worst: 8x8 recompose under a modal | 400k + modal UI 150k + logic | about 600k | about 36 -> 20 fps for 2 frames |

The 20 fps floor holds everywhere; the common case is 30. `plat_draw_skipped()` is honoured: catch-up ticks skip the dirty-rect pass entirely.

### 6.7 Actors on GBA

`bodyspr_render` 16x24 (4 directions x 4 frames) from the 18 KB portrait cache; iso NE/NW use "up", SE/SW use "down" with `IMG_FLIPX`. Sitting and sleeping reuse `EXPR_*` frames shifted by `seat_dz`. The Talk close-up is a separate full-recompose state at scale 2 (32x48 "hero" chibis). The player's chibi is the story's player record (`RF_PLAYER`).

---

## 7. PS1 rendering: a good 2.5D

### 7.1 Scene

- **Room box from the same grid:** one `r3_quad` per floor tile (64 world units per tile, the map's units), textured from a 64x64 floor image via `vram_image`; walls as 2-4 quads per tile column, 128 units tall; skirting, cornice and a ceiling plane as thin gouraud quads (the ceiling only renders when the camera pitch sees it: orbit overshoot and photo mode). Windows: a textured quad plus a semi-transparent sky-coloured glass quad (`R3F_SEMI`) and an additive light shaft by day.
- **Cutaway:** the two walls whose outward normal faces the camera drop to a 12-unit stub. When an orbit crosses a quadrant, the stub swap plays a 4-frame drop-and-pop (the walls fall, bounce, and the new pair rises).
- **Furniture = recipes, drawn live:** the same `FurnPrim` list, emitted through `box()`, `blob()`, `cylinder()`, `pyramid()` factored out of `ps1/src/model.c:78-158` into `ps1/src/prims.c` (the player model uses them too). Back-face culled, gouraud lit, optional per-face texture from a 128x128 furniture atlas (cushion fabric, book spines, screens, posters).
- **Billboards vs low-poly:** a piece with `FF_BILLBOARD` (plushies, plants, candles, skulls, snacks, the sign: anything whose charm is its drawing) is an `r3_billboard` of its baked front sprite plus `r3_shadow`, with independent x/y scale for squash. Everything with volume (seats, beds, tables, wardrobes, appliances, lamps' bodies) is a mesh. The paper-doll decor among solid furniture is the "2.5".
- **Characters:** the player is the real 3D chibi (`model_player_draw`); visitors are `figure_render` 48x80 billboards with `sx, sy` for squash (`DoomoBillboard` has only `scale`, so `DoomoRoomActor` carries its own). Up to 3 visitors plus the player.

### 7.2 Camera and lighting

- **Camera:** looks at the room centre, pitch 35 degrees, yaw 45 + 90k degrees, narrow FOV at a long distance (large `R3Cam.H`) for a tilt-shift diorama. Orbit on L1/R1 is the `TW_BOING` spring on yaw (section 5.1); L2/R2 held is a free orbit with an analogue ramp; two zoom levels (overview, Talk close-up); a +12% zoom punch for 3 frames on comic beats through `plat_screen_fx`.
- **Lighting:** ambient = `day_colour()` (`ps1/src/overworld.c:244`), one directional sun/moon through the window (5 face normals -> 5 shade values computed once per light change), up to 4 lamp point lights with `1/(1 + d^2/r^2)` from a 64-entry table on every floor-grid vertex (49 for 6x6), so lamp pools are gouraud blooms. Night with lamps is cosy; night without is blue with the GPU's dither on.
- **Contact shadows:** `r3_shadow` blobs under pieces plus darkened floor vertices along the wall seam.
- **Recolours through CLUTs:** wallpaper, floor and fabric textures are 8bpp; each material gets its own 256-entry CLUT row (the master CLUT with the 6 marker entries swapped), 512 B uploaded on change. Borrow 4 of the 15 silhouette rows (y 481-495) while the room is up. Zero per-pixel cost.
- **Hell and possession:** the existing full-view red TILE tint with flicker timing; possessed pieces jitter +/-1-2 units at 15 Hz with a red emissive.

### 7.3 Cost, cache and memory

| Part (6x6, 20 pieces, 3 visitors) | Primitives |
|---|---:|
| floor, 36 tiles with about 2x subdivision near the camera | about 80 |
| 2 walls + 2 stubs + trims | about 70 |
| windows, shafts | 10 |
| 20 pieces x about 6 prims x about 3 visible faces | about 360 |
| 10 decor billboards + shadows | 20 |
| player model | about 150 |
| 3 visitor billboards + shadows | 6 |
| **Total** | **about 700** (36 KB of the 56 KB display list) |

Measured Redux cost is 35-50 us per polygon (docs/PS1.md), so 700 uncached is 28-35 ms: borderline. The fix is a **static-vertex cache**: while the camera is still, each static vertex's `sxy`, `sz` and lit colour are stored and static geometry skips RTPT and lighting (GTE plus lighting is well over half the per-primitive work), so static geometry gets 2-3x cheaper: **12-18 ms in Redux = 30 fps; 24-30 fps during the 12-frame orbit**, when tiny pieces (under 4 prims) may be culled by distance. The cache is about 1,500 vertices x 12 B = 18 KB.

**Where the cache lives:** `housing_tech.md` put it in the overlay slot's tail, but the `home` group estimate below leaves only about 14 KB there. Instead the core hands the platform a region of its scene scratch: `DoomoRoomView.plat_cache` / `plat_cache_cap`. On PS1 `DOOMO_SCENE_SCRATCH` is 32 KB and the core uses about 3.9 KB, so 18 KB fits with room to spare; on GBA the core passes the 8 KB tail (the platform may move its LUT cache there, freeing `gba_big` for the Cellar's 4 wall strips); native passes nothing. If `plat_cache_cap` is too small the PS1 falls back to the uncached path (still playable at 24-30 fps).

| RAM | Bytes |
|---|---:|
| resident `ps1/src/room.c` + `prims.c` | at most 8 KB of the 75 KB above the stack reserve |
| overlay group **`home`** (section 9.5): `home.c` 14 KB, `home_edit.c` 10 KB, `home_draw.c` 10 KB, `home_visit.c` 6 KB, `home_mart.c` 6 KB, `home_defend.c` 8 KB, `home_score.c` 3 KB = **57 KB code**; `furn_data.c`: 306 x 28 B catalog 8.6 KB, prims 306 x 6 x 8 B 14.7 KB, strings about 14 KB = **37 KB data** | **about 95 KB** of the 111,376 B slot (estimate; `ovl_plan.py` measures it at build) |
| VRAM | 6 room textures 64x64, one 128x128 atlas, about 20 decor sprites 32x32: all in the existing image cache pages |

If the group measures over the slot, the first cut is `desc` strings (a 160-entry shared phrase table saves about 6 KB), then `home_defend.c` moves its wave tables into the story group (it is only reachable from Ch3).

---

## 8. The catalog: 306 pieces

### 8.1 Categories and counts

`FURN_MAX` is 320; ids are append-only and saved as 9 bits. Prices are HaHaland dollars (H$); the shop's 30% deal of the day applies.

| `FCAT_*` | Count | Theme(s) | Price range | Where from |
|---|---:|---|---|---|
| PASTEL (Hillvile cute) | 36 | Pastel, Cloud, Candy, Cottage | 40-600 | HomeMart from day 1 |
| GOTHIC (goth-cute) | 28 | Goth, Bone | 80-800 | HomeMart after Ch2 / Hellvile visit; Black Market |
| HELLISH (`FF_HELL`) | 28 | Lava, Vapor | 120-1,500 | the Hellvile Black Market (Ch4 / free play after a Hellvile night), Breach drops |
| PLUSH (plushies and toys) | 24 | all | 30-300 | HomeMart, the Fun Fair's wheel, citizen thank-yous |
| KITCHEN | 26 | Cottage, Lava | 60-900 | HomeMart; appliances gated by fishcook recipe unlocks |
| TECH | 16 | Workshop | 150-1,200 | Workshop crafting (scrap, gears, hellsteel), the Mk I/Mk II quests |
| GARDEN | 26 | Picnic, Cottage, Bone | 20-400 | HomeMart (seeds), the Lake, the Graveyard shop, seasons |
| LIGHT | 16 | all | 50-500 | HomeMart; candles from the church; lava from the Cellar |
| WALL (wallpaper decals, posters, windows, doors, fences) | 20 | all | 30-300 | HomeMart; jukebox posters from studio unlocks; windows from Rivet |
| RUG | 12 | all | 60-400 | HomeMart; the pentagram rug from Vex |
| TROPHY | 30 | - | 0 | minigames 7, levels 8, fishing 5, concert 4, arena 3, misc 3 |
| SEASONAL | 20 | 4 seasons x 5 | 50-250 | HomeMart stock rotates with `town` month: spring (blossom branch, egg basket), summer (paddling pool, fan), autumn (pumpkin lantern, leaf pile), winter (tree, hearth stocking) |
| STORY (gag merchandise and `F:` rewards) | 24 | - | 0 | side missions and chapters |
| **Total** | **306** | | | |

### 8.2 Sample rows (the recipe file format)

```
# core/data/furn/pastel.txt - one piece per block; furngen -> core/gen/furn_data.c
[bunny_beanbag]
name  "Bunny Beanbag"
desc  "Sinks when sat on. Sighs when you leave."
cat   SEAT    price 120   unlock -      flags SURFACE|ANIM
foot  1 1 24   seat_dz 10  paints BUBBLEGUM MINT   score cute 6 spooky 0 comfy 7   need HAPPY
prim  blob   0 0 10   14 14 10   A.soft
prim  blob  -6 -6 20   5  5  4   A.soft      # ear
prim  blob   6 -6 20   5  5  4   A.soft
prim  box    0 0 2    15 15  2   B.flat      # base
anim  2 frames: squash 0.96/1.04 on sit
```

```c
/* furn.h (new, owner: home). Append-only ids; FURN_COUNT <= FURN_MAX = 320. */
typedef struct DoomoFurn {            /* 28 B + strings                           */
    const char *name, *desc;          /* <= 20 / <= 40 chars                       */
    uint16_t price;                   /* H$, 0 = gift / story only                 */
    uint16_t unlock;                  /* unlock id gate (any team's range), 0xFFFF open */
    uint8_t  cat, flags, interact;    /* FCAT_*, FF_*, FI_*                         */
    uint8_t  fw, fd, fh;              /* footprint tiles 1..4, height px/4          */
    uint8_t  paint_a, paint_b;        /* default paints, 0xFF = fixed               */
    int8_t   cute, spooky, comfy;     /* score contributions                        */
    uint8_t  need;                    /* STAT_* restored by FI_* use, 0xFF none     */
    uint8_t  seat_dz, light_r;        /* seat/sleep offset, lamp radius (tiles)     */
    uint8_t  theme;                   /* THEME_* (for Style / Harmony)              */
    uint8_t  mass;                    /* barricade HP contribution (0 = none)       */
    uint16_t prim0; uint8_t nprim;    /* recipe in doomo_furn_prims[]               */
    uint8_t  frames;                  /* GBA animation frames                       */
} DoomoFurn;
enum { FF_WALL = 1, FF_RUG = 2, FF_SURFACE = 4, FF_SMALL = 8, FF_LIGHT = 16, FF_ANIM = 32,
       FF_HELL = 64, FF_BILLBOARD = 128 };          /* FF_PORTAL and FF_CURSED live in a 2nd flag byte (flags2) */
enum { FI_NONE, FI_SIT, FI_SLEEP, FI_EAT, FI_COOK, FI_PLAY, FI_WATCH, FI_MUSIC, FI_READ,
       FI_BATHE, FI_PRAY, FI_SUMMON, FI_EXAMINE, FI_TURRET };
```

### 8.3 Named pieces by category (a representative 110 of the 306)

**PASTEL (36):** Bunny Beanbag, Cloud Bed, Cupcake Lamp, Marshmallow Sofa (2x1), Macaron Stool, Candy-Cane Shelf, Heart Mirror, Strawberry Fridge, Teacup Table, Bubble Chair, Rainbow Bookcase, Pastel Wardrobe (back variant), Daisy Rug, Pink Door (skin), Cloud Shelf (wall), Jellybean Jar (small), Sprinkle Vase, Plush Ottoman, Cherry Clock, Honey Lamp, Egg Nest (story, also pastel), Picnic Crate (the Ch1 crate: becomes a table), Nana's Rug.

**GOTHIC (28):** Coffin Bed (2x1), Bone Chandelier (ceiling-hung wall piece), Candelabra, Raven Perch, Gargoyle Bookend (small), Velvet Chaise (2x1), Cobweb Curtains (wall), Skull Planter, Iron Bed, Tombstone End-Table, Bat Mobile (ANIM), Stained-Glass Window (skin), Portrait That Follows You (ANIM: the eyes are 2 frames), Count Snoozula's Pillow (snores).

**HELLISH (28):** Lava Lamp (actually lava: a lamp whose pool is red and whose `spooky` is 8), Pentagram Rug (plush; cursed), Brimstone Hearth, Imp Stool, Chained Chandelier, Vapor Tank (ANIM bubbles), Bone Throne, Cloud Nine Vending Machine, Hellsteel Shelving, Sulphur Candle, Gate Fragment (a Ch4 Hellvile trophy), Rivet's Welded Fence (garden), Smog Curtains, Nox's Desk Lamp (a gift from the Hellvile mayor).

**PLUSH (24):** Mr. Flops (Pip's bunny, borrowed: he goes home at night), Horned-Poop Plushie (gag), Deflated Vapula Plush (sighs), Widow's Plushie (cries at 3 AM, comforts the pet), Mr. Huggins (spare head), Father Fuzz Plush, Cupcake Plush, Grape Cloud Plush, Plain Human Figurine (never animates), Kevin's Three-Eyed Bust (third eye blinks every 83+ frames), Imp Doll (always facing you; cursed), Jumbo Dice, Toy Train Loop (ANIM, 2x2).

**KITCHEN (26):** Strawberry Fridge, Brimstone Oven, Cloud Flour Sacks, Pickle Barrel, Ramen Counter (2x1 surface), Takoyaki Griddle, Kettle (ANIM steam), Spice Rack (wall), Cake Stand (small), Bagel Mountain (decorative, 1/10 scale), Nana's Recipe Board (wall), Grave Cake Display, Lava Cake Mould, Fish Smoker, Sock Stew Pot (yes).

**TECH (16):** Toaster Turret (FI_TURRET; the Ch3 piece), Vapor Detector Mk I, Mk II (sulking), Tube TV (ANIM), Jukebox (plays studio songs; FI_MUSIC), Console (FI_PLAY), Welding Rig (decorative, sparks every 11 frames), Gear Clock, Neon "it's fine." Sign, Forklift Pallet (empty), Weather Vane (points at the Wall, and after Ch4 spins), Record Player.

**GARDEN (26):** Picket Fence / Welded Fence / Bone Fence (ring pieces), Flower Bed x6 colours (3 growth stages), Tomato Pot (humming), Sunflower (grows past the fence), Swing Chair, Picnic Set (2x2), Bird Bath (ANIM), Garden Gnome (Hellvile variant: gnome with horns), Paddling Pool (seasonal), Compost Heap (Spook +4, Comfort -2, flies), Paper-Boat Pond, Small Cloud Stone (the Mayor's), Balloon Letters (the Ch2 ones, kept), Gate Arch (the Ch4 south opening).

**LIGHT (16):** Cupcake Lamp, Honey Lamp, Nightlight (Miss Lulla's), Window Candle (Ep 1.6), Church Candle (counts double for LIT), Paper Lantern, Lumina's Lantern, Lava Lamp, Sulphur Candle, Fairy Lights (wall, ANIM twinkle), Firefly Jar (small), Chandelier (3 kinds), Streetlamp (garden), the Starer's Cold Light (a lamp with no visible source; post-game).

**WALL (20):** 16 jukebox posters (one per unlocked song), Kids' Wall Drawing, Pip's Bee Drawing, Framed Candid, Height Marks, Hug Counter, Tomato Review Board, Badge Board, Counter Board, Boarded Window (smiley), Round / Stained / Bone / Boarded window skins, 6 door skins.

**RUG (12):** Daisy, Cloud, Candy Stripe, Checker, Bone, Lava Crack (ANIM glow), Pentagram (plush; cursed), Picnic Blanket, Welcome Mat ("it's fine."), Hellvile Welcome Mat ("it's fire."), Tuck-In Blanket (bed topper), Rivet's Oil Rag.

**TROPHY (30):** Cupcake Catch gold cupcake, Whack-an-Imp Head (Fuzz), Derby ribbon, Dodge star, Wheel of Fate wedge, Memory heart, Gallery frame (minigames 7); one plaque per Hell Breach rank S (8); Fish Trophy (humming), the Biggest Catch mount, the Smallest Catch (a jar), Fishdex plaque, the Golden Hook (fishing 5); Gold Record, Choir Bell, Concert Poster (framed), the Broken Mic (concert 4); Arena Belt, Boss Rush Skull, Time Trial Clock (3); Hellvile's Biggest Spoon (the Feast, saved branch), the Oversized Headstone miniature (the Feast, lost branch), Runner-Up Ribbon (3).

**SEASONAL (20):** spring Blossom Branch, Egg Basket, Rain Boots, Bee Box, Kite; summer Paddling Pool, Fan, Ice Box, Sandcastle, Hammock; autumn Pumpkin Lantern, Leaf Pile (ANIM), Scarecrow (Hellvile: scare-imp), Cider Barrel, Harvest Wreath; winter Tree (decorations are stackers), Hearth Stocking, Snow Imp, Hot Cocoa Stand, Wool Rug.

**STORY (24):** DAYS SINCE LAST FIRE sign, the Big Chair, Little Black Book, Welded Plate (battle scar), Window Candle, Cuckoo (out), Slow Clock, Spoon Rack, Tiny Spoon, Support Group Chair, Judge's Bib, Folded Smog Gown, Washed Cup, Teacup (cloud foam), Rubber Duck (southbound), Facing Duck, Hauntessa's Duster, Lunchbox, Leech Jar (decorative), One Page Two Names, Wobblewisp's Favourite Vase, Chalk Box, Polish Tin, Skull Mittens, Small Bell, Buckwheat Sack, Cookie Tin (two-sided), Toy Balloon, Kevin's Hat Stand, Boo-Boo Sticker Sheet. (The heart bible names 52 `F:` rewards; the ones not listed here are small variants of the same sprites and count against the 24 plus WALL.)

### 8.4 How you get them

| Source | Mechanism | Count (approx.) |
|---|---|---|
| **HomeMart** (`SCENE_HOME_MART`, a door inside the free-play Shop and a stall at the Hillvile market in story) | the existing shop logic (`fp_shop.c`: 12 daily seeded stock, 30% deal, locked rows shown as "???"), 5 tabs by category, a "Wants" tab that lists what your visitors wished for | 150 |
| **Hellvile Black Market** | the same scene with `FF_HELL` stock, open at night, prices in H$ but the clerk only accepts them after you have "done a shift"; after Ch4 it opens by day | 40 |
| **Side missions** (heart bible section 12, `F:` rewards) | granted at quest completion into storage with a "NEW!" bit; the questlog's Items tab shows them | 52 |
| **Chapters** | the crate (Ch1), Window Candle (Ep 1.6), the Balloon Letters (Ch2), Welded Plates (Ch3), Hellvile pieces at the Annex housewarming (Ch4), the Feast trophies (post) | 14 |
| **Workshop crafting** | `SCENE_WORKSHOP` gains a "Furniture" bench: recipes of `ITEM_SCRAP`, `ITEM_GEARS`, `ITEM_HELLSTEEL`, `ITEM_PLUSH_STUFFING`, `ITEM_GLITTER`, `ITEM_DEMON_HORN` plus a welding minigame grade that sets paint B (S = a gold accent) | 24 (TECH 16 + 8 hellsteel GOTHIC/HELLISH) |
| **Trophies** | `unlock_grant` hooks: when a trophy unlock bit is set, `home_grant(FURN_x)` runs (one table of `{unlock id, furn}` pairs read by `home.c` at load, so no other module changes) | 30 |
| **Friendship gifts** | at friendship levels 20/40/60/80 a citizen gives a piece from their `like_hobby` table in their district's style; fulfilling all 3 room wants gives a citizen-crafted piece (24 unique) | 24 + per-citizen |
| **Breach drops** | mini-bosses drop one `FF_HELL` piece on first kill at rank A or better (Gumdrop Gutterking's gumball lamp, Foreman Fluffnuts' conveyor table, the Weeping Widow's plushie, Bishop Bonebottom's lectern) | 8 |
| **Seasons** | HomeMart stock includes the season's 5 pieces; a piece bought out of season costs 3x from the "Last Season" tab | 20 |
| **The Feast / post-game** | Hellvile's Biggest Spoon (saved) or the Oversized Headstone miniature (lost); the starer's Cold Light after 20 morning sightings | 3 |

Ownership is nibble counts in `HOME.owned` (0..15 of each). Trophies and story pieces are capped at 1 and cannot be sold. Selling back is 40% of price.

---

## 9. Data formats and budgets

### 9.1 Platform API (additive to `platform.h`)

```c
typedef struct DoomoRoomObj {          /* render-ready; the core fills it per frame     */
    uint16_t furn;                     /* 0..319                                        */
    uint8_t  x, y, z;                  /* tile, tile, height px                         */
    uint8_t  rot, paint_a, paint_b;    /* 0..3, PAINT_* or 0xFF                          */
    uint8_t  anim;                     /* frame / state (3 bits of Obj state)            */
    int8_t   sq_x, sq_y;               /* squash -64..63 (/128): DROP, ripple, wobble    */
    int8_t   jitter;                   /* possessed shake amplitude                      */
    uint8_t  flags;                    /* RO_GHOST, RO_INVALID, RO_FULLBRIGHT, RO_SEMI, RO_SELECTED */
} DoomoRoomObj;                        /* 12 B */
typedef struct DoomoRoomActor {
    const DoomoImage *img;             /* chibi (GBA, native) / figure (PS1)             */
    fix16 x, y; int16_t z;             /* tile space 16.16                               */
    int16_t sx, sy;                    /* Q8.8 squash (PS1, native; GBA ignores)         */
    uint8_t flags;                     /* BB_FLIPX, BB_TRANSLUCENT, BB_SHADOW, RA_PLAYER  */
} DoomoRoomActor;
typedef struct DoomoRoomView {
    uint8_t  w, d, wallpaper, floor, wall_paint, floor_paint, ceiling, room_kind; /* room_kind: walls / garden */
    const DoomoRoomObj *objs; int nobj;
    const DoomoRoomActor *actors; int nact;
    uint16_t gen;                      /* bump on any layout/paint change (GBA cache key) */
    int16_t  yaw;                      /* 0..4095; GBA snaps to 4 views, PS1 orbits       */
    uint8_t  zoom;                     /* 0 overview, 1 close                             */
    int16_t  punch_q8;                 /* 256 = none                                      */
    int8_t   pan_x, pan_y;             /* tiles*16                                        */
    uint8_t  daylight, tint, tint_level, weather;
    int8_t   cursor_x, cursor_y;       /* -1 = hidden (GBA: OBJ)                          */
    uint8_t  sel_mask[16];             /* multi-select rubber band, 1 bit per tile row    */
    uint16_t time;
    int16_t  vx, vy, vw, vh;           /* viewport                                        */
    void    *plat_cache; uint32_t plat_cache_cap;   /* scratch the platform may use       */
} DoomoRoomView;
void plat_room(const DoomoRoomView *v);
void plat_screen_fx(int dx, int dy, int zoom_q8, int fade16);   /* weak no-op; GBA BG2/BLDY, PS1 E5 + TILE */
```

The core ships a weak reference `core/src/room_ref.c` (iso drawn with `plat_image` and `plat_rect`, full redraw every frame) that native tests and screenshots use; GBA and PS1 override it. A parity test compares the reference against a GBA harness shot of the same layout (section 11.4).

### 9.2 Save chunk `'HOME'` version 1 (worst case 2,609 B)

```
Header (8 B)
  u8   ver_flags        bit0 moved in, bits1-3 rooms unlocked (1..6), bit4 annex open, bit5 south fence open
  u8   active_room      0..5
  u16  house_points     best score ever (the Library's almanac reads it)
  u8   theme            last applied THEME_* (0xFF none)
  u8   defence          lo nibble best wave held, hi nibble plates owned
  u16  days_since_fire  the sign's counter (reset by Rivet's visits)
6 x Room (10 B header + nobj x 6 B; nobj <= 40)
  u8   wallpaper, floor           0..31, 0xFF default
  u8   wall_paint, floor_paint    PAINT_* (0..31), 0xFF default
  u8   size                       lo nibble w-1, hi nibble d-1 (<= 16x16)
  u8   flags                      bit0 lights on, bit1 curtains, bit2 haunted, bit3 plated, bits4-6 ceiling tint
  u8   nobj
  u8   name                       room-name table index
  u8   window_skins               2 bits per window slot (4 slots)
  u8   door_skin                  0..5 lo, bits 3-7 reserved
  nobj x Obj (6 B)
    u16 w0                        bits0-8 furn (0..319), bits9-10 rot, bit11 on_surface, bits12-15 sub-pos (half-tile x,y)
    u8  pos                       lo nibble x, hi nibble y
    u8  z                         height px/4 (stackers: surface top; wall pieces: height)
    u8  paint_a                   PAINT_* lo 5 bits (0x1F default), bits5-7 state (lamp on, door open, growth stage)
    u8  paint_b                   PAINT_* lo 5 bits (0x1F default), bits5-7 reserved
u8   owned[160]         nibble counts for 320 ids (0..15 each)
u8   seen_new[40]       "NEW!" bits
32 x Gift (4 B)         u16 resident(10) | x(3) | y(3); u16 furn(9) | rot(2) | paint_a(5)
u8   visited[125]       1000 bits
u8   haunt[16]          Ch2 state: night counter, which piece moved, wallpaper-eye position, seed
2 x Blueprint           one Room each (10 + 40 x 6 = 250 B)
16 x Photo (8 B)        room(3) yaw(2) zoom(1) filter(4) daylight(4) | 3 stickers x (kind 3, x 5, y 4) | hash16 | visited id(10) spare
u8   wants_done[4]      per-citizen want completion lives in the Gift table's presence; this is the 4 weekly counters
```

| Part | Bytes |
|---|---:|
| header | 8 |
| 6 rooms, full | 6 x 250 = 1,500 |
| owned + seen_new | 200 |
| gifts | 128 |
| visited | 125 |
| haunt | 16 |
| blueprints | 500 |
| photos | 128 |
| counters | 4 |
| **worst case raw** | **2,609** |
| typical (3 rooms, 15 pieces each, 2 photos, 1 blueprint) raw / LZ | about 900 / **350-600** |

The `size()` callback emits exactly what `write()` does (rooms are written with their real `nobj`, so an empty house is 8 + 60 + 200 + 128 + 125 + 16 + 20 + 128 + 4 = 689 B raw, compressing to about 60 B).

**Slot impact (worst case):**

| Slot | Today (measured) | With `HOME` | Cap |
|---|---:|---:|---:|
| story | 13,504 B | 16,113 B (15.7 KB) | 40 KB |
| free play | 59,148 B | 61,757 B (60.3 KB) | 88 KB |
| free play + the customisation design's `LOOK` (8 KB GBA) + `OPTX` + `UNL2` | | about 70.5 KB | 88 KB |

### 9.3 The undo ring and the working copy (scratch, not saved)

```c
typedef struct HomeUndo { uint8_t op, idx; uint8_t old[6]; } HomeUndo;   /* 8 B; OP_PLACE, OP_REMOVE, OP_MOVE, OP_PAINT, OP_WALL, OP_THEME (2 entries), OP_GROUP (n+1 entries) */
```

32 entries = 256 B. A group move of 8 pieces costs 9 entries (the header entry stores the count), so the ring holds at least 3 full group moves. Undo is per room; switching rooms clears it (the bottom bar says so).

### 9.4 PS1 memory card

Story save 13,504 B + 640 B of header frames + `HOME` compressed. Blocks are 8,192 B. With a typical house (about 500 B) the file is about 14.6 KB: **2 blocks**, as today. The only way to reach 3 blocks is a 6-room, 240-piece house with 2 full blueprints and 16 photos on an "all premade, lived in" island (about 2.2 KB compressed, total about 16.4 KB, 40 bytes over). `memcard.c` already writes 1-12 blocks, so this costs one more second on the card, not a failure; the save screen shows the block count.

### 9.5 PS1 overlay group

Add to `ps1/tools/ovl_plan.py` `GROUPS`, before `"story"`:

```python
("home", r"home(_\w+)?$|furn_data$|room_ref$"),
```

Everything in `home*.c` must only reference `doomo_furn[]` and `doomo_furn_prims[]` from inside the group, or `ovl_plan.py` makes the catalog resident. HomeMart is therefore `home_mart.c` (in the group), reached from the free-play Shop through a door (`scene_push(&SCENE_HOME_MART, 0)`), not a tab. `unlock_grant` hooks for trophies go through `home_grant()` **called by `home.c` at load** (it scans the unlock bits), so no resident module holds a pointer into the group. Story code references the house only through `scene_push(&SCENE_HOME, arg)` and the `HOME` chunk's getters in a small resident `home_api.c` (about 1 KB: `home_grade(room)`, `home_stat(stat)`, `home_grant(furn)`, `home_has(furn)`, `home_days_since_fire()`), which read the chunk's state, never the catalog.

Scene switches: story -> house is one CD read of the `home` group (about 95 KB, about 0.4 s); house -> dialogue costs nothing (resident); house -> exorcism or combat is two reads. **Hold the Door stays inside `SCENE_HOME`** (section 10.3) for this reason: no smash cut in the siege may cross a group.

### 9.6 Registry edits (one each, additive)

| Registry | Today | Change |
|---|---|---|
| `save_ext.h` / `save_ext.c` | 5 slots, all taken (`SONG`, `NICK`, test, `FPLY`, `FISH`) | turn the switch into a 12-slot array (integration owns it); `HOME` takes slot 5; the customisation design's `LOOK`, `OPTX`, `UNL2` take 6-8 |
| `activities.h` / `activities.c` | 6 tables, all taken | `doomo_activities_home` (slot 7): "Decorate my home" (`ACTLOC_TOWN_HALL` in free play, your door in story), "Visit a neighbour" (`ACTLOC_APARTMENTS`, `ACTF_NEEDS_RESIDENT`), "HomeMart" |
| `scenes.h` | 35 scenes | `SCENE_HOME`, `SCENE_HOME_MART` |
| `scene.h` scratch users list | | add `HOME  home*.c  editor state, visit room, defence` |
| `unlock_ids.h` | spare: foundation 202..239, action 564..607 | re-assign **564..595 to "home"** (32 ids): 20 trophies (first house, COSY, LIT, HELD, HOME, 100 pieces owned, every category, all 6 rooms, 50 homes visited, 500 visited, 1000 visited, a 5-star rating, 10 photos, a full blueprint, Harmony 100, the Feast spoon, the starer seen 20 times, a complete seasonal set, the Plain Human bought, every gag piece) + 12 raw bits (which rooms' tutorials ran, defence tutorial, black market opened) |
| `items.h` | 63 of 64 | **unchanged**; furniture is not an item |
| `story.h` flags | bits 228-255 free | Ch3 uses 4 (`SF_HOUSE_PLATED`, `SF_HOUSE_TURRET`, `SF_HOUSE_HELD`, `SF_HOUSE_WALL_GONE`); the rest of the house's story state is in `HOME` |

---

## 10. Story integration

### 10.1 The house across chapters (from `bible_heart.md` section 3, made concrete)

| Chapter | State | Rooms | Decorating goal -> grade | Sincere beat in the house | New pieces |
|---|---|---|---|---|---|
| **1** | Cosy | Living (Ep 1.2), Bedroom (Ep 1.5) | "Make it yours": 6 pieces including a bed and the Egg Nest -> **COSY** (Comfort 40). The egg hatches only in a cosy room. | the hatch; the first "beep." (the smallest bubble in the game, R21 of nothing first, music muted 29 frames) | the Picnic Crate (becomes a table), Nana's Rug, Pip's drawing, Fuzz's cushion, Kevin's bust, Bonejangles' xylophone table, Miss Lulla's nightlight, the Mayor's framed photo of himself, Dr. Bloom's thermometer, Rivet's welded coat hook (left on the step), the Window Candle (Ep 1.6) |
| **2** | Haunted | + Kitchen (Ep 2.1), Garden (Ep 2.4), Hell-Cellar (Ep 2.6) | "Keep the lights on": 3 lamps, candles count double -> **LIT** (Light 60). Each lamp shortens the night's whispers; at LIT the wallpaper eye stops blinking. | reading Rivet's family letters by lamplight; the pet's first horn, in its sleep (R13, then "beep." that simply appears) | Balloon Letters, the Vapor Detector Mk I, the pentagram rug (Vex "gives" it), Church Candles |
| **3** | Barricaded | all unlocked rooms; the Living Room is the shelter | "Hold the Door": Barricade >= 100 and the Toaster Turret placed -> **HELD** (Ep 3.5 outcome: 4 guests safe) | the Longest Night: one wall gone, the pet missing, your first line (Interlude I-C) | Welded Plates (battle scars, kept), the Toaster Turret, Boarded Window (smiley), Hauntessa's Duster |
| **4** | Open | + Roof Annex (Ep 4.3); the Garden's south fence opens onto Hellvile | "Open House": 8 seats, both styles, Annex furnished, Harmony >= 60 -> **HOME** | "Come in." Giggs and Nox at your table. Wisp asleep on Mr. Flops. (No gag. The button is after the credits.) | Hellvile pieces from the Annex housewarming (Nox's Desk Lamp, Imp Stool, Gate Fragment), the Big Chair, Little Black Book |
| **post** | Yours | all 6 | weekly ratings: three visitors 15 frames apart, escalating, then a topper | the morning the starer is at the window (1 in 20) | the Feast trophies, the starer's Cold Light, seasonal rotation |

Every chapter's house state is derived from `HOME` plus story flags: the Ch2 haunting reads `haunt[]`, Ch3 reads `flags.plated` per room and the four `SF_HOUSE_*` bits, Ch4 reads `ver_flags.annex_open`. There is no second copy of the house.

### 10.2 Chapter 2: the haunting (rule of three across three nights)

Ep 2.2 *Things Under the Wallpaper* replaces the "talk to 3 weird people" counter with your own house:

- **Night 1:** at 03:00 the 3 AM move (section 5.4): the Picnic Crate is one tile to the left in the morning. The pet is staring at where it was. Nothing else. The game says nothing.
- **Night 2:** the lamp is off. Lighting it: a 4-frame insert, a face in the wallpaper hearts, every heart an eye. R2, `SFX_TIK`. Back: **one heart is still an eye.** It blinks every 83 frames. (`haunt[2]` stores its wall position.) The pet grows its first horn in its sleep. LIT unlocks.
- **Night 3:** two pieces move, toward the cellar hatch. The pentagram rug Vex gave you is under the bed, though you did not put it there (Comfort -10, Spook +15, the panel notices). The Cellar Key arrives (Ep 2.6).

Reaching LIT ends the moves and the eye stops blinking but **stays an eye** for the rest of the game (the one persistent change; it is in the wallpaper decal layer). Whispers are `audio_music_duck` to 0.4 plus a 5-syllable babble every R21 while any room is below Light 25 at night.

### 10.3 Chapter 3: Hold the Door (house defence, inside `SCENE_HOME`)

**Ep 3.1 (tutorial wave, daylight, 5 min).** Rivet arrives with 3 Welded Plates. The Build mode gains a **Shelter** tab: plates snap to window slots (`flags.plated`), the Toaster Turret has a slot at either window, the Score panel shows Barricade. One daylight wave of 3 imps at the door teaches: the door has HP = Barricade; repairing is A on the door (a 2-frame squash, +10 HP, `SFX_THUD`); the turret auto-fires every 12 frames at the nearest demon in its window's 90-degree arc and must be re-wound (A on it, 8 frames) every 20 shots.

**Ep 3.5 (the siege, 40 min).** The house is the map; no overlay switch happens.

| | Wave 1 | Wave 2 | Wave 3 |
|---|---|---|---|
| Interval | 0 | +90 s (a 30 s authored quiet in between) | +90 s |
| Door | 6 imps, 4 HP each, 1 door damage per hit every 16 frames | 10 imps + 1 brute (20 HP) at the **window** (`[SQUASH 1.3/0.8]` against the plate every 24 frames; a plate takes 10 hits) | 14 imps, 2 brutes, and the fog down the chimney (the pet beat, below) |
| Player | repairs the door, re-winds the turret, comforts guests (A on a guest: 2 hearts, +1 "safe" tick) | the same plus putting a plate back (B picks up a fallen plate, A re-places it with a DROP) | the same with everything at 2x clock (`[PANIC]`) |
| Guests | Pip + Mr. Flops, Miss Lulla + 2 kids, Nana, Kevin, the Mayor at the window "as a decoy" | guests flee a broken window (4-hop flee into the Bedroom); a guest in a room with a demon loses "safe" | the Mayor's decoy works once |
| Comic timing | each imp hit at the door is a `SHK_TAP` on the door sprite, not the screen; every 10th a rimshot | the brute's squash against the plate; Kevin's third eye watches the chimney (ANIM frame forced) | rule of three: three "the door is going to give" false alarms 15 frames apart (door jitter 1 px, 3 px, 5 px with a red flash on the third... which holds) |
| Scripted beat | - | the 30 s quiet: authored guest lines (`beat.h` script in the story group's text, called through `ui_say`) | the sky beeps; the fog comes down the chimney; `[FREEZE all]`, `[MUTE 14f]`; the pet, alone, unfrozen, walks to the fireplace: "bep?"; the fog, in the pet's voice, lower: "beep."; the pet goes up the chimney; `[FLASH 7]`; `[THAW]` `[PANIC]`; wave 3 is the longest |

**Outcome at dawn.** `SF_HOUSE_WALL_GONE` is always set (a wall is gone either way; in the room it is a 3-tile breach drawn as rubble with the Garden's sky behind it until Ch4 Ep 4.3 welds the Annex onto it). **HELD** stamps if 4 guests kept "safe" (all four cast lines in Interlude I-C). The Welded Plates remain as owned pieces (battle scars) and can stay on the windows forever; the Boarded Window (smiley) skin unlocks.

**Night Raids** (Ch3 nights, optional): sleeping at home has a 50% chance of 1 wave (drops `ITEM_SCRAP`); the sign counts.

**Post-game:** "Hold the Door" is an Arena activity that uses the house layout as the map (`home_defend.c` exports the room as waves + a spawn list; the arena draws it through `plat_room`, so it stays in the `home` group).

**GBA cost in wave 3:** 8 demon chibis + 6 guests + the turret exceed the 12-rect limit, so the renderer recomposes every frame (about 20 ms) and the logic runs the actors at 2x clock: 30 fps, section 6.6. Demons are bitmap chibis (`bodyspr_render` of 4 imp looks and 1 brute look, cached) so they occlude correctly behind furniture.

### 10.4 Chapter 4 and after

- **Ep 4.3:** Rivet welds the Roof Annex onto the breach (`SHK_RUMBLE`, then wall strips slide down `TW_SLAM8`); Hellvile neighbours arrive with housewarming gifts (9 arrivals at 8-frame intervals, mirroring Ep 1.5). The Garden's south fence opens: the second garden.
- **Ep 4.6, the Open House:** 8 seats, both styles, two tables pushed together; `@P: "Come in."` Giggs and Nox at your table. The scene is scripted with `beat.h`; the house's own vamp plays straight, no duck, no button. The button is post-credits (the poop grows horns in the pet's nest).
- **The Feast of Rebuilding** (user_notes.md): the Big Chair comes home either way; Hellvile's Biggest Spoon (saved) or the Oversized Headstone miniature (lost); if Grumbleguts is deceased his apartment is a sealed room and the Black Market's clerk says so.
- **Running-gag merchandise** behaves as section 5.2 describes. The DAYS SINCE LAST FIRE sign's counter is `HOME.days_since_fire`, reset by `story_rivet_visits()` (a resident hook in `home_api.c`).

---

## 11. Tests

### 11.1 Unit tests (`core/tests/test_home.c`, run by `make -C core test`)

| Test | Checks |
|---|---|
| `home_catalog_valid` | every `DoomoFurn` has a recipe, `fw*fd <= 16`, paints are marker-consistent, `FURN_COUNT <= FURN_MAX`, every `unlock` id is inside a declared range, `mass` is 0 for wall and small pieces |
| `home_place_rules` | footprints, rotation, stacking on surfaces, rugs under pieces, wall pieces only on walls, the fireplace rule, the door slot is immovable |
| `home_undo_ring` | 40 random ops then 40 undos reproduce the start layout byte for byte; a group move undoes as one |
| `home_save_roundtrip` | a full 6-room house -> `write()` -> `read()` -> identical; `size()` equals bytes written; an empty house is 689 B raw; the worst case is exactly 2,609 B |
| `home_save_caps` | story worst case (the measured 13,504 B island + full house) < 40 KB; free play worst (59,148 + full house) < 88 KB; both asserted on the real codec |
| `home_visit_determinism` | `home_visit_gen(id)` for all 1000 ids on two seeds: same result twice, FNV fingerprint matches a golden per platform (parity like `resident_fingerprint`); never contains a fear-trigger piece |
| `home_visit_time` | 1000 generations under 2 s native; the per-generation op count (through `portrait_last_ops`-style counting) under 60k so GBA stays at 3.5 ms |
| `home_score` | the formulas of section 4.1 on 6 fixed layouts with hand-computed answers; each feng-shui rule in isolation |
| `home_defend_waves` | wave tables sum to the spec; door HP = Barricade; a scripted wave 3 with no input loses exactly the Mayor's "safe" |
| `home_photo_recipe` | pack/unpack of all 8-byte fields; `hash16` changes when the layout changes |
| `home_registry` | `doomo_save_ext_home` and `doomo_activities_home` are registered (the volatile-read pattern), unlock ids 564..595 unique and inside the home range (extends `test_integration.c`) |
| `furngen_check_gen` | `make -C core check-gen` regenerates `furn_data.c` byte-identical (CI already does this for `art_data.c`) |

### 11.2 Runner scripts (`core/tests/scripts/home_*.txt`, played at 240x160 and 320x240 by `run_scripts.sh`)

| Script | What it shows |
|---|---|
| `home_tutorial.txt` | `# assert-no-stubs`; story Ep 1.2: enter, place the crate, nest, lamp; `shot home_empty`, `shot home_drop_frame3` (mid-squash), `shot home_cosy_stamp`; `expect-scene HOME` |
| `home_editor.txt` | rotate view x4 (`shot home_view0..3`), pick up, nope at the window, rotate held piece edge-on frame (`shot home_flip_edge`), recolour, wallpaper swap with the paint-roller runner (`shot home_roller_mid`), multi-select move, undo x3, theme apply, blueprint apply (`shot home_blueprint_ghosts`), photo |
| `home_rooms.txt` | walk through all 6 rooms via doors (`shot home_room0..5`), the Garden at dawn/noon/dusk/night, the Cellar dim |
| `home_visit.txt` | push `HOME 2+id` for a tidy Hillviler, a chaotic Hellborn, a possessed, a ghost, a dead, an away citizen (`shot visit_*`); place a gift; the double-take frames at +8, +24, +32 |
| `home_haunt.txt` | `scene STORY 2xx` night 2: the lamp, the insert frame (`shot haunt_insert`), the eye after |
| `home_defend.txt` | Ep 3.1 tutorial wave and Ep 3.5 wave 3 with the chimney beat (`shot defend_freeze`, `shot defend_dawn`) |
| `home_mart.txt` | HomeMart tabs, a deal, a locked "???", the Wants tab |
| `home_gallery.txt` | the Library's gallery wing re-rendering 3 photos |

Every shot is compared by eye at both resolutions (the project rule) and the PS1 320x240 layout is checked for the wider viewport (the room centres; the HUD bars stay 12 px).

### 11.3 GBA harness (`gba/harness`, libmgba)

`gba/scripts/home.txt` (added to `make -C gba test` as `test-home`): boot into a story save with the house (through `coresave1.txt`'s mechanism), then the same beats as `home_editor.txt` with `fps` labels: `fps home_steady` (target >= 28), `fps home_rotate` (>= 24 over the 10-frame hold), `fps home_cellar_full` (8x8, 40 pieces, >= 20), `fps home_defend_w3` (>= 20), `fps home_modal` (>= 20). `profile 2000000 plat_room` with `--nm` lists the hottest addresses; the acceptance is that no new IWRAM symbol appears in `arm-none-eabi-nm`. `gba/tools/memreport.py` must show `HOME` state <= 2,609 B and the EWRAM free figure the memory pass promised (>= 24 KB before `HOME`, so >= 21 KB after). Flash: `coresave` scripts extended with a house so the streamed save round-trips through the 17 KB window.

### 11.4 PS1 harness (`ps1/harness/run.py`, PCSX-Redux)

`ps1/harness/scripts/home.txt`: the same beats with `fps home_steady` (>= 28 in Redux, which is 1.5-2x slower than a console), `fps home_orbit` (>= 22), `vram home_cluts` (the 4 borrowed CLUT rows visible), `dshot` of the DROP mid-squash and the orbit overshoot frame, `expect-log "ovl: home"` (the group loaded) and `--assert-no-stubs`. `ovl_plan.py`'s report asserts `home` <= slot size and lists what it kept resident (the acceptance is: nothing from `home*` except `home_api.c`). `memcard_check.py` confirms the story save's block count (2 for the test house).

### 11.5 Parity

A golden test renders one fixed layout through `room_ref.c` natively at 240x160 and compares it with the GBA harness shot of the same layout (`test_render.c`'s golden mechanism): identical pixels except OBJ-drawn things (cursor, ghost), which the test hides (`cursor_x = -1`). The PS1 is 3D, so its check is the 0.4-4% differing-pixel budget the harness already uses for 2D screens, applied to the HUD bars only, plus the eyeball.

---

## 12. Build order, owners, risks

| Step | What | Owner | Depends on |
|---|---|---|---|
| 0 | **Memory pass merges** (task #77: EWRAM >= 24 KB free) | integration | - |
| 1 | Contracts: `furn.h`, `home.h`, `platform.h` `plat_room` + `plat_screen_fx`, the `save_ext` array (12 slots), `activities` slot 7, `scenes.h`, `unlock_ids.h` 564..595 -> home, `scene.h` users list, `tween.h` / `beat.h` / `camfx.h` from the timing bible | foundation + integration | 0 |
| 2 | `furngen` (integer-only; recipes in `core/data/furn/*.txt`; `check-gen`), a contact sheet like `artgen --sheet`, the first 60 pieces (Ch1 set + HomeMart basics) | art + home | 1 |
| 3 | Core: `home.c` (state, chunk, grants), `home_edit.c` (build mode, undo, multi-select, blueprints, themes, photo), `home_draw.c` (view assembly, HUD, panels), `home_score.c`, `home_visit.c`, `home_mart.c`, `room_ref.c`, `home_api.c`; `test_home.c`; runner scripts | home | 1, 2 |
| 4 | GBA: `gba/src/room.c` (THUMB, uses `ow_*`), OAM module, affine OBJ for the ghost, `plat_screen_fx`, `GBA_BIG_ROOM`; harness script; memreport check | gba | 3 |
| 5 | PS1: `prims.c` factored from `model.c`, `room.c`, CLUT rows, the vertex cache in `plat_cache`, `plat_screen_fx` (E5 + TILE), the `home` overlay group; harness script | ps1 | 3 |
| 6 | Story: `PLACE_HOME` "Go inside", Ep 1.2 tutorial, Ep 1.5 party arrivals, Ch2 haunting (`home_haunt` in `home.c`, scripted lines in the story group), Ch3 `home_defend.c` + Ep 3.1/3.5 scripts, Ch4 Annex + Open House, the `F:` reward grants in `story_side.c`, Rivet's sign hook | story + home | 3 |
| 7 | Content: the remaining 246 pieces, 12 blueprints, 10 themes, the 32 hobby and 48 food tables for generated rooms, HomeMart stock tables, the Black Market, seasonal rotation | home + art | 2 |
| 8 | Free play hooks: facade -> visit, care screen "Their home" and "Decorate their room", the cottage on the Hillvile facade, Town Hall "Expand", the Library gallery's photo wing, the almanac's house_points | freeplay + library | 3 |

**Risks and what to do about them**

1. **GBA EWRAM.** `HOME` is 2,609 B of 7,680 B free today. Step 0 is a hard gate; the fallbacks in section 6.5 recover about 600 B if needed.
2. **PS1 overlay size.** The `home` group is estimated at 95 KB of 109 KB; the catalog's strings are the first cut (section 7.3). `ovl_plan.py` will say exactly, and anything `home*` that it keeps resident is a bug to fix before merge (the 75 KB resident margin is shared with the customisation design).
3. **ROM.** 0.8 MB more art is fine for the 4 MB padded file (3.25 MB) but it is the biggest single jump the GBA ROM has taken; if the 306 pieces come in heavier than 2.1 KB average, drop back facings to the 20 that need them and bake animated frames as 2 instead of 4.
4. **The 12-rect fallback.** The defence night recomposes every frame; the estimate says 20 ms. If the measured recompose of a 40-piece room is above 28 ms, wave 3 caps at 10 demons on screen (the others queue at the door) rather than dropping below 20 fps.
5. **Save growth across designs.** With `HOME` + `LOOK` + `OPTX` + `UNL2` the free-play worst case is about 70.5 KB of 88 KB; the PS1 story save's 2-block guarantee has a 40-byte edge case (section 9.4). Both are documented, neither is a correctness issue.
6. **Timing drift.** Every house beat must hang off `beat.h` labels, not frame constants copied by hand (the trailer's `music.c` `FR(n)` coupling is the cautionary tale). The code review checklist for step 6 is: no literal frame counts in `home_*.c` outside the tables of section 5.

**What this buys:** a house that is the story's emotional anchor and its scariest room, a Tomodachi-style town of 1000 visitable homes for 0 save bytes, a 306-piece catalog that pays off every minigame, quest and running gag, and two renderers that each do the one trick their hardware is best at (a static bitmap with hardware sprites on the GBA, a cached GTE diorama on the PS1), all animated with the trailer's exact tables so that placing a beanbag feels like the splash screen's heart landing.
