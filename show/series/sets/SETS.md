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
