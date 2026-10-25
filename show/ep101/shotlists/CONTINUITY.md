# EP101 "Mingle Night": continuity log

Owner: continuity supervisor (seg = continuity). Segment owners fix their own files;
continuity fixes only the include order in `show/ep101/episode.shw`.
Checker: `python3 <scratchpad>/show_work/continuity/check.py <repo>` (frame sums, line-ID coverage).

## Frame budget (30 fps, 22:00 = 39,600 f; source: PILOT_BEATSHEET.md rundown)

Assumption (DECISIONS default): each segment owns the eyecatch pair that follows it; a4 owns
the tag, end credits and post-credits button.

| seg | covers | TC | frames |
|---|---|---|---|
| co_mt | cold open + main title | 00:00-02:30 | 4,500 |
| a1 | act one + eyecatch pair 1 | 02:30-08:00 | 9,900 |
| a2 | act two + eyecatch pair 2 | 08:00-12:25 | 7,950 |
| a3 | act three + eyecatch pair 3 | 12:25-16:50 | 7,950 |
| a4 | act four + tag + credits + post-credits | 16:50-22:00 | 9,300 |
| | **total** | | **39,600** |

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
- Time of day: act cards 18:00 / 21:00 / 22:00 / 23:00, Tag = morning.
- Looks per DECISIONS (ART): Pip in tangerine cap + hi-vis bib; Mr. Flops = white teddy;
  Bonejangles ribs bare + bow tie, possessed is a state; Mayor's JUDGE rosette on bow tie.
- Content limits: kids (CO kids, Pip) never in suggestive scenes/dialogue, no graphic gore on kids.

## Passes

### Pass 1, 2026-10-25 (baseline)

No segment shot lists or .shw on origin yet (only `show/ep101/cues.shw`).
- co_mt, a1, a2, a3, a4: MISSING shotlists/<seg>.md and shots/<seg>.shw (owners: each segment).
- episode.shw created as the include list in rundown order.
