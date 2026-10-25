# show/DECISIONS.md - decisions taken without waiting for the creator

Every workflow appends here (one line or a short block each, newest last).
Each names the default taken and where to change it.

## ART: the cast (show/series/cast.shw, looks/*.shw, acting.shw), 2026-10-25

- **Look text format.** `look <id> base=roster:<N>|look:<id>|SPECIES_* ... end` with the roster
  text's keys (docs/ROSTER.md: `hair=`, `eyes=`, `hat=`, ...) and full engine names as values
  (`HAIR_POMPADOUR`, `PAINT_ROSE`, `SKIN_PEACH`, `CHEEK_BLUSH,CHEEK_DIMPLES`).  `char <id>
  look=...` in cast.shw; a line that starts with `key=value` continues the statement above
  (the spec's sec.9.1 excerpt).  Parser, validator and 48-byte packer:
  `show/tools/showtk/art/cast/lookdata.py` (`Bible().char_look_bytes(id)`).  Change: that module.
- **The cast starts from the game's own citizens.** Every named character with a roster record
  uses it (`base=roster:<N>`) and lists only what the show changes, so the show and the game
  draw the same people.  Built from parts: Sunny, Kevin, the storybook kids, Fluffy, the
  insert silhouettes.
- **Mayor: bow tie, no sash.** The pilot script pins a JUDGE rosette on his bow tie and has
  him clutch the clipboard to it, so the roster bow tie stays; the "festival sash" is not used.
- **Nana's apron = TOP_MAID** (the catalogue's dress with a white apron), peach, polka dots.
  Her roster heart aura is off in the show (it fought her R8 looks at bust size).
- **Pip's JUNIOR FESTIVAL PATROL sash = a tangerine cap and hi-vis bib** (no sash part in the
  catalogue); **Mr. Flops = ACC_TEDDY in white** (the only hugged-plush part; it reads as a
  bear: a one-floppy-ear flipbook is deferred).
- **Bonejangles bares his ribs** (TOP_NONE on a skeleton draws the ribcage) and wears a bow tie;
  possession is a state (`state=possessed` / RES_POSSESSED), never a separate look.
- **Kevin is a bald human with stubble** and `eye_count=3`: critter ears, hats and any hair line
  hide a forehead eye at bust size; a bare brow shows it at every size, the stubble reads 30s.
- **Dr. Bloom: lavender scrubs, EYE_DROOPY**; **Miss Lulla: EYE_SLEEPY**: their roster eyes
  vanished at bust size (into the ghoul's dark sockets / into white wool).
- **Mascotty = her roster bunnykin with a costume head**: EYE_WIDE, MOUTH_STITCHED, no brows or
  hair, a one-piece rose suit.
- **Rivet (MT-2 pop-in) wears a gas-mask respirator under a welding helmet**: her face is never
  shown before 108.
- **Fluffy = SPECIES_SLIME in white** (the round, legless body reads as a cloud); heart eyes
  are EXPR_LOVE.  The crayon look is the shot's PAL_CRAYON lerp, not other looks.
- **Lip flap look-dev.** Speakers' `mouth_size` raised 2-5 steps so the 8 visemes read at
  `px=2`; Miss Lulla's rest mouth is MOUTH_SMILE (her roster MOUTH_OPEN reads as mid-word).
  The sheets use the spec's sec.9.3 viseme defaults until AUDIO's visemes.shw lands.
- **Crowds are K-sets of real citizens** (`looks/crowds.shw`): adult sets hold no KID/TEEN
  citizen; the daycare kids are 11 roster kids who pass the kid-look check (Wobbles and Puff Jr
  wear MOUTH_LIPSTICK in the roster, Wafer a TOP_PRIEST: not cast).  Storybook kids are
  explicit looks (`looks/storybook.shw`), never random generation.
- **Kid looks are checked, not trusted** (`lookdata.kid_problems`): height <= 4, a kid's body,
  no facial hair, no adult-coded top/shoes/mouth/marking/accessory/aura/eyes.  An error, never
  a waiver.
- **Grumbleguts has no 101 image.** His cast entry carries `first=ep102`; the sheets and the
  lineup skip him.  The MT-5 shape behind the shut grille is a featureless set silhouette.
- **Model sheets render on the host** (`showtk.art.cast.sheet`: the game's portrait engine compiled
  natively with a patched copy of portrait_head.c carrying hunk (d)) until show.exe can draw
  them; they are review material, never master pixels.
- **A KID's model sheet never draws the possessed, ghost or dead states** (blank "never: KID"
  panels), and `lookdata` refuses `state=` other than normal on an age=child char.
- **register=crayon** marks the storybook cast and K-sets; the lineup previews them through the
  PAL_CRAYON lerp (camfx.c's formula); on the PS1 it is the shot's `look=crayon`.
- **Acting macros are picture-only** (`series/acting.shw`): no sound op in a macro; the shot adds
  the TIK, the creak, the whistle.  `headturn` in brads (64 = a quarter turn, the game's unit),
  `squash` in Q8 (256 = rest).  CLI: `show cast check|sheet|look|macro` (commands/cast.py).

## AUDIO: voice (WP9) and the vendored kit (show/audio), 2026-10-25

- **show/audio/ is the kit and the theme's voice sources, text only (364 KB).** The only edits
  make model, sample and cache paths overridable (`KIT_ASSETS`, `KIT_CACHE`, `KOKORO_MODELS`,
  `KOKORO_VOICES`, `KOKORO_SRC`, `WHISPER_MODEL`); the kit cache moved out of the repo to
  `show/build/cache/kit`. Source hashes: `show/audio/README.md`.
- **The CMU dictionary data is not in git** (3.6 MB): spec §10.7 puts the cmudict wheel in
  `vault-models`. `showtk/voice/env.py` reads `$SHOW_LIB/g2p/cmudict.dict` and, if missing,
  extracts it from the pinned PyPI wheel (`cmudict==1.1.3`, sha256 `81917843…` checked); the GPL
  wrapper is never imported. Its BSD licence is in `show/lib/licences/`.
- **voice.lock is JSON** (`show-voice-lock/1`, one row per line, readable diffs). Placement
  contract: `say <line>` on frame F puts take sample `onset` (= 1440, the 30 ms pre-roll) on
  episode sample 1600·F; `mouth` letter i is on frame F + `mouth_from` (-1) + i. Full format:
  `show/tools/showtk/voice/README.md`. Change: `showtk/voice/lock.py`.
- **Takes live in the take store** (`$SHOW_LIB/takes/<voice>/<ep>/<id>_<kind>_<sha12>.flac`,
  default the scratchpad's `show_lib/`), 24-bit FLAC, keyed by the sha256 of the samples, so
  the vault can mirror the folder as is.
- **Every take is finished the same way**: 48 kHz mono, -23 LUFS (pyloudnorm), exactly 30 ms
  before the first sound (-40 dB re peak), a 60 ms tail with a 10 ms fade.
- **Kokoro's audio runs 65 ms ahead of its token grid** (measured here on bilabial closures and
  fricative bursts, and in the theme's singer.py): every token time is corrected by -1,560
  samples at 24 kHz before the mouth track is made. AT9.2 measures closures from the audio
  itself (the 1-6 kHz band dip), not from the token times.
- **Recordings**: drop folder `show/ep101/vo/<line_id>_take<n>.wav` (also `_t<n>`; scratch
  reads `_s<n>`), gitignored; `takes.toml` `fav = <n>` picks one; with no pick the **highest take
  number wins** (the last read is usually the keeper). A recording beats TTS with no edit.
  `docs/show/ep101/RECORDING_GUIDE.md` was not published yet; BREAKDOWN §2.4's naming is accepted too.
- **Casting** (CASTING.md in the scratchpad's `show_out/voices/`): option 1 of each role is the
  spec §10.3 pick, with pitch moved where the measured native pitch missed the voice bible's
  register: Mayor +3 st (not +2: 136 Hz reads "upper-middle"), Bonejangles **bm_lewis** (not
  bm_george: it sits at 139 Hz, and pitched down and flattened for the possessed ratings Whisper
  heard "Six" as "Ziggs" and "Ten" as "Zen"; bm_lewis, 89 Hz, was heard right every time), Kevin am_onyx +4 st (86 Hz is a basso;
  the bible says "middle"), Hattie bf_isabella -3 st, Lulla af_heart -3 st with a little breath,
  Pip af_sky +4 st / formant +2 st at speed 1.25 (1.30 blurred words). Kids are adult voices
  pitched up with the formants raised; the fog's child is af_kore, never Pip's af_sky.
- **Leading "..." is not spoken** (a timing note: "come in a little late, small"); Kokoro read
  it as a stray "a". **Stage parentheticals** in walla ("(laughter)") speak a `say=` text and
  skip ASR. **Possessed lines** carry `mode=possessed` (voices.shw variant `bonejangles.possessed`).
- **Clinker's twins** are rendered in her voice and then time-warped by WORLD word by word onto
  Bonejangles' locked take (same text, same rhythm); `late=R8` stays a placement field
  (`sync.late` = 12,800 smp) for the mixer.
- **"...Hee. Hee." (lib:mayor/heehee_v1)** is the Mayor's voice saying "hee", re-sung by WORLD
  on E4 then C4, 12 f each, the second 20 f after the first; synthetic until the creator records it
  (`show voice lib ingest take.wav mayor/heehee_v1`).
- **dx_proof lines** live in `show/tests/audio/dx_proof/` (lines.shw + voice.lock): they keep
  the ep101 ids where the script has the line (`co15_perfect`, `co18_stay`, `a1_09_nana_01`) and
  `dx_nana_two` for "Two of everything, dear."
