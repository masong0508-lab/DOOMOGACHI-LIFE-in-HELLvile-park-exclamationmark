# Resident renderer: portraits, chibis, figures and fighters

Owner: **portrait**. Every resident image in the game is drawn at runtime
from the resident's `DoomoLook`. There is no stored art, no allocation and
no floating point, and the cost per image is bounded. The API is in
`core/include/doomo/portrait.h`.

| Image | Size | Entry points | Used for |
|---|---|---|---|
| bust portrait | 48x56 | `portrait_render`, `portrait_image` | dialogs, lists, the relationship UI |
| creator figure | 48x80 | `figure_render`, `figure_image` | the creator preview (whole outfit, tail, wings) |
| overworld chibi | 16x24 | `bodyspr_render`, `bodyspr_image` | 4 directions x 4-frame walk cycle |
| fighter | 32x48 | `fighter_render`, `fighter_image` | third-person shooter body: 5 views (mirror for the right side), anim 0..3 run, 4 fire, 5 roll, 6 hurt, holding the `WFRAME_*` weapon |

## Art direction

The residents are plush critter-folk with a premium, lively and vaguely
furry look, close to Tomodachi Life or Tamagotchi at late-GBA quality.
They have huge heads and tiny round bodies, fur tufts on the cheeks and
crown, big expressive ears and tails, muzzles and button noses, and paw
hands. Eyes are big and glossy with two highlights. Shading uses 3-4
hue-shifted tones with coloured selective outlines. Demonic parts (horns,
wings, fangs, tails) are drawn *adorable*. Only POSSESSED and SOUL_LOST
residents look properly creepy: hollow red eyes, veins, drippy black tears,
a glitch and smoke aura, and drained colour. GHOST residents are pale,
translucent cyan with a wispy tail, and DEAD ones get X eyes.

Every option of every catalog list visibly changes the image, and the
tests enforce it. The catalog lists are 26 species, 10 bodies, 12 heads,
24 eyes, 12 brows, 12 noses, 20 mouths, 32 hair styles, 8 facial hair,
16 ears, 16 horns, 16 tails, 10 wings, 6 halos, 23 markings, 24 tops,
12 bottoms, 12 shoes, 24 hats, 20 accessories, 8 auras and 8 patterns.
Every slider step (height, build, head size, eye size, spacing, height and
tilt, brow height, nose and mouth size, fangs, horn size, eye count),
every cheek flag, the hair highlight, all colours and all 11 expressions
are also visible.

## Source layout

| File | What |
|---|---|
| `core/src/portrait.c` | entry points, `sanitize` (garbage looks are safe), materials (`pctx_init`), the layer order (`draw_step`), per-view layout (bust / figure / chibi / fighter poses), the image cache and sliced re-renders |
| `core/src/portrait_gfx.c` | the raster canvas: shaded ellipses, capsules, triangles, quads, Bezier strokes, rounded rects, pattern fills, masks and clips; part ids; inner contours and the selective outline pass; hot loops in IWRAM on the GBA |
| `core/src/portrait_lut.c` | `pl_bandlut`: precomputed light bands for the sphere-like shading modes (formula in the file header; a test recomputes it) |
| `core/src/portrait_head.c` | head shapes, species details (muzzles, cheek tufts and ruffs, skeleton sockets, frog grin and so on), eyes (24 styles plus expressions, blink, creepy, X), brows, noses, mouths, blush and cheek flags, whiskers, markings, facial hair |
| `core/src/portrait_hair.c` | 32 hair styles (back and front layers, dyed strands), 16 ears (with twitch), 16 horns, 24 hats, face accessories, halos |
| `core/src/portrait_body.c` | body layout (`pb_layout`), legs, shoes, bottoms, torso, tops, collars, arms and paws, tails, wings, neck and back accessories, chest fluff |
| `core/src/portrait_fx.c` | auras (behind and in front), expression effects (tears, hearts, sweat, zzz, vein, sick tint, soul wisp, sparkles, talk lines), possessed / soul-lost effects, ghost and creepy recolouring |
| `core/src/portrait_gun.c` | the 39 `WFRAME_*` weapons in hand for the fighter views, plus the muzzle flash |
| `core/src/portrait_turkey.c` | `SPECIES_TURKEY` (Mayor Giggs' kind, show canon): the long neck, bare head, beak, snood and wattle, bulging eyes, the Hatter's feather tufts (`HAIR_WILD`), the tall crooked top hat (`HAT_TOP_HAT`), the tail coat's lapels and tails (`TOP_SUIT`), the floppy bow tie (`ACC_BOWTIE`) and the turkey fan (`TAIL_FEATHER_FAN`); portrait.c's `turkey_step()` draws a turkey through these and the engine's own parts for everything else |
| `core/src/portrait_int.h` | internal model shared by the above (canvas, materials, head space, context) |
| `core/tools/portrait_sheet.c` | host tool that writes PNG contact sheets |
| `core/tests/test_portrait.c` | unit tests |

## Pipeline

1. **Sanitize.** The look is copied and out-of-range fields are clamped.
   Any byte pattern is safe to render.
2. **Context** (`pctx_init`). Materials are built from the palette ramps:
   each one has five tones (deep, dark, base, light, highlight) plus an
   outline colour, chained darker until it reads as a line. Skin, fur,
   hair, eyes, horns, markings and every clothing item get one. Hair the
   same colour as the fur is pushed a shade deeper. Creepy, ghost and
   dead states adjust materials here. The animation phases (blink,
   breath, twitch, wag, effect frame, talk) come from `anim`.
3. **Layout.** `pb_layout` places the body for the view (portrait, figure,
   chibi or fighter), including the walk or run pose, the height and build
   sliders, and the species body type (for example the cacoblob floats
   and has no legs). The head is a `PHead`: centre, radii and yaw 0..4
   (front, 3/4, profile, back-3/4, back). Parts are placed in a
   normalised head space (Q8, 256 = radius) and projected by `ph_proj`,
   so one description of an ear or eye works in every view.
4. **Layers** (painter's order, in `DL_STEPS` = 6 groups):
   aura back, weapon (back), wings/tail/cape behind, back hair, back ears;
   body; head and face; neck pieces, front hair, brows/third eye, front
   ears; hat, species extras, horns, face accessories, front arms/paws,
   weapon (front); finish.
5. **Finish.** The contour and outline pass, then halo, status effects,
   expression effects, front aura, muzzle flash, and the ghost or dead
   recolour.

### The canvas

Coordinates are Q4 sub-pixels (`PQ` = 16), so shapes are anti-jaggy and
small slider steps still move pixels. Every primitive belongs to a *part*
(`pc_part`). Each pixel stores the id of the part that last covered it,
and decals (`pc_decal`) paint without changing ids. The finish pass scans
id transitions word by word and marks the boundary pixels in a bitmap.
Then:

- **inner contours:** where a front part overlaps a back part, the back
  part's pixel is darkened. This draws the line between, say, a cheek
  tuft and the face in the face's own dark tone.
- **selective outline:** the silhouette is drawn in each part's own line
  colour, lighter on the lit top-left and darker underneath.

Shading modes are BALL (sphere), GLOSS (sphere with a hard highlight),
SOFT (fur), FLAT, VGRAD and HGRAD. The sphere modes look up the light band
in `pl_bandlut` with a per-primitive or shared shading frame (`pc_frame`,
so a head built from several shapes shades as one body). Fuzzy parts use
the high nibble on odd checker pixels for a fur dither. Patterns (stripes,
polka, plaid, stars, hearts, skulls and so on) come from `p_pattern_tiles`.

### Animation

`portrait_render`, `figure_render` and the other direct renderers take an
`anim` tick. The cached accessors animate by themselves from
`doomo_frame_count()`, staggered per resident:

- blink: 4 of every 110 ticks
- breathing: 32-tick halves
- ear twitch: 8 of every 270 ticks
- tail wag and wing flap (figure)
- aura and expression effect frames: 4 frames, each held 2^`PC_FXSHIFT` ticks (16 on GBA, 8 elsewhere)
- EXPR_TALK mouth flaps every 4 ticks

The fighter and chibi animate through their `anim` and `frame` arguments.

## Show canon species (C1)

Two species exist for the show's canon (DECISIONS: adopt the show canon in
the game). The random generators never roll them; the roster and the
creator use them (3 presets each).

* **Turkey** (`SPECIES_TURKEY`, Mayor Giggs, roster 0). The show's locked
  Mad-Hatter turkey (show/art/parts/show_parts.c, ported into the engine):
  a lavender top hat with a magenta band and no card, wild white hair,
  peach head, orange beak, red snood and wattle, teal tail coat, magenta
  bow tie and a tail fan. The look's own fields drive it: `skin` is the
  bare head, neck, legs and hands, `marking_color` the snood and wattle
  (`nose_size` their size), `eye_size` how far the eyes bulge, `hair_color`
  the tufts, `hat_color` / `pattern_color` the hat and its band (`horn_size`
  the crown). Other hair styles, hats, tops, tails and accessories are the
  engine's own, so any turkey can wear anything; a bare torso is bronze
  plumage. The wattle, snood, tufts and fan wobble on the fx phase
  (`phase_of`), twice as much in EXPR_SHOCK.
* **Animatronic** (`SPECIES_ANIMATRONIC`, Bonejangles, roster 5): a
  skull-faced animatronic built as a tribute to a death-obsessed artist.
  Skull sockets for the eye-lights, a hinged ventriloquist's jaw with steel
  bolts, a riveted forehead plate, a brass wind-up crank on the side of the
  skull, steel limbs with servo joints, and a ribcage that is a toy
  xylophone (bright bars on the dark steel frame) whenever the torso is
  bare. Possession stays a status (`RES_POSSESSED`), never a look.

`test_portrait.c` checks both: the locked look's fields and colours, every
view, expression and status in bounds, deterministic and inside the render
budget, distinct walk directions and fighter views, and whole frames only
under sliced re-renders.

## The image cache

`portrait_image`, `bodyspr_image`, `figure_image` and `fighter_image`
return `const DoomoImage *` from one 18 KB pool of 48 x 384-byte blocks.
A portrait takes 7 blocks, a figure 10, a fighter 4 and a chibi 1. The
pool holds at least 6 portraits, 12 fighters or 48 chibis at once. Runs of
blocks are reclaimed LRU (the run whose most recently used victim is
oldest).

- The key is the kind, the resident, an FNV hash of the look, the status,
  the expression and the animation phase. A **uid changes only when the
  pixels change**. A new uid comes from `doomo_image_new_uid()`.
- A pointer stays valid until a later call evicts it, so draw an image
  right after asking for it.
- `portrait_invalidate(id)` drops a resident's images after an edit. A
  changed look also misses on its own, because the look hash is in the key.
- An empty slot (`RES_EMPTY`, a bad id) gives a still lavender "?"
  silhouette.

**Sliced re-renders.** New content is drawn at once: a look, expression or
status that is not cached in any phase. A new *animation phase* of
something already cached is drawn by a background job instead. The job
draws `PC_STEPS` of the 6 layer groups per tick (GBA 1, MIPS/PS1 3, native
all 6), straight into a hidden cache entry, and the phase shown last stays
on screen meanwhile. The job also looks `PC_AHEAD` = 8 ticks ahead and
pre-draws the next phase, such as the coming blink or aura frame, so the
change is usually on time. In tests, 95-99% of ticks show exactly the phase
a synchronous renderer would. Look-ahead claims never evict an image that
was on screen in the last two ticks.

The job shares the canvas with every other render. If another render used
the canvas between two slices, which it detects through `pcv.gen`, the job
restarts. On its third restart it completes in one go, so it always
finishes. A job nobody has asked about for `PC_STALE` ticks is dropped.

`portrait_cache_clock(tick, steps)` runs the cache on a fake clock with a
chosen step budget, and `portrait_cache_reset()` goes back to the real
clock. Tests use both.

## Performance

Measured on GBA code generation (arm-none-eabi-gcc -O2 -mthumb, game
WAITCNT, mGBA) for 16 random citizens. Cycles are at 16.78 MHz; one video
frame is 280,896 cycles and one 30 Hz logic tick is 561,792.

| Render | avg cycles | worst |
|---|---|---|
| bust portrait | 763k | 1.20M |
| creator figure | 915k | 1.38M |
| chibi | 250k | 338k |
| fighter | 454k | 655k |
| cached accessor, animating (sliced job, look-ahead) | ~50k per call | 0.23M per call |
| cached accessor, hit | a few k | |

Native `--stats` reports an average of 4.1k and a worst of 8.7k pixel
visits per render over 1,304 renders. The test bound is 12k.

Where the time goes (GBA PC-sampling profile): shaded spans 17%, capsules
13%, outline and contour marking about 22%, triangles and ellipses about
10%. Measures taken:

- The hot leaf loops are `sh_span`, `capsule_plain`, `scan_transitions`
  and `finish_marked`. They are ARM code in IWRAM (`P_FAST`,
  `.iwram.portrait`, about 3.9 KB) under `DOOMO_PLATFORM_GBA`.
  `-DPORTRAIT_NO_IWRAM` turns this off.
- Shader state is copied to locals, shading is incremental along each
  span, and flat spans take fast paths.
- Circle and reciprocal tables (`circ`, `rcp`) remove per-row square roots
  and divisions.
- `PCT()` replaces `* n / 100`, which is a library call in Thumb.
- Triangles use edge steppers.
- The outline pass scans id transitions word-wise into a mark bitmap.

GBA and native renders are bit-identical: the bench hashes match.

Memory: the cache pool is 18,432 bytes, the canvas (part ids, line
colours, mark bitmap) about 4.5 KB, the cache table 1.7 KB and the job
324 bytes. That is about 25 KB of `.bss`, plus 3.9 KB of IWRAM code on the
GBA. ROM use is about 130 KB, including the 12.7 KB band table.

## Contact sheets (`portrait_sheet`)

Build it with the core's native objects:

```sh
B=/path/to/build
make -C core runner BUILD=$B
gcc -std=c99 -O2 -D_POSIX_C_SOURCE=200809L -Icore/include -Icore/native \
    core/tools/portrait_sheet.c $B/obj/src/*.o $B/obj/gen/*.o \
    $(ls $B/obj/native/*.o | grep -v runner_main) -o $B/portrait_sheet
$B/portrait_sheet --out DIR [--zoom N] [--only NAME.png] [--stats]
$B/portrait_sheet --out DIR --bust LIST [--fig]     # e.g. --bust species
```

| Sheet | Shows |
|---|---|
| `closeup.png` | 4 citizens big: bust, figure, fighter, chibi |
| `opt_<list>.png` | every option of one catalog list (bust above figure) |
| `sliders.png` | every slider at 0, 3, 7, 11, 15, plus eye count and fangs |
| `random_good.png`, `random_hell.png` (+ `_fig`) | 64 `resident_generate` and 64 hellborn citizens |
| `expressions.png`, `statuses.png` | all 11 expressions; alive, possessed, soul lost, dead, ghost |
| `anim.png` | animation phases: anim 0, 8, 32, 40, 92 (blink), 190 (ear twitch), 64, 100 |
| `walk.png` | chibi walk cycles, 4 directions x 4 frames |
| `fighter.png`, `weapons.png` | fighter views x anims, every weapon |
| `bust_<list>.png` / `fig_<list>.png` | large close-ups of one list (`--bust`) |

## Tests (`core/tests/test_portrait.c`, run by `make -C core unit`)

- Every option of every catalog list renders distinct from every other
  option of that list. The check hashes bust, figure and chibi on the
  default citizen and re-checks on two other citizens (a bald human and a
  hellborn imp), so no option hides behind another.
- Every cheek flag, every slider step, the colours and the hair highlight
  are visible.
- Expressions and statuses differ, and the portrait animates.
- Nothing is written out of bounds (canary bytes around the buffer) for
  160 looks, including random garbage, in all renderers and views.
- Rendering is deterministic and never empty.
- The walk-cycle directions and frames differ, all 35 fighter view/anim
  combinations differ, and every weapon differs.
- Cache: uids stay stable until the pixels change, and 6 portraits stay
  resident.
- Sliced re-renders only ever show whole, correct frames: GBA-style
  one-step slices with interfering renders are checked against
  synchronous rendering. No pixel change goes without a uid change, and
  frames are on time in at least 60% of ticks (95% or more measured).
- `pl_bandlut` equals its documented formula.
- Render cost is bounded (12k pixel visits).

## Adding an option

1. Add the enum value in the catalog (owner: catalog). Its `*_COUNT`
   grows.
2. Draw it in the matching function: `hair_back_shapes()` /
   `hair_front_shapes()` in `portrait_hair.c`, `top_details()` in
   `portrait_body.c`, and so on.
   Use the context's materials and `pc_part` for a new outlined piece, or
   `pc_decal` for a print on an existing one.
3. Run `make -C core unit`. `portrait_every_catalog_option_is_visible`
   fails if the new option renders the same as another. Then look at
   `opt_<list>.png`.

## Life stages: body and face forms by age (F4)

`look_for_stage(in, stage, out)` (`core/src/portrait_age.c`, declared in
`portrait.h`) gives the look to DRAW for a life stage. The saved look never
changes; a baby who grows up gets the same face back at adult proportions.
It only moves fields the renderer already has, so every species, body type
and outfit keeps working, and the portrait cache (keyed by the look's hash)
holds one image per stage with no other change.

| Stage | Height | Build | Head | Eyes | Nose / mouth | Else |
|---|---|---|---|---|---|---|
| BABY | 0 | (build + 13) / 2, round | 15 | at least 13 | nose at most 1, mouth at most 3 | blush, no scar, no fangs, no beard |
| TODDLER | at most 3 | (build + 11) / 2 | at least 14 | at least 12 | nose at most 2, mouth at most 5 | blush, no scar, no fangs, no beard |
| CHILD | 3..6 | (build + 8) / 2 | at least 11 | at least 10 | nose at most 4, mouth at most 8 | blush, fangs at most 1, no beard |
| TEEN | 1..12 | as saved | at least 8 | at least 8 | as saved | fangs at most 2, no beard |
| ADULT | as saved | | | | | |
| ELDER | one shorter | as saved | as saved | as saved | nose + 2 | silver hair, white highlight |

`portrait_image`, `bodyspr_image` and `fighter_image` apply it by the
resident's own stage (`RES_STAGE`); for a look you hold yourself (a family
pool member from `family_resident()`, the creator preview) call
`look_for_stage()` or `figure_image_stage()`. `family_age_daily()` calls
`portrait_invalidate()` for a member that changed stage. Hair, outfit, horns,
wings and species are never touched. Not yet done: baby-specific outfits
(an onesie), a dedicated baby body shape (a swaddle), wrinkles and a stoop
for ELDER (BODY_HUNCHED is the player's choice), and a look at the Q4
proportions of each stage on real hardware.

## Customisation v2, P1.M1: part scripts, groups, hooks (SPEC 4.4)

**Layer groups.** `DL_STEPS` is 7: 0 back (aura, weapon, behind-body, back hair, back ears), 1 body,
2a head base, 2b face, 3 neck / front hair / over-hair face / front ears, 4 hat / species bits / horns
/ face accessories / front outfit / weapon, 5 finish (outlines, halo, status). The old group 2 was
split in two so a slice can be smaller; the draw order did not change. `PC_AHEAD` is 9.

**Classic golden.** `core/tests/golden/portrait_classic.txt` holds the FNV-1a of bust, figure, body
sprite and fighter (views 0 and 2) for every 5th premade, frozen before P1 touched the renderer.
`portrait_classic_golden` must stay green: Classic looks render byte-identically. Regenerate only on
purpose: `build/tests --update-golden portrait_classic_golden` (from the repo root).

**Part scripts.** A part is a few bytes of ops, not C. Source: `core/data/parts/{face,head}/*.txt`;
`python3 tools/parts_build.py` writes `core/gen/parts_{face,head}.c` (also run by `make gen`, checked by
`make check-gen`). The format is in the header comment of `tools/parts_build.py` and the byte layout in
`core/include/doomo/parts.h`. In short:

- `list FIELD VALUE` opens the script of one style choice; `block group=G anchor=head|torso
  lod=tiny,mid,big view=portrait,figure,fighter,chibi` is the unit the VM gates by group, anchor, LOD and
  view, so a part can cost nothing where it is not wanted (chibi gets two dots, the bust gets the detail).
- Ops: `mat FIELD [light|dark]`, `shade`, `solid`/`decal`, `ellipse`, `capsule`, `tri`, `line`, `dot`,
  `stamp NAME X Y [Q4]`, `mirror`, `scale`, `region`, and `ifview` / `ifyaw` / `ifanim` ... `endif`.
- Coordinates are normalised (256 = one radius, steps of 2), so sliders move a part with the head or
  torso. Stamps are 1bpp, up to 16x16. Materials come from `pm_of()`: choice 0 is today's derivation,
  `light` / `dark` step the ramp.
- The build rejects an unknown field / op / name, a coordinate outside -256..254, a block over 255 B, a
  stamp over 16x16 and a solid part over about 600 px at bust scale.
- Run a script with `ps_run()` / `ps_run_group()` (`portrait_int.h`). No painter calls it yet; the demo
  scripts (`m1_demo.txt`) sit on style values no catalog list reaches. M2 wires the live lists in.

**Regions and stamps.** `pc_region()` clips a decal to the head, either eye, the lower face or the
torso; `pc_stamp()` draws a 1bpp stamp at a Q4 centre and scale.

**Cache keys and animation.** `look_key(rid)` is the gene plus the deep-look run bytes (FNV-1a); the
free-play memo uses it with the status. `look_anim_mask()` is the one list of looks that need effect
frames or talking phases (the cache's `phase_of()` and the creator's draft bust both use it).

**Hooks for other packages (CF-38).** `pf_material_init()` (end of `pctx_init()`, P2), `motion_pose()`
(in `bodyspr_render()`, P3; no-ops until those land) and `ph_head_only(c, lod)` (head layers only, for
P3's head impostor).

**Provisional deep fields (shim).** P0 is not in this tree, so `look_fields.h` / `look_pool.c` stand in:
16 face fields, a 384 B in-RAM run pool (never saved) and `look_resolve_mask()`. The per-job resolve
(`job.D`), the animated / body / fighter resolves, the chibi key (only `DFF_CHIBI` run entries are
hashed) and `look_anim_mask_rid()` (a `DFF_ANIM` field asks for effect frames; the creator's draft
bust uses it with `LOOK_ID_DRAFT`) all go through it. P0 replaces the two shim files whole.

**Test-build hit check.** Host builds define `DOOMO_HITCHECK` (`CORE_CFLAGS` in `core/Makefile`): every
exact cache hit of `portrait_image` / `figure_image` is re-rendered from scratch and compared with the
cached image, so equal keys must give equal pictures. `portrait_hitcheck_count()` / `_fails()` expose it
(`cache_hits_equal_a_fresh_render_with_and_without_a_run`). Not defined on GBA / PS1 / DC.

**PS1 LZ blocks.** `parts_build.py` also packs each script set into LZ blocks (the save codec) of at
most `PART_BLK` = 2048 raw bytes, ending on entry boundaries, and emits them beside the raw array
(`PartLz`: `z`, `zofs`, `rofs`). `ps1/CMakeLists.txt` defines `DOOMO_PARTS_LZ`: the raw array is then not
compiled and `ps_run_group()` reads through `PART_OPS()` -> `part_lz_ops()` (`core/src/part_lru.c`), which
decodes a block into one of four 2 KB slots (8 KB, least recently used replaced). GBA and host read the
raw array. Host builds define `DOOMO_PARTS_LZ_TEST`, so the blocks are checked against the raw scripts
and the LRU is tested with a synthetic set; the whole unit suite also passes with `OPT="-O2
-DDOOMO_PARTS_LZ"` (the VM reading through the LZ path). Not yet built with the PS1 toolchain.

**M2 wave-2 face fields (painters done, lists not yet).** Painters read `c->d->v[DF_x]`; 0 is today's
picture. All of these are done against the shim ids and covered by one distinctness test each
(all-zero set == `portrait_render`; every choice differs from 0 and from every other choice of its field):

| Field | Choices | Where |
|---|---|---|
| `DF_PUPIL` | slit, pinpoint, wide, bar | `eye_glossy` |
| `DF_SCLERA` | white or shaded sclera round a smaller iris | `eye_glossy` |
| `DF_BLUSH2` | slashes, wide round, dots, low bar (1 px on chibis) | `blush()` |
| `DF_GLASSES2` | round, square, half-rim, cat-eye; the accessory slot's glasses/sunglasses/monocle win | `glasses2()` in `ph_face_acc` |
| `DF_EAR_FOLD` | tip, half lop, full lop, left down, right down, both down, crumpled (7) | `ear_fold_amt()` in `one_ear`; full lop 110 vs both down 190, equal amounts rendered identically |
| `DF_HOOD_HEAD` | pushed back, up with face opening, up and low with drawstrings; hoodie/cloak and no hat only; covers the whole head from behind | `hood_head()` in `ph_hat` |
| `DF_LASH` | flicks, wing, lower lashes | `face_deep()` |
| `DF_LID` | heavy droop line, half-lidded, double crease | `face_deep()` |
| `DF_LINER` | thin, winged, thick with waterline | `face_deep()` |
| `DF_BAGS` | one line, two lines, smudge | `face_deep()` |
| `DF_FRECKLE` | sparse cheeks, nose band, heavy | `face_deep()` |
| `DF_MOLE` | by the mouth, under the eye, cheek | `face_deep()` |
| `DF_SCAR2` | across the cheek, through the left eye, across the nose | `face_deep()` |

"Face C-tier" is read as the seven shim fields between `DF_HOOD_HEAD` and `DF_MAKEUP` (the spec has no
written list); `DF_MAKEUP`, `DF_FACEPAINT` and `DF_AURA2` stay in M3. `face_deep()` draws on the bust only
(no chibi flag). `DF_EAR_FOLD`, `DF_GLASSES2`, `DF_HOOD_HEAD` and `DF_BLUSH2` are `DFF_CHIBI` in the shim.
`portrait_render_deep()` (`look_fields.h`) draws a bust with a given deep set for tests and contact sheets.
Unit suite 828/828. **Nothing is live:** only tests call `look_pool_put()`, no creator row or save field
sets a deep value, so no player can see any of these yet. The pictures have only been checked as
"different pixels", not by eye.

**List appends (B76 done).** B76 halos +6 (Double Ring, Heart Halo, Square Halo, Neon Glitch, Flower Ring,
Saturn Ring) are in `ph_halo()`, appended after the live count: `HALO_COUNT` stays 6 and `HALO_ALL_COUNT` is 12.
`portrait.c` sanitises halos to `HALO_ALL_COUNT`, `resident.c`, the creator, saves and the roster still use
`HALO_COUNT`, so the new values cannot be chosen or saved until the list is flipped live (a premade must wear
each value, `roster_uses_every_catalog_option`, and the art review must pass). Test:
`wave2_halos_are_appended_after_the_live_count_and_every_one_is_distinct`. Unit suite 829/829.

**Still to do in M2.** The other list appends F01, F07, F25, F29, F35, F47, H01, H13, E01, B66, B79 and
O25; the art review and contact sheets for all thirteen fields above (chibi, back view and the
hood/hat/glasses interactions in particular).

**Not yet.** The real deep-field set (P0, replaces the shim), M3 (`DF_MAKEUP`, `DF_FACEPAINT`,
`DF_AURA2`, the remaining face/head/makeup fields, A28 expressions, aura and halo detail),
`live_p1.txt`, the mGBA bench of the SPEC's accept numbers, and a PS1-toolchain build of the LZ blocks.

## Many parts (CR2): slot hooks, script sets, family macro ops, the PS1 part arena

The creator plan (1,611 new parts) draws most of them as **part scripts**, not C. CR2 makes a script a
first-class value of its slot.

**Script sets.** Every `core/data/parts/<dir>/` (all but `catalog/`) is a set: `tools/parts_build.py`
writes `core/gen/parts_<dir>.c` and the registry `core/gen/parts_index.c` (every set; per list the set
that holds it, since a list's scripts live in one dir; per gene list the smallest scripted value,
`doomo_part_minv[]`). List ids are u16 (`part_meta.h`: a `GF_*` id, `PL_DEEP0 + DF_x`); `PartRef` is
`{value, top, ofs}` inside per-list `PartList` ranges, `top` being how far the entry reaches above the
head (measured by the build, used by `ph_top_extent` so a tall hat gets room on the bust).

**Slot hooks.** Each painter asks `PS_SLOT(c, list, value, group)` first; a worn value that has a
script is drawn by the script's blocks of that layer group and the painter draws nothing for it. A look
without a scripted value pays one byte compare per hook, so Classic looks are byte-identical
(`portrait_classic_golden`). `sanitize` keeps a value past the painters' range when a script draws it.

| list | hook (painter) | groups |
|---|---|---|
| head_shape | `head_shape()` (draws on the head part: `solid same`) | 2 |
| eye_style | `eyes()`, `ph_face_over()` (third eye / over-hair bits) | 3, 4 |
| brow_style | `brows()` | 4 |
| nose_style, mouth_style, facial_hair | `nose()`, `mouth()`, `facial_hair()` | 3 |
| hair_style | `ph_hair_back()`, `ph_hair_front()` | 0, 4 |
| ears | `ph_ears(c, 0 / 1)` | 0, 4 |
| horns, hat | `ph_horns()`, `ph_hat()` | 5 |
| accessory | `back_acc()` (any view), `neck_acc()`, `ph_face_acc()` | 0, 4, 5 |
| halo | `ph_halo()` (finish) | 6 |
| aura | `pf_aura_back()`, `pf_aura_front()` | 0, 6 |
| tail, wings | `pb_draw_tail()`, `pb_draw_wings()`: once per image, every block | any |
| top, bottom, shoes | OVERLAY: drawn over the plain garment the painters give an unknown value (after `bottoms()` / `torso()` in group 1, after `collar()` in group 4) | 1 (top also 4) |

`parts_build.py` (`HOOKS`) rejects a block in a group its list's hook never runs, and a chibi block on
a deep field that is not chibi-visible (`DFF_CHIBI`), so decorative parts cost nothing in crowds.
Lists without a hook (pattern, marking, body, species, the deep fields until CR5) only take VM demo
values (`state=test` in the catalog).

**Macro ops (the family grammar).** `crown KIND x y w h` (dome, cone, cylinder, flat, slouch, peaked,
beret), `brim KIND x y w h` (none, bill, small, wide, floppy, cocked), `band`, `strap`, `bow`, `ring`,
`chain` (links on a sagging line), `frill` (scallops), `pouch`. Each is one 4-7 byte op drawn with the
current material, shade and solid / decal mode through the same projection as the primitives, so the
sliders, the yaw and the views move it. `def NAME P1 .. / enddef` and `use NAME A1 ..` are build-time
templates (`$P`, `$P+N`). The first family batch, `core/data/parts/hat/family_a.txt` and
`outfit/family_a.txt`: 12 hats (Gumdrop Hat, Stovepipe, Sun Bonnet, Pillbox, Starry Wizard, Captain's
Cap, Bow Beret, Garden Sun Hat, Bell Crown, Tricorn, Lantern Helmet, Lure Bucket), the Pageant Sash
top, the Explorer Satchel and the Pearl Necklace: 15 parts in 628 B (about 42 B a part), drawn and
tested but *hidden* until the lists flip live (CR3) and the art review signs them off.

**The PS1 part arena.** With `DOOMO_PARTS_LZ` the 4-slot 8 KB block LRU became a 2-slot block LRU
(4 KB) plus a 4 KB per-render arena: `pctx_init()` empties it, and each worn entry is copied into it
once, so a render decodes a block once however many groups and hooks run the entry (host test: 7
groups x 3 entries = 3 block loads, was 21 with the LRU alone). Same 8 KB. The compressed bytes come
through `part_lz_src()`; a build with `DOOMO_PARTS_SPU` (CR10) supplies one that DMAs the block from
SPU RAM into a staging buffer.

**The PS1 builds without the VM for now** (`DOOMO_PARTS_VM=0` in `ps1/CMakeLists.txt`): once painters
call it, the VM, its LZ buffers and the arena are about 22 KB resident (measured: the disc ended
7,184 B into the 64 KB stack reserve), more than the 14,976 B the tree had above the reserve. With the
flag every hook is a constant 0 and sanitize draws a scripted value as its slot's default; the PS1
costs +1,888 B resident for CR1/CR2/CR12 (the age gate, the hooks' call sites, the 128 B `minv`
table). CR10 (SPEC P3 reclaims, the SPU-RAM part store) turns it on; until then CR3 must not flip a
list live on the PS1 past its painted values.

**Shape gate.** `portrait_shape_mode(1)` forces every material onto one grey ramp (direct renders only;
a no-op on the GBA, where it costs no RAM). `test_part_render.c` requires every scripted part to differ
from every other value of its slot by >= 24 px of the bust in shape mode (no palette swaps) and by
>= 6 chibi px when it draws on chibis at all, and to render distinct on three citizens.

**Cost weights.** `part_costs_match_the_renderer` measures every drawable part (pixel visits over the
slot's value 0, bust + figure + chibi, units of 64) into `core/data/parts/catalog/costs.txt`; the
catalog carries them (`PartMeta.cost`) for the creator's Busy meter (CR9). The kitchen-sink test keeps
every family hat with a full outfit under the 12k pixel-visit bound.

**Review sheets.** `portrait_sheet --out DIR --slot KEY [--set SET] [--shape]`: every catalogued part
of a slot (live, hidden and planned) as bust 1x, the creator's 2x head crop, figure, chibi 3x and the
fighter's back view, labelled with name, state and set; `--shape` is the shape-only sheet.

**Age.** `look_for_stage()` ends with `part_age_filter_look()` (CR12): a part whose catalog age class is
above the stage is drawn as its slot's default. See docs/CREATOR.md "The age gate for parts".

Tests: `test_part_render.c` (9), `test_part_meta.c` (8), and in `test_portrait_script.c` the LZ blocks
and the arena against the raw scripts for every set, the 2-slot LRU and the once-per-render arena;
`MAXOPT` in `test_portrait.c` is 160. Not done here: art batches (CR4), flipping lists live (CR3),
deep-field hooks and saving (CR5), icons (CR7), the grid picker (CR6), the SPU-RAM loader (CR10).
