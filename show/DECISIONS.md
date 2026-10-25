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
