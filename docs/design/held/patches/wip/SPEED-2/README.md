# SPEED-2 wip patches (PAUSE 2026-11-15 ~20:50 UTC)

Unlanded work of the second SPEED machine (lead session_01Xd63cvyt9PpfrF57wBDFeR), one folder per package,
`git format-patch` series. Apply with `git am` on a worktree (the base sha is below; rebase onto the tip, resolve).
Nothing here is gated on the current tip; every package still needs its review and the full gates. Evidence
(screenshots, tables) was in the lead's scratchpad and is gone with the machine: re-measure.

| folder | base | what it is | state |
|---|---|---|---|
| S3-TALK | 08f5b054 | Talk screen toward 30 fps on the GBA (talk_scene.c, cross-stream, portrait box / list panel / line generation spread) | built `partial`, review minor, land started (waiting on the landing hold); 30 not reached |
| S3-7 | 0d7e0d69 | fog vignette and cutscene skies (cutscene*.c, backdrop.c; new backdrop family ids 4 and 5) | built ready, review minor, land started |
| S3-CREDITS | cf5e9f27 | credits parade: lighter parade ground, ticker / head panels | built `partial`, review minor, land started |
| S3-8 | d198b776 | library lobby kept stage (family id 7), jukebox stamps, Almanac list backdrop, level select (cross-stream levels_select.c); the studio STAGE tab was left to the STUDIO stream (07ba63c restores it) | built partial, never reviewed or gated; docs not written |
| S3-15 | 08f5b054 | PS1 scene change: enter after the call (one overlay read), scene_push_late / scene_set_late | unfinished: no prefetch / keep-last-group, no measurements, no gates; the review verdict was blocking |

Numbers reported by the builders (GBA libmgba work ms, before -> after, not re-measured on the tip): S3-8 lobby 37.4 -> 21.9,
jukebox 41.3 -> 30.3, Almanac list 31.7 -> 30.0, level select idle 35.8 -> 27.6 (move still 20 fps).
Open under the owner decision EVERYTHING AT 30: every window above that is still over 33 ms.
