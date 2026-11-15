# CONCERT-30 findings (running file, 2026-11-15)

Machine: libmgba harness, `gba/harness/scripts/concert_profile.txt`, `--worklog`, tip 2c948ea5 + SPEED-3's s4-c.mbox.
Drawn fps / work avg ms / max ms.

| window | baseline (tip + s4-c) | after CLITE_LEAD |
|---|---|---|
| solo (Sugar Bowl) | 30.0 / 29.1 / 32.3 | 30.0 / 27.0 / 30.2 |
| band | 20.8 / 38.1 / 41.8 | 23.2 / 36.0 / 39.5 |
| choir 12 | 29.9 / 30.3 / 34.7 | 29.9 / 30.3 / 35.4 (unchanged: the lead is not drawn by this path) |
| setup | 30.0 / 7.5 / 9.0 | 30.0 / 7.5 / 9.0 |

## What each change bought
- s4-c.mbox applied (silhouette loops, exact): in the baseline above.
- Experiment, not kept: holding the lead's figure to one still pose (`portrait_hold(1)`) saves only 1.3 ms
  (38.1 -> 36.8): the "cache thrash" of her phases is a small part of the band's cost.
- CLITE_LEAD (exact): the lead's figure and shadow as one baked image (49x81 in the choir's mini buffers, made again
  when her image uid changes): -2.1 ms on the band and the solo. Native has a soft shadow (colormap) unlike the GBA's
  black one, so under CLITE_LEAD the fallback draws the GBA's shadow spelled out (silhouette + image) and the test
  compares baked against that.

## Left (band needs another ~3-6 ms, max 39.5 -> <33)
Stage 10-12.5 ms, crowd 9 ms (back silhouettes 3.6, front chibis 3.4), band members 6.4 ms (instruments as kept
pictures -3 ms), karaoke + HUD 2 ms. The Dreamland stage is 3-5 ms over the others: not profiled yet.
