# GBA combat emergency-detail benchmark — November 19, 2026

Measured using the **real GBA ROM** and the repository's headless mGBA harness:
`make -C gba test-3d` (`gba/harness/scripts/view3d.txt`), with the
script's seeded scene, identical 130-tick windows and 240×160 output.

| Camera / quality | Before (FPS) | After (FPS) | Difference |
| --- | ---: | ---: | ---: |
| Balanced, first-person | 18.4 | 22.6 | **+4.2** |
| Balanced, third-person | 17.8 | 22.2 | **+4.4** |
| Smooth, first-person | 18.2 | 18.2 | 0.0 |
| Smooth, third-person | 18.4 | 18.4 | 0.0 |
| Max detail, first-person | 18.7 | 18.7 | 0.0 |

**Source jobs:** baseline [GBA CI run 44658362302](https://github.com/masong0508-lab/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/actions/runs/44658362302)
and experimental [GBA CI run 44660709690](https://github.com/masong0508-lab/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/actions/runs/44660709690).
The experiment was merged directly into `main` as
[`ec3018f`](https://github.com/masong0508-lab/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/commit/ec3018f3c32ed7d07e1aaee561e6f5df537971d7).

## How it works

In GBA Balanced detail, a full-resolution render that exceeds the two-vblank
30-FPS budget switches to half resolution. If half resolution also misses it,
the next GBA viewport renders at quarter resolution and uses the **existing
in-place 2x upscaler twice**. This does not allocate a new page, heap
buffer, texture, or framebuffer. A four-byte timing estimate is added in
EWRAM. The 2D HUD remains full resolution. Smooth keeps its 2x scaling and
Max retains its prior full/half behavior.

**Visual trade-off:** the 3D world becomes more pixelated under load.
This intentionally exchanges some detail for responsiveness on GBA, not on
PS1, Dreamcast, or desktop ports.

## Remaining bottleneck

The seven scripted Hell Breach level runs are not yet at 30 FPS. Their
start-room FPS changes (baseline → experimental) are:

| Start room | Previous | Current |
| --- | ---: | ---: |
| Bone Cathedral | 4.7 | 5.3 |
| Cloud Nine Spire | 3.9 | 4.3 |
| Plushie Factory | 5.3 | 6.1 |
| Graveyard Shift | 5.2 | 6.0 |
| Sugar Rush Sewers | 6.1 | 7.0 |
| Hellvile Underbelly | 6.6 | 7.6 |
| Vape Cellar | 8.6 | 8.8 |

These are *smaller* gains; no four-FPS improvement is claimed for Breach
levels. Subsequent level-specific renderer work should use the exact
`test-breach2` windows and avoid changing the simulation or adding RAM
buffers. The standard engine-test speed tour forces full detail, so it is
**not** a substitute for these real-game measurements.
