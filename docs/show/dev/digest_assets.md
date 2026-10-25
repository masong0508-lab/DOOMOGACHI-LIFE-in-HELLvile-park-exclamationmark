# Digest: production assets the pilot can reuse

> **Development reference.** An inventory of the repo's reusable art, toolkit and audio, made for the pitch round. The production plan that uses it is `../PRODUCTION_ROADMAP.md`. The concept-sheet items it lists as "must be cut or retuned" are out of the show (`../GAG_BIBLE.md` §7.6). See `README.md` in this folder.

Repo root: /home/user/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark. All paths below are relative to it.

## 1. Visual style (ART.md, ARCHITECTURE.md "Art direction", PORTRAIT.md, tk_chibi.c)

- **Look:** premium late-era GBA/PS1 pixel art that must never look cheap. The target is Tomodachi Life / Tamagotchi at late-GBA quality.
- **Critter-folk rules:**
  - Huge heads on tiny round plush bodies.
  - Fur tufts on the cheeks, crown, chest and tail tip.
  - Big expressive animal ears and fluffy tails.
  - Muzzles, button noses and paw hands.
  - Big glossy eyes with two highlights.
  - Species: cat, fox, bunny, bear, pup, mouse, sheep, raccoon, owl, frog and moth folk.
  - Demonic parts (horns, wings, fangs, devil tails) are drawn *adorable*.
  - Only POSSESSED and SOUL_LOST residents look creepy: hollow red eyes, veins, drippy black tears, a glitch and smoke aura, drained colour. GHOST residents are translucent cyan with a wispy tail.
- **Rendering:**
  - 3-4 tone hue-shifted shading: shadows lean cool and purple, highlights warm.
  - Cool rim light.
  - Coloured selective outlines in a dark shade of the local colour. Pure black is used only at contact points.
  - Soft dithering only where it reads as fluff.
  - "Nothing is ever static": breathing, blinks, ear twitches and tail wags run on co-prime periods.
  - Every pop-in has anticipation and overshoot.
- **Contrast is the premise:**
  - HaHaland/Hillvile is a sugary pastel candy-land.
  - Hellvile is grotesque Doom: flesh, bone, blood, lava, rust and vape clouds.
  - The CUTE gore setting turns violence into confetti, hearts and plush stuffing.
- **Palette:** `core/src/palette.c` holds 256 hand-picked colours.
  - UI colours (pink, mint, cream, vape purple, LCD green).
  - 32 paints x 3 shades.
  - 8 skin tones.
  - 15 eight-shade ramps: STONE, WOOD, GRASS, PASTEL_PINK, SKY, VAPE, CANDY, MINT, PEACH, BLOOD, LAVA, FLESH, BONE, SICK, STEEL.
  - Tints: damage red, holy gold, sick green, vape haze, and NIGHT (emissives such as lava, gold and windows stay lit).
  - This palette is the show's colour script.
- **Ready-made plates (`artgen --sheet`):**
  - 47 seamless 64x64 textures, cute set and hell set. Examples: SHOP_FRONT, WORKSHOP_FRONT ("Rivet's"), VAPE_SHOP_FRONT ("Cloud Nine Vapor"), CHURCH_WALL, FLESH_WALL, PENTAGRAM_FLOOR.
  - 5 sky panoramas at 256x96: PASTEL_DAY, SUNSET, NIGHT_STARS, HELL, VAPE_STORM.
  - 11 demons with 8 frames each (walk, attack, pain, death): Impling, Vape Wraith, Puff Demon, Hellhound Pup, Bone Jester, Slime Belcher, Gargoyle, Cyber Piggy, Brute, Cloud Spider, Lord Vapula.
  - Props and FX, including the Doomogachi egg and pet sprite.
- **Model sheets for any of the 1000 citizens (`core/tools/portrait_sheet.c`):**
  - Views: bust 48x56, figure 48x80, chibi 16x24 (4-direction walk), fighter 32x48.
  - 11 expressions and statuses (possessed, soul lost, ghost, dead).
  - 24 species, 32 hairstyles, 24 hats.
  - Every option is visible and covered by tests.
- **Concept sheets (`docs/design/story/concept/`, which I looked at):**
  - All 5 sheets are Grumbleguts only, drawn by `trailer/concept/grumbleguts.c` (binary at `trailer/build/ggsheet`).
  - His design: crimson, glossy and rubbery; bone horns; vape-purple bat wings; peach belly; peach cheeks; studded leather harness and belt; brass studs; chains; spiked striped tail.
  - `gg_turnaround`: front, side, back, taunt.
  - `gg_poses`: monologue + bagel, sucks in, sproing, unbuckle, the crop.
  - `gg_feast`: full front/back, a scale shot against a citizen chibi, and a "Keeper of the Seal" card.
  - `gg_storyboard`: a 6-panel Feast of Rebuilding on a crowd strip.
  - `gg_closeup`: a reaction cut with eyes bulging one at a time; a censor sticker reading "HELLVILE PARK BOARD OF DECENCY (approved)"; a thermometer-belly X-ray ("60% air, 40% bagel").
  - **Reusable as is:** the scale shot, the reaction cut, the censor-sticker gag and the X-ray.
  - **Must be cut or retuned for a stated broadcast rating:**
    - the hose close-up with hands on the cheeks;
    - "BRAAP" gas both ways;
    - the crop and unbuckle "flirt" poses;
    - the "...that's hot" crowd line.
  - Tone down the harness and crop to a "gatekeeper uniform". Keep him to vanity, belly and butt-wiggle slapstick.
  - `docs/design/story/refs/*.webp` are third-party character references (a Cow and Chicken devil, Baloo). They are for inspiration only and must never appear in pitch material.
- **Media (`docs/media/`):**
  - `title.png`: the puffy pastel logo with rainbow and hills, and the star sticker "nothing to see here! ♥" covering a red drip.
  - `trailer_scenes.png`: 8 trailer stills.
  - `ui_gallery.png`: the window skins CUTE, MINT, CREAM, HELL, LCD and DARK, a dialogue box with name plate and portrait, toasts, a keyboard. These suit lower thirds, eyecatch cards and title cards.
  - `dippinn_productions.png` (the studio wordmark) and `trailer/assets/dippinn_logo.mp4`: the studio ident bumper.
  - `gba_engine.png` and `ps1_engine.png`: real engine captures for the "it's also a game" slide.

## 2. Trailer toolkit (`trailer/`): built and working here (gcc, python3 and ffmpeg are present; `build/trailer --list` runs)

- **Format:**
  - 240x160 canvas (3:2) at 30 fps, drawn procedurally in C with the real game palette.
  - 78 s in total: 15 scenes, 2340 frames.
  - Outputs:
    - mp4 at 960x640 with AAC 48 kHz, mastered to about -16 LUFS;
    - gif at 480x320;
    - a 1080p cut from `assemble.sh`: the logo bumper, then a 6x nearest-neighbour scale to 1440x960, pillarboxed.
- **CLI:**
  - `--png scene frame` writes one frame at 4x.
  - `--sheet scene` writes a 2x contact sheet, which works as storyboard thumbnails.
  - `--raw` writes rgb24 to pipe into ffmpeg.
  - `--wav` writes the 22.05 kHz mono synth track.
  - `make preview SCENE=` and `make list`.
- **Animatic-ready:**
  - `timeline.c` is the single source of truth for timing: `X(name, secs)`.
  - Unwritten scenes render automatically as weak placeholder slug cards ("name / (scene coming soon)"). Listing the pilot's beats with durations therefore gives a timed animatic immediately, and each placeholder can be replaced one by one.
  - Music is pinned to `tk_scene_start()`, so the score follows when scenes are re-timed. Beat constants inside a scene, however, are duplicated in `music.c` via `FR(n)` (a known coupling hazard).
- **Primitives:**
  - Shapes and dithered gradients.
  - A 5x8 font with outlined captions and typewriter text.
  - The GBA dialogue box with a speaker plate, in all 6 window skins.
  - ASCII-art sprites.
- **Post FX:**
  - `hellify` (0-16): the cute palette rots into hell pixel by pixel. This is the core visual metaphor.
  - shake, glitch, flash, fade, darken, scanlines.
  - Iris wipe, as a circle or a **heart**.
- **Particles:** confetti, gibs, sparkles, hearts, vape cloud, stars.
- **Easing:** ease, bounce and triangle-wave helpers.
- **Chibi renderer (`tk_chibi.c`):**
  - Draws natively at sizes 1-4 (about 24x32 up to about 96x128), with more detail at larger sizes rather than fat pixels.
  - 11 expressions, 8 mouths (cycling OPEN/O/SMILE gives a crude lip-flap), 9 hairstyles, 10 ears, 9 tails.
  - Horns, wings, halo, hats (party, priest, marine helm), blush and fluff.
  - Idle clock.
  - `tk_random_citizen(i, bad)` makes deterministic crowds.
  - Presets: TK_HERO (fox), TK_MAYOR (sheep, angel), TK_FUZZ (bear, priest), TK_RIVET, TK_NANA (bunny), TK_PIP (mouse), TK_VEX (cat), TK_GOTH, TK_IMP.
  - **Missing presets:** Steamy, Sootkin, Bolt, Wisp, Mascotty, Kevin (Kevin is built inline in s05), GURGLE.
- **Set pieces already animated (15 scenes, 14.5k lines):**
  - s01 splash heart; s02 parallax title crane-up; s03 parallax town pan with a record-scratch brake at the Wall.
  - s04 LCD pet hatch; s05 creator UI; s06 Tomodachi dollhouse cut-away of Hillvile Towers at night.
  - s07 Cloud Nine Vapor opening and possession; s08 party freeze, sky crack and Lord Vapula rising.
  - s09 Rivet's weld bench; s10 a first-person **raycaster** with cute and brutal modes; s11 the St. Cuddles church exorcism, rule of three.
  - s12 1000-citizen census crowds; s13 the sunset Wall crumble; s14 a product studio; s15 the logo with a post-credits sting.
- **Limits:**
  - No camera system or 3D. Each scene hand-codes its transforms.
  - No sample, voice or external audio playback (synth only).
  - The theme song "Doomtastic Times" and any voice-over must be muxed with ffmpeg.

## 3. Music (`docs/design/music/SPEC.md` §14-15, `trailer/music.c`)

- **The shared cell E-E-G:**
  - HOOK: C major, 140 bpm, head E-E-G-C'. The sunny HaHaland and title theme.
  - RIFF: E minor, head E-E-G-B♭. Hellvile, from folk swing to war march. A **redeemed** form also exists.
  - A pivot through the shared cell flips the world.
  - COMMINGLING is the E-major anthem: the HOOK over the redeemed RIFF.
  - MINGLE (F major, the music box, head C-F-A-F) is the destination key. The finale lifts E to F.
- **Other motifs:**
  - WALL: `e f e b-`, resolving to `e f+ e b`.
  - BEEP: the piezo "beep." motif.
  - VEX: Am glides.
  - VAPULA, FLUFFY.
  - HEE HEE bell.
  - FUZZ hymn and amen.
  - RIVET: G mixolydian plus an anvil.
  - HOME, LOVE, MAYOR, NANA, PIP, TRUE NAME, GOODBYE.
  - GRUMBLEGUTS: a 7/8 gate engine with the RIFF in major.
- **The dial:** S0 lullaby, S1 HaHaland, S2 E-minor swing, S3/S4 metal. It doubles as an act-escalation map.
- **Vinyl FX:** NEEDLE, STOP, TAPESTOP, REWIND (flashback bus), SPINBACK, WOBBLE, STUTTER.
- **About 70 stingers:** ITS_FINE (cheery C over a wrong F♯ bass), DUN_DUN_DUNN, SAD_TROMBONE, RIMSHOT, HALLELUJAH, IRIS, VICTORY and others.
- **24 fear stings.**
- **The trailer score:** a hand-scheduled chiptune with square, triangle, saw, noise and sine voices, a choir and power chords. Its anthem cut (s13) and its CUTE-mode riff transposition (s10) are templates for the theme song's DOOM/cute switch.

## 4. SFX and timing kits

- **Trailer SFX (20):** BLIP, SELECT, BOING, POP, SQUEAK, SPLAT, SHOTGUN, EXPLOSION, SCRATCH, HEARTBEAT, BEEP, CHOIR, FANFARE, WHOOSH, ROAR, SPARKLE, SPLASH, THUD, GLITCH, KAZOO. The recipes are at `tk.c` lines 500-519.
- **Game comedy kit (story SPEC C11, 15 synth one-shots):** SCRATCH, BOING, POP2, THUD, WHOOSH, KAZOO, BEEP, GLITCH, TIK, SHWIP (the iris sound), TINK, FANFARE, CHOIR, PLUCK, CREAK, plus VOICE_GURGLE and voice babble.
- **Comic Timing Bible (`comic_timing.md`):**
  - 12 rules.
  - A beat ladder of 2/3/5/8/13/21/26 frames at 30 fps.
  - Entrances take 4-6 frames with overshoot.
  - Whiplash inserts last 3-4 frames.
  - Rule of three at an identical interval.
  - A button 12-25 frames after the punch.
  - Crowd ripples of 0-3 frames.
  - A silence budget of about 23%.
  - §5 has density curves: cutscene, 1 gag per 1.5-3 s; heartfelt, 0 gags for 3-5 s; horror is comedy with the release removed; never undercut the final sincere beat.
  - The trailer runs at 1 gag per 1.7 s, which is too dense for 22 minutes. Scale the structure, not the density.
- **Running gags:**
  - "beep."
  - "it's fine."
  - The dripping red HELL with its "nothing to see here!" sticker.
  - The party bunny arc.
  - The crowned cloud as "employee of the month".
  - Kevin and the starer.
  - The DAYS SINCE LAST FIRE sign.
  - The Board of Decency sticker.

## 5. Gaps and upgrades for a "well produced" show

- **Canvas:** move to a 16:9 canvas, either 384x216 (x5 = 1080p) or 480x270 (x4). TW/TH are `#define`s, but the scenes hard-code their layouts.
- **Frame rate:** keep 30 (29.97) fps so the timing ladder carries over unchanged.
- **Audio:**
  - Real voice cast, foley and the external theme stems.
  - Mix at 48 kHz stereo/5.1.
  - Deliver at -24 LKFS (ATSC A/85) or -23 LUFS (EBU R128), with -16/-14 LUFS for streaming. The trailer's -16 LUFS is not a broadcast spec.
- **Pipeline:** add a camera/rig layer, mouth-shape lip-sync and an external-audio track in the timeline. Fix the `FR(n)` coupling hazard by adopting the game's named-beat approach.
- **Rating flags:**
  - Vaping as demonic corruption: keep it satirical, never glamorised, no real brands.
  - Brutal gibs: the pilot should use the CUTE gore set.
  - The Grumbleguts storyboard items listed above.
