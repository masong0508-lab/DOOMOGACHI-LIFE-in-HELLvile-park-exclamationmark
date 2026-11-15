# Main title redo: anime-opening feel, with the huggle motif

Owner, 2026-11-15: "when all is said and done, redo the intro of the tv show, to replicate the feelings,
the pacings and the motions of these anime intros, while incorporating the huggle motif into the
song / chorus-bridge". Queued: it starts after the current work (speed, Talk, creator, levels, Dreamcast).

The intro is the main title of the pilot, "Doomtastic Times" (`PILOT_ROADMAP.md`, the 0:50 cut boarded
by song section; `PILOT_BEATSHEET.md` "INTRO: HAHALAND FROM ABOVE").

## The three references (the owner's uploads; only these measurements are kept, never the media)

The songs and footage are someone else's work. We take the feel, the pacing and the kinds of motion,
and write our own melody, lyrics and pictures. Nothing is sampled, traced or copied.

| Reference | Length | Shots | Average shot | Tempo (estimate) | Cuts per 9 s, start to end |
|---|---:|---:|---:|---:|---|
| Attack on Titan, opening 1 | 1:32 | 97 | 0.95 s | ~185 BPM | 4, 0, 1, 8, 15, 5, 14, 3, 15, 31 |
| Going Wild Going Green (theme) | 1:30 | 77 | 1.17 s | ~152 BPM | 3, 4, 14, 11, 20, 8, 5, 7, 3, 1 |
| Shinryaku! Ika Musume, opening | 1:29 | 47 | 1.90 s | ~185 BPM | 10, 1, 2, 2, 2, 2, 14, 2, 3, 8 |

Measured with ffmpeg scene detection (threshold 0.30) and an onset autocorrelation; tempo can be off by
a factor of two. All three stay at full loudness from the first seconds to the last.

What the numbers say:

* **Attack on Titan: the slow fuse.** Almost no cuts for the first 27 s (held, heavy shots), then bursts
  that grow to 31 cuts in the last 9 s. The feeling comes from holding back, then letting go.
* **Going Wild Going Green: the bounce.** Brisk from the start, busiest in the middle (20 cuts in 9 s),
  then it calms down into a long final pose.
* **Ika Musume: the comedy rhythm.** Long held shots (about 2 s) with two bursts: one at the very start
  and one at the chorus (14 cuts in 9 s). The jokes live in the holds.

## The plan (defaults chosen by Claude, owner may override)

* **Length:** a full 1:30 opening (the anime standard) as the series main title; the pilot's 0:50 cut
  stays as its short version and is re-cut from the long one.
* **Pacing map:** intro and A-melo hold like Ika Musume (2 s shots, gags in the holds); the B-melo builds
  like Attack on Titan (holds turning into bursts); the chorus runs at the Going Wild bounce (about 1 s
  shots, everything on the beat); the bridge drops to a single held shot; the last chorus is the Attack on
  Titan finish (cuts every half beat), into a calm end pose.
* **Motions** (from the references, made ours): the hero run toward camera, the cast line-up pops on the
  beat, whip pans and smear frames between groups, the big sky crane, speed-line backgrounds on the
  chorus, a sudden freeze-frame gag, the final group pose and logo slam.
* **Tempo:** around 170-180 BPM for the J-pop drive; the existing master's keys and hits stay where they
  fit (the six-hit stab of the GAG_BIBLE, the drop as a record scratch).
* **The huggle motif:** the owner's term. Read as a hug-and-snuggle hook: a short, singable "huggle"
  vocal figure that is the chorus's answer line and comes back, slower and alone, as the bridge, with
  the picture of the cast piling into one big hug (Mascotty still in the middle, not hugging). Every hug
  stays a plain hug a child may decline; nothing suggestive. If the owner means something else by
  "huggle", the hook changes, not the plan.
* **The show's own rules stay:** the mirrored knock at the Wall, the gate with seven padlocks, Hellify
  rotting the town on the sweetest line, the pink paint roller over the red drip, the Hellvile side in
  silhouette. These become shots inside the new pacing.

## Steps when it starts

1. Song: a new arrangement and lyric of "Doomtastic Times" at the new length, with the huggle hook in
   the chorus answer and the bridge; a demo with a click track and the section map.
2. Board: shot list timed to the demo (shot lengths per section from the map above).
3. Animatic, then the cut, then the 0:50 short version from it.
4. Every pass is compared against the pacing table above (cuts per 9 s and shot lengths, measured the
   same way), so "it feels like those openings" is checked with numbers as well as by eye.
