# EP101 "Mingle Night": continuity log

Owner: continuity supervisor (seg = continuity). Segment owners fix their own files;
continuity fixes only `show/ep101/episode.shw` (order and includes).
Checkers (shared scratch, run from the repo root; `<s>` = the session scratchpad):
- `python3 <s>/show_work/continuity/check.py .`: follows episode.shw; per-file TC, beat sums
  against the beat sheet, line-ID staging (say / walla on), duplicate shot ids.
- `python3 <s>/show_work/continuity/scenes.py .`: per-scene cast, kids, gore, risky ops, gag counts.
- `python3 <s>/show_work/continuity/sets.py .`: every `set=<set>:<variant>` exists in series/sets.
- `python3 <s>/show_work/continuity/ids.py .`: cue / SFX ids missing from series/music.shw, sfx.shw
  (last output: `<s>/show_out/continuity/missing_audio_ids.txt`).

## Frame budget (30 fps, 22:00 = 39,600 f; source: PILOT_BEATSHEET.md rundown)

As staged (DECISIONS "STAGING co_mt"): co_mt owns EC1; a2 and a3 stage EC2 and EC3 in their own
files; a4 owns the tag, end credits and post-credits button.

| file (in episode order) | seg | TC in-out | shots | frames |
|---|---|---|---|---|
| shots/co.shw | co_mt | 00:00-01:40 | 21 | 3,000 |
| shots/co_mt.shw | co_mt | 01:40-02:30 | 13 | 1,500 |
| shots/a1.shw | a1 | 02:30-07:50 | 55 | 9,600 |
| shots/ec1.shw | co_mt | 07:50-08:00 | 2 | 300 |
| shots/a2.shw (incl. EC2) | a2 | 08:00-12:25 | 57 | 7,950 |
| shots/a3.shw (incl. EC3) | a3 | 12:25-16:50 | 39 | 7,950 |
| shots/a4.shw | a4 | 16:50-22:00 | 56 | 9,300 |
| **total** | | | **243** | **39,600** |

## Line IDs (docs/show/ep101/lines.csv: 165 lines, each staged exactly once)

| seg | scenes | lines |
|---|---|---|
| co_mt | SC02, SC05, SC13 | 11 |
| a1 | SC17-SC27 | 55 |
| a2 | SC30-SC37 | 48 |
| a3 | SC40-SC46 | 18 |
| a4 | SC49-SC60 (incl. T-2, PC-1) | 33 |

Walla IDs shared across acts (`w_giggle_01..11`) are staged by the act whose scene lists them
in lines.csv, not by the first act to use the sound.

## Cross-segment watch list

- Running gags (GAG_BIBLE §6): "beep." (ends a2 on the prize-case crack), "...it's fine.",
  "Hee. Hee.", "We don't talk about the other side." (spoken only, never sung, three uses),
  the fountain giggle, Population 500 / "1", Mascotty the Starer.
- Time of day: act cards 18:00 / 21:00 / 22:00 / 23:00, eyecatch clocks 21:00 / 21:40 / 22:59,
  Tag = morning. Every lantern is dark from 2-01 ("dark since nine", script 3-01).
- Looks per DECISIONS (ART): Pip in tangerine cap + hi-vis bib, Mr. Flops = prop `gen:mr_flops`
  in Pip's paws (a bunny, not ACC_TEDDY); Bonejangles ribs bare + bow tie, possessed is a state;
  Mayor's JUDGE rosette on bow tie.
- Content limits: kids (CO kids, Pip) never in suggestive scenes/dialogue, no graphic gore, never
  within reach of the possessed (cast.shw pip note, GAG_BIBLE §8.8). The lints work per `scene=`.

## Open findings (newest pass; owner named; tick when fixed)

Severity: **B** = blocks compile or a hard-limit lint (STD-*); **H** = visible continuity error;
**L** = low / bookkeeping.

| # | sev | owner | finding | fix |
|---|---|---|---|---|
| B1 | B | co_mt | `scene=mt` covers mt01-mt13. Pip pops in in mt03, so the whole main title is a kid scene, and mt09's `tag shot gore_truth=yes` + `gore BRUTAL` trips STD-KIDS (lints.py `lint_kids`: gore_truth is in SUGGESTIVE). The header note "no child in the corridor" is not what the lint sees. | Give the pop-in its own scene: `scene=mt_popin` on mt02, `scene=mt_wall` on mt04 (mt04-mt13 then have no child). |
| B2 | B | co_mt (a2, a3) | Duplicate string ids once every strings file loads: `s_ec2out_bubble` (strings/a2.shw + strings/co_mt.shw), `s_ec3out`, `s_ec3in` (strings/a3.shw + strings/co_mt.shw). Because of this episode.shw cannot include strings/co_mt.shw yet, so no co / mt / ec1 super resolves (co.shw, co_mt.shw and ec1.shw include no strings file). | Follows decision D1 below: a2 drops `s_ec2out_bubble`, a3 drops `s_ec3out`/`s_ec3in`; then continuity includes strings/co_mt.shw. (If an owner objects to D1: co_mt deletes its three instead.) |
| B3 | B | a4 | `sign s_gate_keepout on=keepout` (a4.shw:580) is defined in no strings file; strings/a4.shw's header says it is shared, but no one else uses it. | Define it in strings/a4.shw ("KEEP OUT." / smaller "FOREVER.", per GAG_BIBLE §8.4). |
| B4 | B | a3 | a3.shw:554 has two timed statements on one line (`+234 say a3_07_bone_01 bone  +242 say a3_07_clink_01 ...`); spec §5.2 is one statement per line. | Split it, and anchor the clink like a1/a2: `@a3_07_bone_01.start+R8 say a3_07_clink_01 clinker mouth=none`. |
| B5 | B | a2, a3, a4 | STD-AGE (lints.py `lint_age`): every crowd must declare `age=`. Missing: a2.shw:146, 157, 273, 474, 554; a3.shw:33, 69, 117, 131, 140; a4.shw:373, 400, 427. The two mixed crowds with Pip in them (a2.shw:474 `a2_48`, a4.shw:427) would hide the child from the kid lint. | Add `age=adult` (demons, Mascotty); for the two mixed crowds stage Pip as an `actor` or use `age=child`. |
| H1 | H | ART, a2, a4 | Lanterns relight with no beat. 2-01 puts every lantern out (`bandstand:dark`), and the script (3-01) says "every lantern has been dark since nine", but `bandstand:torn`, `:rite` and `:night` draw them lit (park.shw: `night` = "1-12 to 4-08 (lanterns lit)"). From a2_28 every wide in a2, a3 and a4 shows them lit. Close-ups also flip: a2_04-a2_09 `bandstand_cu:north_dark`, then a2_19 and a2_34 `north_night`. | Decision D2: ART adds `lan01..lan10:out` to `torn` and `rite` (candles stay) plus a lanterns-out `night` (or a new `night_out`); a4 moves a4s15, a4s18, a4s31 off `bandstand:night`; a2 uses `north_dark` for its post-21:00 CUs, or ART makes `north_night` match. The firework in 4-08 is the first new light. |
| H2 | H | a1, a2, a4, co_mt | Mr. Flops is a prop (DECISIONS, ART review: `gen:mr_flops` in Pip's paws, not part of the look). Only a3 stages him (`prop=mr_flops` on the actor). He is missing in a1_10a, a1_10b, a1_10d; a2_19, a2_28, a2_43; a4s02, a4s07, a4s08, a4s10, a4s11, a4s25, a4s26, a4s28, a4s31, a4s32, a4s33; and mt03. The poses (`hold_flops_*`, `lean_out_flops`) will draw empty paws. | Add `prop=mr_flops` to every Pip actor (a3's form). Cosmetic: a1's pose `tap_sash` and the string `s_a1_10_sash`. Pip has no sash (cap + bib, DECISIONS), so check that nothing draws one. |
| H3 | H | a3 | a3s26 and a3s30: Pip at x=200 is 16 px from possessed Bonejangles (x=184), with no adult between them. Nana starts at 230 and walks away to 160. The script (3-06) has Pip "on the grass in front of" Nana and Fuzz, with Bonejangles up on the bandstand, so this violates the cast note and GAG_BIBLE §8.8. | Put Pip at x of about 150-165, in front of Nana and Fuzz and away from Bonejangles' x, and keep Pip there through a3s30. |
| H4 | H | a2, a3 (D1) | The eyecatch pairs don't match. EC1 IN (co_mt) puts Mascotty as `gen:mascotty_tiny(h=8)` at 40,40 `plane=back` by the banner pole. a2_57 has her as a chibi crowd at 250,70, and adds her on top of `base=a2_56`, so that is two changes (COMP-ONECHANGE). a3s39 has her as a chibi at 286,96, scale 0.25. The beat sheet wants the same depth in every pair. The bell, cue and flash also differ from pair to pair. | Decision D1. |
| L1 | L | AUDIO, a3 | `w_kids_01..06` are staged only through the walla bed `kids_far` (co.shw:34). The bed (`walla kids_far lines=w_kids_01..w_kids_06 ...`, spec §6.2) belongs in ep101/lines.shw, which is not on origin yet. a3 uses a line id as a bed (`walla w_giggle_09 on`). | AUDIO defines `kids_far` (and a bed for w_giggle_09) in lines.shw; a3 then uses the bed name. |
| L2 | L | AUDIO + each seg | 18 cue ids and 210 SFX/foley/ambience ids used in shots are not in series/music.shw or sfx.shw: co_mt 1/0, a1 3/51, a2 3/59, a3 4/44, a4 7/56 (cues/SFX). Naming also differs between segments (`FOL_PEN_SCRIBBLE` vs `FOL_pen_scratch`). The list is in missing_audio_ids.txt. | AUDIO adds them or names the existing equivalent; the segment renames. |
| L3 | L | a4, ENGINE | a4.shw:50, 52 use `say - sunny mouth="..."`: a mouth flap with no line (Sunny never speaks). This is not in spec §5.5. | ENGINE accepts `say -`, or a4 uses a pose/flipbook. |
| L4 | L | a3, a4 | a3.shw and a4.shw declare no `act` / `beat` block (co, co_mt, ec1, a1 and a2 do). Their shots still sum exactly to the beat sheet's beats. | Add the `act A3` / `act A4` blocks (beat lens = the beat sheet; 4-08 protected, 1-01 elastic is a1's). |
| L5 | L | a3/a4 | strings/a3.shw and strings/a4.shw are now included from episode.shw. | Do not also include them from the shots files (that would define them twice). |

### Decisions (continuity as integrator; also in show/DECISIONS.md)

- **D1, eyecatches**: one owner and one design for all three pairs. Use co_mt's ec2.shw/ec3.shw.
  a2 deletes a2_56, a2_57, their two `beat e101_ec2_*` lines and `s_ec2out_bubble`; a3 deletes
  a3s38, a3s39 and `s_ec3out`/`s_ec3in`. Continuity then includes `shots/ec2.shw` after a2 and
  `shots/ec3.shw` after a3, plus `strings/co_mt.shw`. Frames are unchanged (+300 -300 each).
  Until both deletions land, episode.shw keeps the a2/a3 versions, so no frames are doubled.
- **D2, lanterns**: dark from 2-01 to the end of Act Four. The candles and the 4-08 firework are
  the light. Tag = dawn.

## Passes

### Pass 2, 2026-10-25 (all five segments on origin, compiler not yet on origin)

Passing:
- Runtime is **39,600 f = 22:00:00 exactly**. Every file starts on its beat-sheet TC. All 60 beats
  are present and every beat's shots sum to the beat sheet. The exception is MT-1..MT-7: they are
  cut to the theme's bar map (DECISIONS co_mt), and the MT total is still 1,500.
- 243 shots, with no duplicate ids. Every `set=`/variant exists in series/sets.
- Lines: 159 of 165 are staged exactly once (`say` / `walla <id> on`), 0 are staged twice, and
  every staged id is in lines.csv. The 6 left are `w_kids_*`, which go through the `kids_far` bed (L1).
- Running gags:
  - The "Hee. Hee." tic is used exactly 3 times: co13, a1_08_mayor_09, a1_13_mayor_01. Nana's
    quote (a1_09_nana_06) and the flat read (a1_12_mayor_02) are not the tic.
  - "We don't talk about the other side." is used once, spoken (a1_08_mayor_08).
  - "...it's fine." appears as a caption once (a2_23). The ITS_FINE sting otherwise plays only on
    the EC INs and in the credits.
  - The fountain giggle plays only where the beat sheet asks (CO-5, 1-03 + the on-time "Hee.", 1-08).
  - The "1" chain holds: a1_02 sign, 1-10 lost-property lantern, a2_20 drift, PC-1.
  - Mascotty is still everywhere except the sanctioned paw (1-11) and thump-back (1-13).
  - Pet "beep." lines: 6 (mt7, a2_04, a2_08, a3_02, a3_05, a3_07), at the GAG_BIBLE cap. EC2's
    bubble is text only. Watch item: add no more.
- Screen direction: in mt05 and a4s37 the Hillvile lane is screen-left and Hellvile screen-right.
  The brick wipe runs L to R going in and R to L coming back.
- Gore: BRUTAL appears only in a2_teacups, a3_fair, a3_house_fp and a3_tent_fp (each asserts or
  shows no child) and in mt09 (B1). No child is possessed. Shots with no `gore=` default to CUTE
  (sidecars.py).
- Time of day: act cards 18:00 / 21:00 / 22:00 / 23:00; EC clocks 21:00 / 21:40 / 22:59 (set
  variants match); Act One runs sunset to dusk to night; Tag `cottage:dawn`.

Fixed here: episode.shw now includes strings/a3.shw and strings/a4.shw, which no file included before.
Not rendered: `show compile` is not on origin, and `show standards` / `show status` need
build/ep101/timeline.json, so these are source-level checks only.

### Pass 1, 2026-10-25 (baseline)

No segment shot lists or .shw on origin yet (only `show/ep101/cues.shw`).
- co_mt, a1, a2, a3, a4: MISSING shotlists/<seg>.md and shots/<seg>.shw (owners: each segment).
- episode.shw created as the include list in rundown order.
