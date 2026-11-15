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
