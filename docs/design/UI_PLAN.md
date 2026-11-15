# UI plan: one style, decluttered menus, a better Options screen

Stream UI (lead brief `held/leads/UI.md`, status `held/status/UI.md`). Owner, 2026-11-15: "everything must be
30 frames per second, and do declutter all menu interfaces, while improving them as much as they can,
particularly with options and other things like that."

This file is UI-0: the survey, the one shared style, the layout rules and the Options redesign. Later packages
(UI-OPTIONS, UI-MENUS, UI-POLISH) follow it. Numbers marked "to measure" are filled in by the first package
that touches the screen, with the `tools/speed2.py` windows (each package reports before and after).

## 1. Survey (what is on screen today)

Sources: the code (`core/src/fp_*.c`, `ui*.c`), `SPEED_AUDIT3.md` (S3-13 list), `docs/GBA.md`. The screenshot
walk of all three ports starts when the machine setup is done; each package adds its before / after shots.

| Screen | Now | Unclear or missing | fps (S3-13 / audit 3) |
|---|---|---|---|
| **Options** (`fp_options.c`) | 10 tabs (Quick, Game, Controls, Shooter, Life Sim, Audio, Display, Performance, Accessibility, Cheats), only 3 tabs visible at 240x160 and L/R cycles through all 10; 7 rows visible; one help pill of up to 3 lines; Game page has 12+ rows (Language, Gore, Monster, Difficulty, Text speed, Autosave, Blood, Confetti, Tips, Tips reset, Mature, Notes, Memorial) | the page you are on and the pages that exist are hidden; a page of 12+ rows scrolls with no count; Game is a catch-all; Shooter mixes aim, HUD and gore; Accessibility holds Screen shake and UI bounce but not Subtitles-adjacent rows in one place; no preview of a value; reset only per page with no per-row default; the help pill eats a third of the 160 px screen | 30-33 ms rows (to measure) |
| Pause menu (`pause_menu_draw`) | one window, one button per row, "PAUSED" title, hint line | fine; music row shows a number label | pause 28-30 (S3-13) |
| Title (`fp_title.c`) | animated title, buttons, theme | button count differs by state | to measure |
| Shops: food, gifts, HomeMart, Black Market (`fp_shop.c`) | list left, detail right, tabs, hint | too many numbers per row (price, stock, owned, effect); help and detail overlap on GBA | shop 32.1, Black Market 32.4 (house 33.2), HomeMart cursor 31.6 |
| Island hub, town hall, apartments | button lists plus a header and a hint | town hall 9 buttons on one page; apartments list shows every field | town hall menu 9 frames, Black Market 4 (S3-13) |
| Citizen picker and roster | portrait grid or list, side panel | roster side panel repeats the list line | roster 31.0, resident 31 |
| Quest log (`story_questlog.c`) | list plus detail | long titles run past the window | to measure |
| News | three tabs | third tab slow | news 32.2-32.6 |
| Items, Unlocks, Almanac | scrolling lists with a detail pane | detail pane always on, long lists with no position count | items 30.4, Unlocks 30.9, Almanac 32.4 |
| Library lists, studio lists, cafe | lists | follow the list rules | cafe 30.7, SONG 31.3-32.0, LYRICS 32.1, CHOIR 30.8, swatches 32.0 |
| Save and load | slot list | slot facts in one line each | to measure |
| Pop-ups, prompts, toasts (`ui_modal.c`, `ui_prompt.c`) | confirm, text entry, toast | confirm texts are long sentences | modal hold already retained (S2-2) |

Out of scope here (other streams): Talk (TALK), creator pages (CREATOR), studio (STUDIO), level select
(LEVELS), Dreamcast-only screens (DC).

## 2. The shared style

* **Frame:** `ui_window` in the player's window skin (`WIN_CUTE`); `WIN_HELL` only for Cheats and after the
  invasion. One outer window per screen, no window inside a window (use a thin rule line).
* **Header:** one line at the top: the screen name, centred (`ui_title`). Page tabs, when a screen has pages,
  are one row under it and always show **all** pages as short names or icons (see Options).
* **Body:** rows 11 px high at 240x160, 13-14 px at 320x240 (`fp_big()`); 6 px side margin; label left, value
  right; the selected row has the cursor and a lighter fill. No more than 7 rows visible at 240x160 (8 at
  320x240); a list longer than that shows "n / total" at the right end of the header.
* **Help:** at most **one** line under the list: what the selected row does, one sentence, at most 40 letters
  wide per 100 px (the existing wrap check `fp_options_help_overflow` becomes a one-line check). More detail
  goes behind a button (A on a "?" row or SELECT) as a second page, never on the main page.
* **Hint bar:** one bar at the bottom (`ui_hint`), at most three actions: `A Choose  L/R Page  B Back`.
  The row type changes the words; nothing else is printed there.
* **Colours:** only `ui_style` colours: text, dim text, accent (gold for the new, red for danger), fill,
  outline. No raw palette indices outside `ui*.c`.
* **Icons:** the existing `STR_ICON_*` set; one icon per page tab, one per row type (toggle, slider, list,
  action). No decorative icons in rows.
* **Motion:** short eases only (4-6 ticks): a window pops in, the cursor slides one row, a value nudges one
  pixel. Nothing loops except the cursor wave. Reduced by Accessibility's UI bounce (and off at 0).
* **Sound:** cursor move `SFX_CURSOR`, change `SFX_BLIP` for sliders / `SFX_CURSOR` for lists, choose
  `SFX_SELECT`, back `SFX_CANCEL`, error `SFX_ERROR`, reset `SFX_SPARKLE`. One sound per press.

## 3. The 30 fps rule for menus

A menu window is done when its steady frames and its cursor / page-change frames all stay at or under 33 ms on
the GBA (the slowest port), measured with the speed2 window of that screen. The method is S3-13's retained
panel: the list body and still text are drawn once into a kept panel, the cursor row is a hole re-drawn each
frame, so a cursor move or a value change costs a row, not a page (`ui_modal_hold`, S2-6 / S2-8). Rules that
keep this cheap: no text wrap in the draw (wrap once when the row changes), no `snprintf` per row per frame
(format when the value changes), at most one shaded rectangle per screen, backdrops held while a menu is up.
Memory: no new resident bytes (GBA EWRAM and PS1 resident share is 0 until the coordinator gives one); the
panel and any scratch live in the scene scratch arena.

## 4. The Options redesign

### 4.1 Pages (7, down from 10)

| Page | Short name | Holds (rows) |
|---|---|---|
| **Game** | Game | Language, Difficulty, Gore, Text speed, Autosave, Tips, Mature, Content notes, Memorial names, Monster Mode, Combat feel, Loss, Pet alerts, News ticker |
| **Display** | Display | Window skin, Title theme, Cursor, HUD layout, Font style, LCD filter, Camera filter, Clock, Weather, Crosshair + colour, Field of view, Damage numbers, Hit markers, Blood colour, Confetti |
| **Sound** | Sound | Music, Sound, Voice volume (slider rows with a live preview note), Voice babble, Subtitles |
| **Controls** | Controls | Control scheme, Turn buttons, Stick mode (Dreamcast one-stick), Invert look, Look speed, Aim assist, Lock-on, Camera, Shoulder, Rumble |
| **Access** | Access | Screen shake, Head bob, Reduce flashes, Colourblind aid, UI bounce, Font style (shared link), Subtitles (shared link) |
| **Perf** | Perf | Combat detail, Effects detail, Draw distance (PS1 3D), Show FPS, Demon density |
| **Cheats** | Cheats | the 12 cheats and Slider range (locked ones show "???" and their hint as today) |

Quick stays as the front **Quick** page (Language, volumes, Text speed, Difficulty, Gore, Preset), so the
tab strip is Quick plus the seven above = 8 short names (4-5 letters each, icons at 240x160). The Sim page
(Possessions, Town events, Sim speed, Day length) joins **Game** under a "Life" sub-group header row; the
Shooter page's rows are spread over Display (looks), Controls (aim) and Perf (Demon density) as listed.
Row ids and the option store do not change (OPT / OPT2 stay loadable): only `.page` in the info tables and the
row order move; `doomo_option_page_names` and the page enum change in step (add-only is kept for saved page
indices: the page is never saved).

### 4.2 Row behaviour

* **Group header rows** (not selectable) split long pages: "Look", "Feel", "Life". A page has at most 3
  groups and at most 12 rows in all.
* **One-line help** per row (the "Help" rule above): rewrite the 70 help strings to one line each (about 40
  letters); the long explanations (Controls, Mature, Combat feel) move to a **"?" detail card** opened with
  SELECT or Y, one screen of up to 6 lines, B closes.
* **Live preview where cheap:** Window skin, Cursor, Font style and Clock repaint at once (they already do);
  volume sliders play a blip at the new level; Crosshair and Field of view show a small swatch / frame in the
  help area instead of text; Text speed types one sample line; the HUD layout shows a 3-line mock.
* **Defaults:** every row's default is shown dimmed next to its value when it differs ("Normal  (default
  Plushie)" is too long: a small dot marks a changed row). **Y** on a row resets that row; the last row of the
  page resets the page (as now, kept; never touches Language, Monster Mode, Mature, Tips reset).
* **Presets** stay on Quick (Default, Comfort, Classic DOOM, Cozy life, Hardcore, Max accessibility) and
  gain a one-line list of what they change on the detail card.
* **Changed-since-opening marker** and a "Back" that never needs confirming: every change is live and saved
  at the next autosave / on leaving, as now.
* **New rows of other streams** go in the group named above (stick mode on Controls, slider range on Cheats,
  combat detail on Perf, notes / memorial on Game); a stream that adds a row writes its page in its status
  file and the UI stream places it.

### 4.3 Tabs and navigation

* All page names visible in one strip: at 240x160 icon + 4-letter name for the selected page, icons only for
  the others; at 320x240 the full short names. L/R changes page, up/down moves rows, left/right/A changes the
  value, B leaves. Page memory: Options opens on the page it was left on (not saved).
* The page's row count shows at the header's right end ("3 / 11").

## 5. Package order and gates

1. **UI-0** this file (landed first), status and wave-table lines.
2. **UI-OPTIONS** the Options rebuild of section 4, tests (`core/tests` option-page census: every option has
   exactly one page, every page has at most 12 rows, every help string one line, every default loadable),
   screenshots, speed2 windows, GBA / PS1 / DC builds.
3. **UI-MENUS** in the order of use: pause, shops (food, gifts, HomeMart, Black Market), items, roster /
   picker, news, Unlocks, quest log, town hall / apartments / island hub, save / load, title, the rest.
4. **UI-POLISH** motion, sounds, control cards, last screens.

Every package: before / after screenshots, the speed2 numbers, `make -C core -j8 test`, `check-gen`, GBA / PS1
/ DC builds where the toolchains exist, docs and `ROADMAP.md`.
