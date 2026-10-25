# show/series/sets - every set of the pilot (ART track)

Every location the pilot's scenes name has a set here. The generators in `show/art/gen/`
are the source: plates are never painted by hand and never committed; the compiler's bake
phase (showc phase 5) calls them and caches the indexed PNGs. Everything is drawn in
master-palette indices (`core/src/palette.c`, read-only), so every 2D frame is 100 %
on-palette (PIC-PAL, AT5.3).

    show sets list | check [-v] | sheet [SET ...] [--out DIR] | bake SET OUTDIR
    (until cli.py loads commands/sets.py:  cd show/tools && python3 -m showtk.art.setkit ...)
    python3 -m unittest discover -s show/tests/art -p 'test_sets*.py'

## 1. The cheapest route per location

| Beat(s) | Location (time) | Set | Route | Why |
|---|---|---|---|---|
| CO-1 | Crayon calendar page | `calendar_page` (px=2) | new plate + STROKES | the circle and the fireworks scribble are live strokes (reveal, boil) |
| CO-2, CO-3 | Storybook park, one park / with the Wall (night) | `park_storybook` | new crayon plates | one frame for co02 and co09: the Wall is its own plane, `later` drains the pink (a LUT) |
| CO-2 co03 | The bridge | `bridge_storybook` (px=2) | new | the rail is an order=front plane (kids walk behind it) |
| CO-2 co04/06/07, co17 | The sky over the bandstand, the hand | `sky_storybook` | new | one tall sky (tilt 12 px); the hand and the slipped loop are props |
| CO-2 co05 | The soup table, top-down | `soup_table_storybook` (px=2) | new | the two bowls are props |
| CO-2 co08 | The river, the cloud drifts south | `river_storybook` | new | a 456-px plane for the 0.5 px/f pan |
| CO-4 | The storybook banner | `banner_storybook` | new | MINGLE and the GIGGLE layer (reveal by wipe) share the pixel banner's letter grid |
| CO-5 | Bandstand at dawn, the banner, the ladder | `bandstand_banner` | new (MS plates + props) | letters on the crayon banner's baseline (match cut); bloom1-3, the drip, the balloon are props/states |
| CO-5 co14 | The fountain's two faces (insert) | `fountain_faces` (px=2) | new | |
| CO-6 co19, 1-03 | Fountain plaza / the park gate (dawn, sunset) | `fountain_plaza` | new | one plate, two palette variants; the gate under the GIGGLE banner and balloon (continuity) |
| CO-6 co20 | Nana's bakery window (dawn) | `bakery_window` | new | basket, egg, lamp, tag are props |
| CO-6 co21 | The egg (CU) | `egg_basket` (px=2) | new | |
| MT-1 | HaHaland from above | `hahaland_crane` | **OVERWORLD on the game's `hahaland` map** | the map exists; a camera move is all it needs |
| MT-2 | The cast pops in | `cast_popin` | new (reuses `candy_hills`) | a pastel stage for pop-ins |
| MT-3 | Pan to the Wall | `hahaland_wall_pan` | **OVERWORLD `hahaland`** (the Wall and gate at x 56) | |
| MT-3, MT-6, 4-08 | The split screen | `wall_split` | new | both sides in one plate, the Wall on x = 160; Hellvile is silhouettes only |
| MT-4, 4-03, PC-1 | The sealed gate (night) | `sealed_gate` | new | the Wall's top in frame (the lantern drifts over it); the door is a prop (it bulges in PC-1) |
| MT-5 | The first-person corridor | `title_corridor` | **VIEW3D on the game's `vape_cellar`** (its pastel back room) | heart wallpaper + candy-stripe ceiling read as the cute side; HELLIFY + the HUD do the truth |
| MT-6 | The logo end pose | `logo_card` | new (the logo drawn the way `fp_title.c` draws it) | |
| MT-7 | The pilot title card: GIGGLE NIGHT | `title_card` | reuses the banner MS plates | |
| 1-01 | Candy Hills Road (sunset) | `candy_hills_road` | new | the pink bus is a prop (drive/stop) |
| 1-02 | The town sign (sunset) | `town_sign` | new | words are strings; the digits are prop tiles (the 1 falls off); the painted-over 1000 is paint (art) |
| 1-04 | Hook-a-Duck (dusk) | `hook_a_duck` | new | six ducks are props; one faces the Wall |
| 1-05 | Face painting (dusk) | `face_painting` | new | |
| 1-06 | Hattie's tent (dusk) | `hattie_tent` | new | |
| 1-07 | Miss Lulla's craft tent (dusk) | `lulla_tent` | new | eleven drawings pan; the Cloud Nine Vapor sign is a prop (flicker state) |
| 1-08, 1-12, 1-14, 2-01..2-07, 3-04, 3-06, 3-07, 4-01..4-06 | The bandstand (dusk, night, dark, vape, torn, rite) | `bandstand` | **new TILEMAP** (800x240, 16x16 tiles) | the heart of the episode; one plate, named framings (`wide`, `north`, `steps`, `boards`, `gate`, `fair`) |
| (CUs on the above) | Bandstand close-ups | `bandstand_cu` (px=2) | new | backings for busts: north, boards, steps, wall |
| 1-09, 3-02 | The bake tent (night) | `bake_tent` | new | twin pies, muffins are props; the sign is a string (BAKE TENT -> BAKE TEN) |
| 1-10, 1-11, 2-03, 2-04, 3-01 | The fun fair (night, panic, cute) | `fun_fair` | new | teacups, the Not Scary House, the low lantern string; the LOST PROPERTY lantern is a prop |
| 1-13 | The whole Giggle at 21:00 | `giggle_wide` | new | chibi scale, the town clock at nine |
| 1-13 insert | Beyond the Wall: the pipe's sigh | `pipe_yard` | new, minimal | no faces |
| 1-14 insert | The kids' tent eleven years ago (sepia) | `kids_tent_memory` | new | the cloud lady and the crowned silhouette are cast |
| 2-08 | Whack-an-Imp booth, Rivet's prize case | `whack_an_imp` | new | imps, prizes and tags are props (tags are strings) |
| 3-03 | INT. Absolutely Not Scary House (FP) | `scary_house_fp` | **VIEW3D `vape_cellar`** | |
| 3-05 | INT. Bake tent (FP) | `bake_tent_fp` | **VIEW3D `vape_cellar`** (its tiled room under the candy-stripe canopy) | |
| 4-07 | The joke graveyard path | `joke_graveyard` | new | FLUFFY's stone and its cupcake are props |
| 4-08 | Launch hill | `launch_hill` | new | the Wall below, a big sky for one firework |
| T-1, T-2 | INT. Pink Door Cottage (dawn) | `cottage` | **ROOM: the game's house** (`HOME_COTTAGE`, living room, `HP_EMPTY`) | a crate and a window are furniture objects |
| CR-1 | End credits: storybook stills | `credits_storybook` (px=2) + `credits_card` | new | six crayon stills as variants; the head card has peeled to MINGLE NIGHT |
| EC1-EC3 | Eyecatch cards | `eyecatch` | new | OUT/IN pairs as variants; the clock is a prop |
| act cards | The Mayor's clipboard | `act_cards` | new | every line is a prop (pops 8 f apart) |
| head | TV-MA | `tvma_card` | new | |
| PC-1 end | Dippinn Productions | `dippinn_card` | quantised from `docs/media/dippinn_productions.png` (read-only) | |

Not reused, and why: the game's `park_united` overworld is the *finale's* park (the Wall
down) and its fair is the wrong festival; the trailer's plates are C code in `trailer/`
(a reference for the look, never a renderer, spec 4.2); `CUTSTAGE` cannot exist (spec 8.3).

## 2. Time of day = palette variants

A variant swaps the plate's CLUT; the pixels never change. `show/art/gen/light.py`:

| light | used for | how |
|---|---|---|
| `day` | noon, cards | identity |
| `dawn` | CO-5, CO-6, the tag | soft peach-pink (family-preferring nearest, like artgen's `nearest_fam`) |
| `sunset` | 1-01..1-03 | warm rose-gold |
| `dusk` | 1-04..1-08 | lavender blue hour; emissives kept |
| `night` | 1-09 on | moonlit blue; **emissives kept lit** (the NIGHT tint's rule: lantern cores, lit windows, red eyes) |
| `game_night` | (spare) | the game's own `TINT_NIGHT` table verbatim |
| `dark` | the lanterns out (1-14, 2-01, 2-03) | night, nothing kept lit |
| `vape` | (spare) | violet; the 21:00 flood is better done as camfx `PAL_LILAC` on the shot |
| `sepia` | the 1-14 memory insert | |
| `drained` | co09, the pink drained out of the crayon | |

Skies are BANDS planes: `bands=sky:<light>` is a list of (palette index, rows) per time of
day (`light.SKY_BANDS`, drawn as flat TILE bands with one-row stripe seams; no Gouraud).
Stars are a 128x80 `tile=` plane. Lantern light pools are a `light=fixed` plane that only
shows when the lanterns are lit.

## 3. The set grammar (what the compiler reads)

One statement per line, `#` comments, `end` closes a block (spec 5.2). Values never contain
blanks; a generator's text uses `_` for a space.

```
set <id> kind=2d|view3d|overworld|room px=1|2|3 matte=open|black look=pixel|crayon desc="..."
  plane <id> image=<src>|tilemap=<src>|tile=<src>|bands=<bands> [size=WxH] par=<x>[,<y>]
             [at=x,y] [order=back|front|top] [light=fixed] [hidden=yes] [boil=N] [reveal=wipe_x]
  strokes <id> par=<f> [boil=N]
    stroke <id> nib=crayon:<NIB> w=<px> path=<path src> [outside=on]
  end
  prop  <id> art=<src> [art.<state>=<src> ...] plane=<plane> at=x,y
             [anchor=bottom|center|top|topleft] [light=fixed] [hidden=yes] [text=x,y,w,h]
  path  <id> y=<y> from=<x> to=<x>
  mark  <id> <x>[,<y>]
  frame <id> at=x,y                 # a named camera framing (shots may use cam at=<its at>)
  variant <id> [light=<light>] [bands.<plane>=<bands>] [image.<plane>=<src>] [light.<plane>=<light>]
               [show=<id,...>] [hide=<id,...>] [state=<prop>:<state>,...] [desc="..."]
  # 3D kinds:  map=MAPID_*  /  home= room= preset=  ;  cam3d <id> from=x,y to=x,y [look=x,y] [z=eye]
  #            [daylight=0..15] [yaw= zoom=] ;  obj <id> furn=FURN_* at=x,y rot=r ;
  #            variant <id> [tint=TINT_* tint_level=N] [daylight=N]
end
bands <id> rows=<INDEX>:<rows> ... [seam=stripe|none]       (top level)
nib   <NIB> ink=<INDEX> edge=<INDEX> gap=<on>,<off>         (top level, crayon.shw)
```

* `<src>`: `gen:<generator>(k=v,...)` (show/art/gen), or later `pxt:` / `hand:` (spec 9.4).
* `<INDEX>`: a palette index as the repo's enums: `PAL_*`, `PAINT_<NAME>.<0-2>`,
  `RAMP_<NAME>.<0-7>`, `SKIN_<NAME>.<0-2>` (= `PAL_PAINT(PAINT_X, s)` etc.).
* **Coordinates are show pixels** (320x180; show y + 30 = screen y). A plane with no `at=`
  is centred on the frame at camera 0 and its top is the open matte's top (screen y 0):
  plate pixel (px,py) shows at show (px - (w*px_scale - 320)/2, py*px_scale - 30). Plates are
  240 tall at px=1 and 120 tall at px=2. Parallax: `screen = at - cam * par`.
  A prop's `at=` is in the same frame, on its plane (it moves with that plane's parallax);
  `anchor` says which point of the prop image `at=` names (default bottom-centre).
* `order`: back planes draw before the actors, `front` after the actors, `top` last
  (grain). Within an order, file order; a plane's props draw right after it.
* `hidden=yes` starts a plane or prop off; variants and shot ops show it.
* `text=x,y,w,h` (props): the box, in prop pixels, where the player draws the prop's
  string (sign words, name cards, the KEEP OUT plate, the population line).
* `variant`: `light=` is the LUT for every plane that is not `light=fixed`; `light.<plane>=`
  overrides one plane; `bands.<plane>=` swaps a BANDS list; `image.<plane>=` swaps a plate
  (the credits stills); `state=` picks a prop's `art.<state>`.

## 4. What the player must do with these (contract for ENGINE)

* **CLUTs.** A plate in a variant uses `CLUT[i] = doomo_palette[lut[i]]` (256 entries, index 0
  transparent). When a camfx palette lerp is live (`PAL_CRAYON`, `PAL_HELLIFY`, `PAL_SEPIA`,
  `PAL_LILAC`), push the set CLUTs through it too: `CLUT[i] = camfx_pal_entry(lut[i], id, l, ...)`,
  or the plates will not rot with the master CLUT (MT-5's hellify, the crayon register).
* **TILEMAP.** `bake()` returns the whole plate plus `meta['tiles']` (unique 16x16 index tiles)
  and `meta['map']` (rows x cols tile indices); pack the tiles 256 to an 8bpp page.
* **Strokes.** `path=gen:...` returns a polyline, or a list of polylines (the firework
  scribbles), in show pixels. A nib's `gap` is the on/off pattern along the arc length.
* **Prop states** are separate images; a shot changes one with a show op (proposal:
  `+N prop† <id> state=<state>|show|hide`), the lanterns' `out` one after another in 1-14.
* **Budgets.** `show sets check` estimates each set's texels (opaque bounding boxes, tilemap
  unique tiles) against the 5-page pool: every set fits; the busiest are listed as warnings.

## 5. Look rules

* Background shapes take a family-dark outline, never the sprites' black; far planes are
  lighter and greyer; the brightest, most saturated colours are left for the cast.
* Characters stand at figure scale: feet on the bandstand deck at show y 124, on the lawn
  and the fairground at y ~150-175; counters at y 118.
* The crayon register: waxy diagonal-lane fills (the ink's light shade inside, the paper at
  the skipping edges), 2-px wobbly crayon lines with a gap pattern, a 4 % paper-fleck GRAIN
  tile. No faces on the young cherub or the young imp.
* Children are in the cold open, the bridge, the craft tent and Pip's scenes: these sets
  carry no gore and nothing suggestive; the red in the cold open is candy-red paint
  (`PAINT_CHERRY`, D16), never the BLOOD ramp (a test checks the drip).

## 6. Ambient layers: every set lives (ART motion pass, the creator's quality bar)

Every set carries `amb` statements and at least two parallax depth planes (new
`clouds` / `fog` / `haze` planes where a plate stood alone). Nothing is hand-keyed:
each layer is a pure integer function of (parameters, shot frame t, seed), so shots
stay pure (spec 8.2) and a seek draws the same pixels. Reference math:
`showtk/art/motion/ambient.py`; its C twin `show/art/engine/amb_math.h` (a host
test checks they agree value for value). Previews: `show_out/art/motion/`.

### 6.1 Grammar (inside a `set`)

```
amb <id> kind=drift  plane=<p> vx=<px/f> [vy=]                  the plane slides by itself, wrapping
amb <id> kind=wave   plane=<p> amp=<px> period=<f> len=<rows> [strip=2] [rows=y0..y1]
                                                                rows shear in the wind (strings, bunting, flags, grass, water)
amb <id> kind=pcycle plane=<p> idx=<INDEX>,... every=<f>        rotate those CLUT entries on that plane (fountain water)
amb <id> kind=lamps  plane=<p> | props=<ids> [hold=a..b] [dim=%] [bloom=%] [seed=]
                                                                every lantern core / bulb flickers on its own clock
amb <id> kind=sway   props=<ids> amp=<px> period=<f> [lag=<f>] [dy=<px>]   hanging props swing, each lag f later
amb <id> kind=flip   art=<flipbook gen> plane=<p> at=x,y [anchor=] [speed=] [phase=] [light=fixed]
amb <id> kind=motes  type=firefly|sparkle|ember|dust|petal|snow|bubble|leaf n=<N> area=x,y,w,h
                     [plane=<p> | order=front | par=<f>] [seed=] [speed=]
amb <id> kind=crowd  form=walk|idle kset=<kset> n=<N> y=<feet y> x=<x0..x1> [speed=] [depth=] [seed=] [plane=]
common:  hidden=yes (a variant's show= turns it on); variants' hide=/show= take amb ids
```

Lamps are found at bake time, not listed: connected blobs of `RAMP_CANDY.7` + `PAL_WHITE`
(2..48 px) in the plate or prop are lantern cores. Dim = the core one step down
(WHITE->CANDY.7, CANDY.7->CANDY.5); bloom = a 1-px CANDY.6 ring. A prop lamp only
flickers in its `lit` state (dark variants switch them off by themselves).

### 6.2 Contract for ENGINE (the player module that draws them)

* Record (proposed, lane 2's props/crowds range): `SHWR_AMB = 36`, n items of
  `{u8 kind, u8 plane_id, u16 bytes, int16 params[...]}`; flip drawings ride `SHWR_IMAGE`
  like plates, lamp blobs as `{x, y, w, h, image}` dim/bloom overlay images.
* `drift`: add `amb_drift(vx_q8, t)` to the plane's x before the camera transform; the
  plane is drawn with SHWPF_TILE_X. `wave`: draw the plane in `strip`-row SPRT strips,
  each offset by `amb_wave_dx(amp, period, len, row, t)`. `pcycle`: rewrite the plane's
  CLUT entries every `every` frames (no texels). `sway`: add `amb_sway_dx(...)` to the
  prop's x. `lamps`: `amb_lamp_state()` picks no overlay / dim / bloom. `flip`:
  `amb_flip_frame(holds, n, t, speed_q8, phase)`. `motes`: `amb_mote()` returns
  (x, y, size, role); size 0 = one TILE_1, 1 = plus, 2 = 2x2, 3 = r3 cross; colours
  per `ambient.MOTE_COLOURS`. `crowd`: `amb_walker()` per member == a CRWD lane whose
  members wrap 20 px off both edges (odd members walk left); `form=idle` = SHC_SCATTER
  with SHCF_BOB and glances (ambient.idler).
* Order: an amb draws right after its plane's props; `order=front` after the actors;
  crowds (no plane) with the actors, behind them.

### 6.3 Budget (every set with its ambient layers; `python3 -m showtk.art.motion.budget`)

Flipbook drawings are all resident; motes/waves/sways/pcycles cost no texels;
crowd chibis live in the portrait pool (384 B blocks), not the set pool.
Worst set: **fun_fair 4.88 of 5 pages** (the ferris wheel is 4 drawings x 4 f, a 1/32
turn per step; a 5th drawing would overflow). Every set is PS1-LEGAL.

| set | kind | plates (pages) | ambient texels | total pages | amb prims/f | chibi blocks | amb layers |
|---|---|---|---|---|---|---|---|
| act_cards | 2d | 1.64 | 0 | **1.64** | 40 | 0 | 2 |
| bake_tent | 2d | 3.11 | 2442 | **3.15** | 109 | 28 | 7 |
| bake_tent_fp | view3d | 0.00 | 0 | **0.00** | 0 | 0 | 1 |
| bakery_window | 2d | 1.51 | 1225 | **1.53** | 65 | 0 | 4 |
| bandstand | 2d | 4.67 | 480 | **4.68** | 233 | 60 | 11 |
| bandstand_banner | 2d | 4.41 | 0 | **4.41** | 48 | 0 | 4 |
| bandstand_cu | 2d | 0.40 | 0 | **0.40** | 24 | 0 | 3 |
| banner_storybook | 2d | 2.20 | 0 | **2.20** | 280 | 0 | 4 |
| bridge_storybook | 2d | 0.80 | 0 | **0.80** | 272 | 0 | 3 |
| calendar_page | 2d | 0.43 | 0 | **0.43** | 40 | 0 | 2 |
| candy_hills_road | 2d | 3.05 | 0 | **3.05** | 76 | 0 | 3 |
| cast_popin | 2d | 1.93 | 0 | **1.93** | 168 | 0 | 3 |
| cottage | room | 0.00 | 0 | **0.00** | 0 | 0 | 1 |
| credits_card | 2d | 1.68 | 0 | **1.68** | 32 | 0 | 2 |
| credits_storybook | 2d | 0.44 | 0 | **0.44** | 32 | 0 | 2 |
| dippinn_card | 2d | 1.33 | 0 | **1.33** | 0 | 0 | 1 |
| egg_basket | 2d | 0.39 | 0 | **0.39** | 40 | 0 | 3 |
| eyecatch | 2d | 2.04 | 0 | **2.04** | 32 | 0 | 4 |
| face_painting | 2d | 2.66 | 816 | **2.67** | 169 | 32 | 6 |
| fountain_faces | 2d | 0.37 | 7042 | **0.48** | 58 | 0 | 4 |
| fountain_plaza | 2d | 3.60 | 2653 | **3.64** | 177 | 44 | 9 |
| fun_fair | 2d | 4.63 | 16536 | **4.88** | 260 | 40 | 8 |
| giggle_wide | 2d | 1.56 | 0 | **1.56** | 108 | 112 | 5 |
| hahaland_crane | overworld | 0.00 | 0 | **0.00** | 0 | 0 | 1 |
| hahaland_wall_pan | overworld | 0.00 | 0 | **0.00** | 0 | 0 | 1 |
| hattie_tent | 2d | 1.64 | 0 | **1.64** | 105 | 36 | 5 |
| hook_a_duck | 2d | 2.75 | 864 | **2.77** | 183 | 20 | 6 |
| joke_graveyard | 2d | 2.43 | 0 | **2.43** | 192 | 0 | 4 |
| kids_tent_memory | 2d | 1.45 | 0 | **1.45** | 64 | 0 | 2 |
| launch_hill | 2d | 1.47 | 0 | **1.47** | 224 | 0 | 5 |
| logo_card | 2d | 1.65 | 0 | **1.65** | 72 | 0 | 3 |
| lulla_tent | 2d | 3.44 | 0 | **3.44** | 277 | 20 | 6 |
| park_storybook | 2d | 3.97 | 0 | **3.97** | 424 | 0 | 4 |
| pipe_yard | 2d | 1.50 | 0 | **1.50** | 48 | 0 | 2 |
| river_storybook | 2d | 2.41 | 0 | **2.41** | 288 | 0 | 3 |
| scary_house_fp | view3d | 0.00 | 0 | **0.00** | 0 | 0 | 1 |
| sealed_gate | 2d | 2.22 | 0 | **2.22** | 64 | 0 | 4 |
| sky_storybook | 2d | 3.11 | 0 | **3.11** | 176 | 0 | 4 |
| soup_table_storybook | 2d | 0.46 | 0 | **0.46** | 32 | 0 | 2 |
| title_card | 2d | 3.53 | 0 | **3.53** | 40 | 0 | 3 |
| title_corridor | view3d | 0.00 | 0 | **0.00** | 0 | 0 | 1 |
| town_sign | 2d | 2.35 | 0 | **2.35** | 160 | 0 | 3 |
| tvma_card | 2d | 1.30 | 0 | **1.30** | 0 | 0 | 1 |
| wall_split | 2d | 1.77 | 0 | **1.77** | 96 | 0 | 4 |
| whack_an_imp | 2d | 2.60 | 0 | **2.60** | 83 | 12 | 6 |

### 6.4 Cast motion (series/acting.shw)

`cycles chars=... kinds=...` declares `<char>_<kind>` flipbooks = `gen:cycle(char,kind)`
(show/art/gen/motion.py): the character's own engine render cut into parts (hat/ears
band, paws, feet) and moved. Kinds: idle (engine breath + blink), walk (8x3 f
waddle: contact squash, passing bob, feet lift, paw swing, lean, hat/ears lag 2 f), run
(6x2 f), tiptoe, back_step, pop, pop_small, take_big (anticipation, stretch 1.25, hat pop
6 px landing late, overshoot, settle), react_small, laugh_full, hit (hat jumps, ears
flop), smear_l/smear_r (whip smears), exit_run. `flip` lines name the shot lists' poses
and prop flipbooks. Macros: enter_walk, enter_run, exit_walk, exit_run, walk_to,
take_big, react_small, laugh_full, hit_react, whip_smear_r/l, beat_gesture, pop_in.
`pose A <kind>` resolves to the actor's `<char>_<kind>`; a one-shot hands back to its
`next` (idle; exit_run -> run) when it ends. Sheets: `python3 -m showtk.art.motion.actors sheet`.
