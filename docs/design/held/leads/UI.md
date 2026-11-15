# Lead brief: stream UI

Standing brief of the session that leads the UI stream (`../PARALLEL_WAVES.md`). A new or restarted lead
reads it and follows it.

Owner, 2026-11-15: "everything must be 30 frames per second, and do declutter all menu interfaces, while
improving them as much as they can, particularly with options and other things like that". Also earlier the
same day: "declutter, and beautify as much as you can with the interfaces" (OWNER_DECISIONS.md, TALK QUESTS
AND A CLEANER, PRETTIER UI, item 2; EVERYTHING AT 30). Nobody watches this session: never stop to ask; take the
plan's default, write it into OWNER_DECISIONS.md as "default chosen by Claude (UI), owner may override", go on.

## Read first
1. CLAUDE.md; `../PARALLEL_WAVES.md` (streams, file ownership, budgets in section 3, landing in section 4 with
   steps 3b and 3c: the land loop of `tools/agents/wave-runner.js` and the `LANDING_HOLD` file).
2. `../OWNER_DECISIONS.md` (latest wins), the comprehensibility rules (CL1-CL6 in ROADMAP.md / HELD_OFF_ROADMAP),
   `../../SPEED_AUDIT3.md` (S3-13 menu headroom: about 30 menu windows at 30-33 ms; section 3.1 rank 28),
   docs/GBA.md (retained panels S2-6 / S2-8, the UI group), `../status/UI.md` (yours: keep it current).

## The plan
1. **UI-0 survey and style:** walk every menu on GBA, PS1, Dreamcast (title, pause, Options (every page),
   shops (food, gifts, HomeMart, Black Market), island hub, town hall, apartments, the citizen picker and roster,
   the quest log, news, items, unlocks, library lists, the save and load screens, pop-ups and prompts); for
   each: what is on screen, what is unclear, what is missing, the fps. Write `docs/design/UI_PLAN.md`: one
   shared style (frames, fonts, spacing, colours, icons, motion), the layout rules (one header line, the
   cursor's item described, the rest on a second page or behind a button, no more than one help line), and
   the Options redesign. Land it first.
2. **UI-OPTIONS:** Options rebuilt: grouped pages (Game, Display, Sound, Controls, Accessibility, Performance,
   Cheats), every row with a one-line description of what it does and a live preview where possible, sensible
   defaults, reset-to-default per page, the new rows of the other streams (stick mode, slider-range cheat,
   combat detail, ...) placed in the right group; the option save format stays loadable (OPT / OPT2).
3. **UI-MENUS:** the other menus in the order the survey ranks them (most-used first), each decluttered and
   restyled in the shared style, each holding a steady 30 with no frame over 33 ms (S3-13's retained-panel
   recipe: the list body and still text as a kept panel, the cursor row its hole).
4. **UI-POLISH:** calm motion (short eases), consistent sounds, the control cards, the last screens.
Every package: before / after screenshots of every changed screen, the 30 fps rule measured with the speed2
windows of those menus (nothing done below a steady 30), memory gates (GBA EWRAM share SPEED's 4 KB is not
yours: ask the coordinator for a share with numbers; prefer the scene scratch arena), tests, docs, ROADMAP.

## Files
Yours: `core/src/ui*.c`, `options*.c`, `fp_options.c`, `fp_shop.c`, `fp_title.c`, `story_questlog.c`, the
menu parts of other `fp_*.c` screens (keep those changes small, `cross-stream:` where a stream owns the file),
the language packs' UI strings (every language keeps working; new strings in English fall back as today).
Not yours: the Talk screen (TALK), the creator pages (CREATOR), the studio (STUDIO), the level select (LEVELS),
Dreamcast-only screens (DC): agree changes through status files. The SPEED-2 machine leaves S3-13 to you.

## Machine and running the stream
A fresh container: `sh tools/dev_setup.sh apt`, then in the background `JOBS=2 sh tools/dev_setup.sh psn00bsdk
psxavenc redux` and `JOBS=2 sh tools/dev_setup.sh kos flycast` (about an hour; the GBA works after apt).
Workflow tool with `tools/agents/wave-runner.js` (stream "UI"); without it the Agent tool with the same three
steps; if a permission check refuses both, stop and message the coordinator (session_01RLcGbHBa2kju7ynbaJLHT6);
never work around it. At most about five agents busy; check-in with send_later every 40 minutes while agents
run; one push per package; never push while docs/design/held/LANDING_HOLD exists without your package in it.
Never force-push, push only to claude/nice-allen-gr6czm, no pull requests.
