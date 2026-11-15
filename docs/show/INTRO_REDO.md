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

Owner, 2026-11-15, on the first draft of this plan: the structure should be refined and properly
structured instead of weird brokenness, especially the weird interruption that reveals the "nothing to
see here" gag; but it must still feel cheeky. So: **one clean, standard anime-opening form, on a fixed
bar grid, with no stops, no record scratch and no tempo breaks.** The cheek comes from what happens on
the beat, never from breaking the beat.

### The song: one steady form

* **176 BPM, 4/4, one key** (a single lift of a whole step for the last chorus, the classic opening
  move); one bar = 1.36 s; **66 bars = 1:30.0** exactly. Every phrase is 4 or 8 bars.

| Section | Bars | Time | Music | Picture (shots cut on the bar grid) |
|---|---:|---|---|---|
| Intro | 8 | 0:00-0:11 | the chorus hook on lead and bass, full band from bar 1 | crane up over pastel HaHaland; the logo letters land one per beat on bars 7-8; 2-bar shots |
| A-melo (verse) | 16 | 0:11-0:33 | the story verse, steady groove | the cast introduced on the beat, one character per bar; Mascotty simply in every shot, perfectly still; 1-bar shots |
| B-melo (pre-chorus) | 8 | 0:33-0:44 | the build: rising line, drums double up in bars 5-8 | the mirrored knock at the Wall, both sides on the same beat; half-bar shots in bars 5-8; the gate with seven padlocks on bar 8 |
| Sabi (chorus) | 16 | 0:44-1:05 | the main hook; the huggle answer line after each sung line | everything on the beat: speed-line runs, Hellify rotting the town on the sweetest line, three candles out in rhythm; 1-bar shots, 2 per bar on the last 4 bars |
| Bridge (huggle) | 8 | 1:05-1:16 | the huggle hook alone, half-time feel, then the band builds back over bars 5-8 | one slow push-in on the cast piling into one big hug (Mascotty in the middle, not hugging); held, then 1-bar shots as the band returns |
| Last chorus (key lift) | 8 | 1:16-1:27 | the last half of the chorus a step higher | the fastest cutting of the song, still on the grid: half-bar shots, every main gag of the show as a beat-synced flash |
| End pose | 2 | 1:27-1:30 | the final hit and a short ring-out | the two halves meet on the logo; the Mayor's pink roller paints over the red drip on beat 1; the drip creeps back through on beat 4 |

* **No interruptions.** No silence gaps, no record-scratch drop (the old 0:02 "drop" is now the last bar
  of the B-melo, a drum fill into the chorus), no tempo change. The song never stops to make a joke.
* **The huggle motif:** read as a hug-and-snuggle hook (the owner's term): a short, singable "huggle"
  vocal figure that answers each chorus line and becomes the bridge, over the big group hug. Every hug
  stays a plain hug a child may decline; nothing suggestive. If the owner means something else by
  "huggle", the hook changes, not the form.

### The cheek, kept on the beat

* **"Nothing to see here! ♥"** stays, but as a beat, not a break: in the intro's bar 8, a red drop splats
  on the lens on beat 1 and the star sticker slaps over it on beat 3 with a snare hit (the band keeps
  playing). The same sticker comes back once more on the last chorus, a beat late, as the button gag.
* **Cheek in the holds, not the gaps:** characters glance at the camera on off-beats, Mascotty never
  moves while everyone else bounces, the Hellvile side mimes along a bar behind, the sweetest lyric
  over the nastiest picture.
* **Motions** (from the references, made ours): the hero run toward camera, the line-up pops, whip pans
  between groups, the big sky crane, speed-line backgrounds on the chorus, the final group pose and
  logo slam. All cut on the bar grid.

### Same fix in the trailer

The trailer's title sting (`trailer/README.md` shot `s02_title`, `docs/design/story/comic_timing.md`
row s02) has the same break: the splat, 10 frames of silence, then the sticker. When the redo runs it
gets the same treatment: the music keeps going and the sticker lands on a beat.

## Steps when it starts

1. Song: a new arrangement and lyric of "Doomtastic Times" in the form above (176 BPM, 66 bars), with the
   huggle hook in the chorus answer and the bridge; a demo with a click track and the bar map.
2. Board: shot list timed to the demo (shot lengths per section from the map above).
3. Animatic, then the cut, then the 0:50 short version from it.
4. Every pass is compared against the pacing table above (cuts per 9 s and shot lengths, measured the
   same way), so "it feels like those openings" is checked with numbers as well as by eye.
