# DOOMOGACHI LIFE in HELLvile Park! - teaser trailer

A 78-second comedy teaser for the game, as pixel animation. **It is not captured
gameplay.** The game (a real GBA ROM plus a real PS1 disc) is still in
development. Every frame of this trailer is drawn procedurally in C at the GBA's
native 240x160, using the game's **real palette** (`core/include/doomo/palette.h`,
which is generated from `core/src/palette.c`). That means the colours match what
the game uses, but the staging, characters and jokes are made for the trailer.
The soundtrack and sound effects come from a small built-in chiptune synth.

Outputs: `trailer.mp4` (960x640, 30 fps, H.264 + AAC 48 kHz stereo, about -16 LUFS)
and `trailer.gif` (480x320, 15 fps, under 15 MB).

## Building

You need `gcc` (or any C11 compiler), `python3` and `ffmpeg`.

```sh
cd trailer
make                       # build/trailer.mp4 + build/trailer.gif
make BUILD=/some/dir       # put everything somewhere else
make preview SCENE=s04     # contact sheet of one scene -> build/sheet_s04.png
make list                  # scene list with start times and frame counts
```

The renderer is a single binary (`build/trailer`) with these modes:

```sh
build/trailer --list
build/trailer --png  s07 120 out.png      # one frame of a scene, 4x
build/trailer --sheet s07 out.png [STEP]  # contact sheet, 2x
build/trailer --raw > frames.rgb          # all frames, 240x160 rgb24 @ 30 fps
build/trailer --wav out.wav               # the soundtrack (22.05 kHz mono)
```

`make` masters the audio when it encodes the MP4: +5 dB into a -1 dBFS limiter,
at 48 kHz stereo. The raw synth mix peaks at -4 dBFS and never clips.

To iterate on a few scenes without the rest, build only those files. Scenes
that are left out render as placeholders:

```sh
make BUILD=/tmp/t SCENE_SRCS="scenes/s04_doomogachi.c" MUSIC_SRCS="" /tmp/t/trailer
```

**1080p cut with the studio logo:** `./assemble.sh build/trailer out.mp4 [out.gif]`
puts the Dippinn Productions logo bumper (`assets/dippinn_logo.mp4`) in front.
It then scales the pixel trailer 6x with nearest-neighbour to 1440x960 and
pillarboxes it in 1920x1080, for about 88 s in total.

## How it is put together

| file | what it does |
|---|---|
| `tk.h` / `tk.c` | Toolkit: framebuffer of palette indices, primitives, 5x8 font, outlined captions, GBA dialogue box, sprites, particles, post effects (hellify, shake, glitch, flash, fade, iris wipe), synth and sfx. |
| `tk_chibi.c` | The plush critter-folk citizen renderer, drawn natively at sizes 1-4. The ready-made cast is `TK_HERO`, `TK_FUZZ`, `TK_RIVET`, `TK_VEX`, and so on. |
| `timeline.c` | Scene order and durations. This is the single source of truth for timing. |
| `scenes/sNN_*.c` | One file per scene. Each file has `scene_X(f, n)`, which draws local frame `f`, and `scene_X_audio(t0, dur)`, which schedules sfx. |
| `music.c` | The score. It is pinned to `tk_scene_start()` plus each scene's frame constants. |
| `main.c` | The CLI (png, sheet, raw, wav). |

**If you re-time a beat inside a scene,** update the matching `FR(n)` in
`music.c`. The score deliberately drops out for comic beats: the splat, the
wall, the starer, "Everything is fine.", "(We judge a little.)", the failed
exorcisms, and the deadpan PlayStation line.

## Scene list

| # | scene | start | len | what happens |
|---|---|---|---|---|
| 1 | `s01_splash` | 0.0 s | 2.5 s | "a HaHaland production (100% demon-free!)". A devil tail flicks out from behind the heart mascot for 3 frames, the heart freezes and side-eyes the camera, then the footnote appears: "*mostly". |
| 2 | `s02_title` | 2.5 s | 4.0 s | The puffy title drops in and the O's are googly eyes. A red drop forms under HELL, gets flung at the lens and splats. A star sticker goes over it with "nothing to see here! ♥". The heart iris-out closes on the sticker. |
| 3 | `s03_hahaland` | 6.5 s | 5.0 s | A parallax pan through candy town: "Welcome to HaHaland!", "Population: 500 very nice people". A record-scratch stop at the Wall shows red eyes, a party hat, and "(and 500 not-so-nice people next door)". |
| 4 | `s04_doomogachi` | 11.5 s | 5.5 s | The egg hatches ("beep."). It eats a cupcake, poops, and "It loves you!". Then four frames of pentagram hell, and back to "beep." with tiny horns: "...it's fine." |
| 5 | `s05_creator` | 17.0 s | 5.0 s | The citizen creator cycles faster and faster: "Create ANYONE." "Literally anyone." It settles on Kevin, clicks EYES +, snap-zooms, and a third eye opens: "Yes, three eyes." The iris closes on the third eye, which blinks alone. |
| 6 | `s06_life` | 22.0 s | 5.0 s | A dollhouse apartment block at night. A high-five escalates into a POW ("Make friends!"), a heart burst ("Fall in love!"), Nana gives her own cupcake 10/10 ("Eat cupcakes!"). In the dark room, the one plain human stares: "Ignore that one." The iris closes on him while he slowly waves. |
| 7 | `s07_vape` | 27.0 s | 5.5 s | Cloud Nine Vapor's grand opening. Vex: "Try Brimstone Blast. It's organic. *may contain souls". The bunny inhales, FWOOMP. Grinning, possessed, still in the party hat: "Everything is fine." |
| 8 | `s08_invasion` | 32.5 s | 6.0 s | Park party, then a record scratch, "beep.", and the sky cracks. Lord Vapula rises with a crooked crown and a giant vape: "THE DEMONS OF HELL HAVE INVADED (wherever you are)", with proof polaroids of the bath, the bakery and your bed. |
| 9 | `s09_weld` | 38.5 s | 4.5 s | Rivet welds a rifle part by part: "WELD YOUR WEAPON." A stray spark sets Rivet's tail on fire, the DAYS SINCE LAST FIRE sign flips 7 to 0, and Rivet gives a proud thumbs-up, still ablaze. Then CUTE plush launcher VS BRUTAL super shotgun: "(9 part slots. Millions of guns.)" |
| 10 | `s10_shooter` | 43.0 s | 8.5 s | Raycast corridor. CUTE MODE: a teddy round, the imp pops into confetti, "*squeak*". BRUTAL MODE: shotgun gibs. "(We don't judge.)" ... "(We judge a little.)", and Father Fuzz says "tsk." Then "...or third person!" with a minigun and GORE: BOTH?! |
| 11 | `s11_exorcism` | 51.5 s | 7.5 s | "THE POWER OF CUDDLES COMPELS YOU!" The demon hisses at the holy water: "ow. my everything." EXORCISM FAILED x1 (spray), x2 (hug), x3 (bucket). "SOUL LOST." The ghost has the map upside down. "Lay to Rest?" ... the cursor sneaks to MONSTER MODE: "(or turn on MONSTER MODE, you monster.)" |
| 12 | `s12_census` | 59.0 s | 4.0 s | A counter rolls to 1000 CITIZENS. 500 GOOD do a stadium wave, 500 BAD sway. 1 PARK: the fence squeezes everyone in, one bear pops out like a cork, "(cozy.)" |
| 13 | `s13_ending` | 63.0 s | 5.0 s | "...and maybe, just maybe..." The Wall comes down and both sides run to each other: "everyone can get along." Kevin and the starer are in the crowd. The camera tilts down to a smoking mountain of shell casings and Vapula's dented crown: "(after a LOT of shooting.)" tink. |
| 14 | `s14_platforms` | 68.0 s | 4.5 s | "Coming to GBA and PlayStation". In deadpan silence: "Yes, the actual PlayStation. The grey one." Then the two consoles high-five. |
| 15 | `s15_logo` | 72.5 s | 5.5 s | Final title card. HELL catches fire and the bottom half rips into Doom brick: "(it's fine.)", "IN DEVELOPMENT", "GBA ♥ PlayStation". Post-credits: the horned Doomogachi poops, the poop grows horns too, "beep." |

**Running gags:** the Doomogachi's "beep." (s04, s06, s08, s15). "it's fine."
(s04, s07, s08, s15). The dripping red HELL (s02, s14, s15). The pink party
bunny goes from customer to possessed, through the exorcism, to cured at the
ending (s07, s08, s11, s13). The crowned cloud appears as the "employee of the
month" (s07), in Rivet's window (s09) and as Lord Vapula himself (s08). Kevin
and the unblinking starer both turn up at the ending party.

The trailer uses no real brands or console logos. The hardware is drawn as
generic shapes, and only the platform names appear, in plain text.
