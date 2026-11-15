# STUDIO-30 findings (running file, 2026-11-15)

Baseline on the tip 2c948ea5 (GBA, libmgba, `make -C gba test-speed2 SPEED2="studio studio_play"`):
stage tab 22.5 fps (36.5 ms avg / 37.0 max, 65 frames over 33 ms); every other window 30.0 fps; templates
29.1 / 35.1 ms, picker 23.4 / 33.1, play loop 25.2 / 33.0, drum loop 26.9 / 34.4.

## STAGE tab (GBA)
Experiments (work avg ms): no chibis 30.5; no marquee bulbs 29.6. So the 16 chibis cost 6 ms and the 56 bulb
circles (112 rect calls) 4.2 ms.
1. Poster chibis as hardware sprites (the show's option C planner, backwards from the front; the button and the
   show count are the only later bitmap parts): 36.5 -> 33.8 ms (12 of 16 singers are sprites; the edge ones
   touch the button).
2. Marquee bulbs as 32x5 pictures (4 bulbs, 2 phases, ROM), 7 plat_image calls a row: 33.8 -> 31.3 ms avg / 33.0
   max, 30.0 fps. Exact (test studio_bulbs_equal_the_circles); sprites exact (studio_poster_sprites_equal_the_bitmap).

## New windows: gba/harness/scripts/speed2_studio_tabs.txt (every tab / mode the other two groups lack)
Baseline (tip): song idle 31.0 / 32.3, song playing 31.6 / 35.8, notes idle 27.4, notes playing 28.8, order 24.6,
lyrics 31.5 / 33.1, choir 28.8, perform early 20.5, perform later 21.3, start card 21.0, control card 13.5,
made song playing 28.5 fps (32.5 / 38.6, 10 frames over 33 ms), file top 30.4, file end 29.7.

## SONG tab (GBA)
The record is 7 circles, ~70 rect calls: 5.5 ms. Its still part is now one baked picture (st_bake, the studio's
pool): song idle 31.0 -> 26.9 ms, made song playing 32.5 -> 28.5 ms, 30.0 fps. Screenshots of every
studio_tabs window byte-identical to the tip's (cmp).

## ST-EASY gap: a take over a composed song cannot be undone
Cause: the whole-song before-image (892 B raw) RLE-encodes to over the 768 B journal for a dense song. Fix: the raw
before-image goes in StEasy.before (scene scratch, BakeSt's pixel pool shrank by 892 B: 9,796 B left; no new
.bss) and the journal holds one 6 byte record (BIG_OFF) that names it. Test
studio_take_over_a_composed_song_undoes_and_redoes (fails without the fix).
