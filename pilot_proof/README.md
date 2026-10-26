# Pilot proof scene - cold open, live-action half (pages 2-4)

A 70-second proof that the pilot can look and move like the teaser trailer.
It is built on the teaser's own toolkit in `../trailer` (`tk.c`, `tk_chibi.c`,
`main.c`, the game's real palette and the chiptune synth), which it reuses
without changing anything there. Every frame is drawn procedurally at 240x160.

Pacing: shots last 4-8 s, with a few longer comic holds. The energy comes from
inside each shot: constant squash, stretch and overshoot, camera push-ins and
drifts, sfx hits, pop-in sound words, and small shakes on impacts. Extreme
close-ups are used only for the big beats (the M and the egg).

## Build

```sh
make BUILD=/some/dir                       # /some/dir/pilot
/some/dir/pilot --list | --png SHOT F out.png | --sheet SHOT out.png | --raw | --wav out.wav
./assemble.sh /some/dir/pilot TAKES_DIR out.mp4   # 1080p, AAC, with the Mayor takes
```

`TAKES_DIR` is the show's locked Mayor take store (`show_lib/takes/mayor/ep101`).
The takes are looked up by the line ids in `show/ep101/voice.lock`:

- `co12_hum`: the creator's recorded hum of the melody
- `co13_heehee`: "...Hee. Hee."
- `co15_perfect`: "Perfect."
- `co18_stay`: "Stay."

Each take runs through a dialogue chain (rumble cut, denoise, de-mud,
presence, de-ess, 3:1 compression, light slap, loudness match) before it is
placed in the mix. The new short lines are dialogue boxes with synth babble.

## Files

| file | what it holds |
|---|---|
| `wd.c/h` | World-space camera drawing: shaded blobs, tris, lines, world-scaled font, crayon post |
| `mayor.c/h` | Mayor Giggs as a turkey with Mad Hatter energy, posable, drawn natively at any zoom |
| `sets.c/h` | Bandstand at dawn, GIGGLE banner with an old mystery M underneath, fountain, Wall, plaza, bakery, egg, crayon inserts |
| `shots.c` | The 13 shots and their sfx |
| `music.c` | The melody (from the creator's pad1.mid, 150 bpm, 3/4) as a music box and as a waltz |
| `timeline.c` | Shot order and durations |
