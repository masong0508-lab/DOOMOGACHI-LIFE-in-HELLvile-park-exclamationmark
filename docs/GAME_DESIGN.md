# BOREagotchi LIFE! in HELLvile PARK... — Game Design

> GBA tamagotchi × Tomodachi Life × DOOM.
> It starts sugary-sweet and pastel, then the demons of hell invade
> wherever you are. Real GBA ROM and real PS1 disc.

## 1. The world

* **HaHaland**: the good place. A candy-coloured little land of
  cottages, cafes, flower beds and a big pastel park. Everything bounces.
* **Hellvile Park**: the giant park at the heart of HaHaland, split in
  two by a river and a big ugly wall:
  * **Hillvile**: the **good** side. Home of the **500 good citizens**
    (resident ids 0..499).
  * **Hellvile**: the **bad** side, built on top of the sealed hell gate.
    Home of the **500 bad citizens** (ids 500..999): imps, ghouls, goth
    gremlins, skeleton bureaucrats, vape demons, petty villains, and a few
    secretly lovely ones.
* The park is in the title because the ending is everyone from both sides
  finally comingling in it.

## 2. Modes

**Title screen (CL2, C8).** One line under the menu says what the
highlighted row is ("The plot, chapter by chapter.", "Run an island of
citizens.", the theme and language rows say LEFT / RIGHT). Continue names
its save ("Story: Marvin, Ch.1, Day 2" / "Island: 12 citizens, Day 3"); a
Continue with no save is greyed and says "No story save yet" (A buzzes);
New story with a save says "Starts over. Old save kept." The help line
keeps to about 22 characters at 240x160 (the room between the two waving
figures; the footer makes way while the menu is open) and about 30 at
320x240. The menu rows no longer flash an empty pill while they settle.

### Free Play (Tomodachi Life style)
New Island choices:
1. **Empty Island**, like the original games. You create every citizen
   yourself in the creator, up to 500 per side. This is where the immense
   customisation lives.
2. **Hellvile Park Classic**: move in all **1000 premade citizens** (500
   good + 500 bad), each hand-tuned with a unique look, name, job, bio,
   personality, voice and relationships.
3. **Random Island**: 1000 seeded random citizens.

You can edit or replace any premade citizen, and create new ones in either
district.

Free play loop (real-time clock, day and night, adjustable sim speed):
* **Island map** hub: Apartments (Hillvile Towers / Hellvile Pits), Cafe
  "Brimstone Bites", Shop, Church of St. Cuddles (exorcisms), Graveyard,
  Workshop (welding), Concert Hall, Beach, Hospital, Arena (unlockable),
  the Wall and the Gate, Town Hall (creator, options, save).
* **Apartments**: browse 500 homes per district by floor. Each window shows
  the citizen inside doing their activity, with a thought bubble when they
  have a **problem**.
* **Resident screen**: needs meters (hunger, happiness, hygiene, energy,
  health, sanity, corruption, social), mood, current activity,
  relationships list, likes and fears, bio. Actions: feed (favourite /
  hated food reactions), gift, clean, play, talk (voice babble +
  catchphrase), heal, comfort, introduce someone, advise on
  confessions / breakups / make-ups, edit look, exorcise (if possessed),
  **Lay to Rest** (see 2.4).
* **Social sim**: friendships, best friends, crushes (including
  cross-border Hillvile ↔ Hellvile romances), dating, proposals, marriage,
  rivals, nemeses, families, breakups, babies ("hellspawn" or "angel
  babies", which take a free slot), arguments you resolve, a news ticker.
  (LS1) The ticker's last 32 headlines are saved with the island, in the
  `LIFE` chunk: each is an 8-byte record (a template, a table index, the
  day, up to three resident ids) printed when it is read, so a renamed
  resident's old headlines show the new name; up to two free-text lines
  (a concert, a fun-fair record, a story line) are kept as text. A move-in
  no longer wipes the feed; loading greets you with "Welcome back!".
* **Boreagotchi**: a tamagotchi pet on an LCD-green screen (see 2.3).
* **Possession**: at night, corruption creeps up (vape exposure, Hellvile
  visits, nightmares, nemeses). At 100 a resident becomes **POSSESSED** by
  a named Hellvile demon: red hollow eyes, creepy portrait, haunting
  others, spreading corruption to friends.

### 2.1 Story Mode (the full arc, ~2–4 hours)
You create **yourself** (a dedicated player record) and move to HaHaland.

**Ch.1 "Welcome to HaHaland"** (extremely cutesy open world, top-down)
* Mayor Gigglesworth (a Mad-Hatter turkey, show canon) welcomes you; quest for your house key.
* Nana Pudding (baker) gives you a **Boreagotchi egg**. Hatch tutorial:
  feed, clean, play, sleep.
* Pip lost their bunny plush. Explore the park, the beach, the playground.
* Make 3 friends; bring a cupcake to someone who loves cupcakes; tiny
  errands for colourful citizens; the sealed Wall to Hellvile looms, with
  strange purple fog leaking over it.

**Ch.2 "Something in the Vapor"**
* **Cloud Nine Vapor** opens next to the Wall, run by the suspiciously
  charming **Vex**. Flavours: Brimstone Blast, Unholy Grape.
* Citizens who vape start acting weird, and corruption rises.
* First possession. **Father Fuzz** (plushie priest) teaches the exorcism
  ritual. The possessor's true name is a Hellvile citizen.
* **Rivet**, an imp who ran away from Hellvile, runs the Workshop and
  teaches **welding**. Your first weapon is your choice: cute or brutal.
* **Hell Breach 1: the Vape Cellar.** First-person / third-person shooter
  tutorial with stairs, ledges, a lift, a secret, and a mini-boss vape
  wraith.

**Ch.3 "Hell Invades"**
* Vex's giant vape cloud tears the gate open. Demons of hell invade
  **wherever you are**: the cute overworld palette rots into the infested
  version (`MAPID_HAHALAND_INVADED`), music turns metal, and encounters
  drop you into first-person/third-person combat.
* Rescue citizens, exorcise the possessed, weld better guns, clear
  infestation zones.
* **Hell Breach 2: Cloud Nine Spire.** A multi-height platforming tower
  with lava, lifts, a cloud bridge, and the boss **Lord Vapula, Duke of the
  Ninth Cloud** (three phases).

**Ch.4 "Hellvile Park"** (the ending)
* With Vapula gone you finally cross into Hellvile. Most of its 500 bad
  citizens were Vapula's prisoners, coerced into vape-gate work.
* Redeem, befriend and exorcise your way to peace. Gossip and help quests
  cross the border, and Rivet goes home.
* The Wall comes down. **Every citizen of both sides comingles in Hellvile
  Park** for one giant festival (`MAPID_PARK_UNITED`): music, fireworks,
  confetti, cross-border couples.
* Credits are a **parade of all 1000 citizens** through the united park.
  Free play then unlocks the united park and cross-border life.

### 2.2 The Shooter (first person AND third person)
Hell Breach levels, invasion encounters and the Arena. Toggle the camera
at any time (SELECT). Both modes are fully supported:
* **First person**: classic Doom feel with modern touches, including
  height-varied architecture (stairs, ledges, pits, platforms, low
  ceilings, lifts, doors), look up/down, jump, crouch.
* **Third person**: an over-the-shoulder chase camera that collides with
  walls, with shoulder swap. **Your own customised citizen** runs, rolls and
  shoots with your welded weapon visible in hand.
* Movement: accel/friction, strafing, sprint, dodge-roll, step-up,
  knockback. Aiming: vertical aim assist, soft lock-on, recoil, ADS zoom
  with scopes, hit markers, damage numbers, directional damage indicator,
  hit-stop, screen shake, muzzle-flash lighting, low-health heartbeat.
* **Demons** (cute-ish in Ch.2, grotesque in Ch.3): Impling, Vape Wraith,
  Puff Demon, Hellhound Pup, Bone Jester, Slime Belcher, Gargoyle, Cyber
  Piggy, Brute, Cloud Spider, and boss Lord Vapula. Real AI: chase, strafe,
  flank, ranged and melee, flying, infighting, pain states.
* **Gore setting**: CUTE (confetti, hearts, plush stuffing, squeaks) /
  BRUTAL (blood, gibs) / per-weapon style.

### 2.3 Boreagotchi (tamagotchi)
Egg → Baby → Child → Teen → Adult, with 16 forms across 4 lines: CUTE (good
care), ANGEL (holy), FERAL (neglect), DEMON (corrupted). Care actions:
meal / snack, play (minigame), clean poop, medicine, discipline, lights.
Care mistakes count. The adult form gives a combat buff. The screen is an
LCD-green tamagotchi panel with chunky pixels and beeps.

### 2.4 Exorcism, failure, and killing off
* **Exorcism** (Church of St. Cuddles, Father Fuzz). Three phases:
  1. Prayer rhythm: button prompts slide into a heart-shaped hit zone.
  2. Holy water: aim and throw at the demon darting around the host.
  3. True name: choose the possessor's name (a real Hellvile citizen)
     from clues about their personality and job.
  Items (holy water, sage, plush cross, rosary, blessed salt, hymn book)
  and the host's sanity raise the odds.
* Each failure adds to the citizen's fail count. After **3 failures** the
  soul is **lost** (`RES_SOUL_LOST`), and only then can you choose **Lay to
  Rest**. They go to the graveyard, and their friends grieve.
* **Monster Mode** (Options, double confirmation "I AM A MONSTER") allows
  killing off anyone at any time. It's tracked by a town-wide *monster
  meter*, citizens remember it, and the graveyard marks it.
* **Graveyard**: plots, flowers, a memorial. A late-game rite can raise a
  citizen as a **ghost** (`RES_GHOST`).

### 2.5 Welding
At Rivet's Workshop you weld **frame + barrel + core + mod + sight + stock
+ magazine + underbarrel + charm + 2 paints + CUTE/BRUTAL style**.
* Frames cover all kinds of guns (pistol, revolver, dual pistols, machine
  pistol, SMG, assault rifle, battle rifle, bullpup, LMG, minigun, DMR,
  sniper, railgun, crossbow, bow, pump / super / auto / lever shotguns,
  grenade and rocket launchers, flamethrower, plasma rifle, nailgun,
  BFG-class cannon, chainsaw, katana, sledgehammer, fists) **and dedicated
  cutesy weapons** (bubble wand, plush launcher, confetti cannon, lollipop
  hammer, heart bow, squeaky mallet, glitter sprayer, water pistol, sticker
  gun, yarn crossbow).
* The welding minigame (keep the bead in the zone, heat management) sets
  quality, which scales the stats. Every combination gets a generated name
  and a first-person sprite. Weapons level up with kills.

### 2.6 Unlockables & options
An **immense** amount: creator parts, paints, weapon parts, shop items,
pet forms, modes (Arena, Boss Rush, Hellvile visits, Tamagotchi-only),
cheats (big-head, confetti everything, cute demons, brutal residents,
low-grav, paper mode), gallery, jukebox, window skins, crosshairs, title
themes. Most creator options start unlocked, so the creator is huge from
minute one. The rest are earned through play, with unlock news (a toast
that names the kind of thing: "New Creator outfit: Cozy Scarf", "New trophy:
Dice Roller"; a burst is told once as "5 new unlocks! See Unlocks", see 5.1)
and an Unlocks gallery that shows how to get each one.
About 45 data-driven options cover game, shooter, sim, audio, display and
accessibility.

## 3. The 1000 premade citizens
* **500 good (Hillvile)**: bakers, nurses, kids, grannies, goths who are
  actually sweet, cherubs, plushies, frogling mail carriers, robot
  crossing guards, ghost librarians... Mostly cute species, pastel outfits,
  kind-to-quirky personalities.
* **500 bad (Hellvile)**: imp accountants, ghoul landlords, skeleton
  bureaucrats, hellhound bouncers, cacoblob bookies, vape lords, edgy
  teens, cultists, and a few secretly lovely ones (Chapter 4 redemption).
  Demonic species, dark or edgy outfits, blunt / unhinged personalities,
  higher corruption, but still **cute-drawn**.
* Every one is unique: name, job, bio, look built with the full option
  set, personality, voice, likes, fear, catchphrase, birthday, and
  pre-seeded families, couples, rivals, and cross-border crushes.

## 4. Controls

| Action | GBA | PS1 |
|---|---|---|
| Confirm / fire | A | Cross / R1 |
| Back / sprint-jump | B | Circle |
| Menu / pause | START | START |
| Camera FP/TP | SELECT | SELECT |
| Strafe / lock-on | L / R | L1 / R2-lock, Square roll, Triangle weapon |
| Look up/down | hold L + D-pad (classic) | L2/R2 or D-pad (modern) |

### 4.1 UX budgets: presses per journey

Every common journey has a press budget, measured by the journey meter
(`make -C core journeys`, part of `make -C core test`): the scripts in
`core/tests/journeys/` play each journey with the fewest presses the
game allows today, at 240x160 and 320x240, and fail when a journey takes
more presses or logic ticks than its row in
`core/tests/journeys/budgets.tsv`, or a different number of presses at the
two sizes. A package that shortens a journey lowers its row in the same
commit; nobody raises a row without saying why. Depth is how many scenes
or modals deep the journey went, screens how often the top scene changed;
typed letters (names in a search) are counted apart from presses.

Two limits of the meter. Depth and screens count the scene stack only: a
menu a scene draws inside itself (the story overworld's pause menu, the
island's location and START menus) is depth 0 and no screen change, so J6
reads depth 0 although it opens the pause menu, and a package that flattens
such a menu shows it in presses, not depth. The ticks column is the
script's own waits plus 2 per press, a ceiling on how long the script
waits rather than a measure of the game's speed (a slower transition fails
the script's `expect-scene` instead); a package that adds a wait to a
journey updates its ticks. Setup commands that move the game without a
press (`scene`, `push`, `money`, `item`, `problem` and the like) are
refused between `journey-begin` and `journey-end`; put a journey's setup
before it begins.

| Journey | Presses | Depth | Target (owner) |
|---|---|---|---|
| J1 boot -> title menu | 2 | 0 | (U5/U6) |
| J2 boot -> story continued | 3 | 0 | 3 (U6, Continue first: done) |
| J3 title -> music volume -1 in Options -> title | 9 | 1 | 5 (U2) |
| J4 combat pause -> music volume -1 -> back in the fight | 4 | 0 | 4 (U3a: done, the pause shell's Music row) |
| J5 title -> language to Spanish -> title | 5 | 1 | (U5) |
| J6 story overworld: save from the pause menu, walk on | 2 | 0 | 3 (U3b quick save: done, START twice) |
| J7 island: save from the START menu, back to the hub | 2 | 1 | 3 (U3c quick save: done, START twice) |
| J8 story overworld -> title screen | 5 | 1 | (U3b) |
| J9 boot -> a new Classic island's hub | 7 | 1 | 10 (U6c: met; LS6: 9 -> 7, A builds, START names) |
| new island: New Island screen -> hub | 3 | 1 | (U6/LS6: A builds at once; START asks for a name first) |
| help the next citizen with a problem (item in the bag) | 2 | 3 | 2 (LS-QA: B = next problem, done) |
| a problem that needs an item not in the bag: find, shop, help | 2 | 3 | 3 (LS-QA: buy on the spot, done) |
| feed a citizen found by name | 6 + name | 3 | (LS-QA) |
| introduce two citizens found by name | 13 + 2 names | 4 | (LS-QA) |
| collect today's rent | 0 | 0 | 0 (LS-QA: automatic, done) |
| save the island | 2 (= J7) | 1 | 3 (U3c: done) |
| open the news | 1 | 1 | |
| open the Boreagotchi | 1 | 1 | |
| add a household member | reserved | | 6 (F2) |
| care for the whole household | reserved | | 1 (F2) |

Frame rates and the latency of each life-sim screen's opening press are
measured on the consoles by `make -C gba test-life` (docs/GBA.md "Life
tour") and `ps1/harness/scripts/perf_life.txt` (docs/PS1.md).

### 4.2 First-time tips and the Field Guide (CL4: clarity C4 + C12)

**Tip cards** (`core/src/tips.c`, contract `doomo/tips.h`). The first time
the player reaches a place that needs explaining, a card pops up at the
bottom of the screen: a speaker chip and name, one line in that character's
voice, and one plain instruction line on its own band (what to press). The
three speakers are Mayor Giggs (the island, Town Hall, apartments, care,
the Boreagotchi, the Studio), Rivet (the house, Hold the Door, the shooter,
the workshop) and Father Fuzz (exorcism, family, the Library). The voice
line follows docs/design/story/SCRIPT_STYLE.md (abrupt indie-parody comedy,
the odd training-tape joke); the plain line never jokes. The shooter's
plain line follows the control scheme (Modern, Classic, Dual stick).

| Tip | Where it shows (first visit) |
|---|---|
| island | the free play hub: the whole button map |
| town_hall | the Town Hall menu: the two sides, creating, rent |
| apartments | Browse homes: window icons, floors, SELECT help |
| care | a citizen's Care tab: what each action fills |
| house | your own house, Live or Build: modes and the START menu |
| defend | Hold the Door: repair / wind / calm / plate (the night waits) |
| shooter | the story fight, a Hell Breach level or the Arena intro (the fight waits) |
| exorcism | the ritual's tool picker: tools, START, the three phases |
| pet | the Boreagotchi LCD |
| studio | the Music Studio tabs |
| workshop | Rivet's workshop: parts and the stat names |
| family | the Family screen |
| library | the Grand Library lobby and its Field Guide |

The card is **non-blocking**: the scene under it keeps running and
animating (where a fight or the Hold the Door night would start, it waits
instead), only the presses go to the card. It ignores presses for its
first 14 ticks (the pop-in), and the first press after that closes it; no
press made while it is up reaches the scene, so it is never a menu action.
Toasts wait behind it. It belongs to the scene that asked for it: hidden
while another scene is pushed over it, dropped if that scene goes away
(then it shows again next time unless it was up for 2 seconds). Once it
has boinged in it holds still as a retained panel (G7; it borrows the
toast's panel id, no toast being drawn meanwhile), so the GBA copies it
instead of drawing its text again, and the island hub keeps its frames
with the card on them (`tip_steady` / `tip_hold`, as for a steady toast).
The card sits clear of the bottom 14 rows (its drop shadow included), so a
scene's hint pill and the island's news strip, which repaints itself on
kept frames, never cross it. Measured on the GBA (libmgba, `make -C gba
test-tips`): the hub draws 20.5 fps with the card up and 24.6 fps after it
closes (10.5 fps when it kept no frame under a card), for the few seconds
a first-time card is on screen.

**Seen flags** (one bit per tip, 32 at most) are the machine's, not the
island's: they survive New Game. Where the platform keeps the machine
config record (PS1 memory card file, Dreamcast VMU file) they are bytes
12..15 of it and the Tips switch is byte 11 (`doomo/options_store.h`); where
it does not (native, GBA) they ride in the save's 'OPT2' chunk (version 2: the seen
word, then the option values) and merge on load (a load never forgets a
seen tip). A PS1 card write takes about a second, so a seen tip is not
written when its card closes: the record follows at the next save
(`save_game` -> `opt_config_flush`) or option change. Options > Game has
**Tips: On / Off** and **Show all tips again** (a confirm, then every tip
shows once more).

**Budgets.** Scripted runs never see a card: the native runner, the GBA
harness, the PS1 harness (in lockstep) and the Dreamcast harness (dcrun's
`test_flags` mailbox word) report `PLAT_TEST_NO_TIPS`
(`doomo/plat_ext.h`), so every journey in 4.1 keeps its presses. A script
that wants the cards says `tips on` (runner, GBA harness, dcrun);
`expect-tip NAME` / `expect-tip none` and `tips reset` check and reset them
(core/tests/scripts/clarity_tips.txt plays every card at both sizes).

**The Field Guide** (`core/src/lib_help.c`, SCENE_LIB_HELP) is the
glossary behind the tips: an eighth book on the Grand Library's shelf (not
a wing: no completion and no unlock ids), a "Field Guide" shortcut in the
free play Library menu, and the **Help** row of the island START menu
(above Back to title) and of the story pause panel (above Title Screen).
17 pages: Buttons (with a GBA line, SELECT+A / SELECT+B, and a PS1 line naming
Cross, Circle, Square and Triangle), H$ and rent,
Needs, Moods, Dark, Bonds, Two sides, Exorcism, Weld stats, The house,
House score, Hold the Door, Boreagotchi, Family, Play as, Studio words,
Tips. The list works like every Library book (A opens a page, LEFT / RIGHT
turn pages, UP / DOWN scroll, B closes); `HELP_ARG_PAGE(n)` opens one page
directly. Each page is up to four paragraphs, each its own English literal
under 250 bytes, so every paragraph is one row for the translators
(docs/LANGUAGES.md); a language without the row shows English.

## 5. Look & feel
**Premium, not cheap pixel art: cute, lively, constantly animated, and
vaguely furry.** Most citizens are soft plush critter-folk (cat, fox,
bunny, bear, pup, mouse, sheep, raccoon and owl folk alongside imps,
ghosts, skeletons and slimes), with fluffy fur tufts, big expressive ears,
fluffy tails and paw hands. Everything breathes, blinks, wiggles and
bounces. Shading is soft multi-tone with coloured outlines and rim light.
See ARCHITECTURE.md "Art direction" for the rules.

### 5.1 Toasts, hints and the HUD band (clarity CL1)
Comprehension comes first: a toast never hides what the player needs to
read. The rules (`core/include/doomo/ui.h`, `core/src/ui.c`):

* **Lanes.** A screen with a HUD band says where its toasts go. In the story
  overworld the band is the objective pill (top left) and the clock (top
  right); toasts slide in just under it and the region banner sits under it
  too (it rises from below, so it never crosses the goal). Under the pause
  panel toasts keep the top row. On the Free Play hub map toasts slide in
  under the day / clock / purse row (so "Rent collected" never covers the
  money it changes); over a place's menu they keep the top row. Every other
  screen keeps the top row.
* **Kinds and order.** SYSTEM (saves, autosaves and their errors: the story
  pause's save, the inn, Free Play's autosave) > FEEDBACK (what you just did:
  bought, moved in, rent) > HINT (how to use this screen) > UNLOCK (news). A
  newer toast of a higher kind goes ahead of the waiting ones, never in front
  of the one on screen. A hint waits until the dialogue box is closed.
* **Scene changes.** A new screen (scene_set) drops every toast but SYSTEM
  ones; pushing or popping a full screen drops the old screen's hints (so the
  hub's "A Visit a place" never scrolls over the shop or the title). A toast
  posted on the tick of the change belongs to the new screen.
* **Unlock news.** Unlocks are not toasted one by one. News waits until
  nothing else is queued and no dialogue is up, and a burst is told once:
  one unlock says what it is ("New paint: Lemon"), more say "N new unlocks!
  See Unlocks". The kind comes from `unlock_kind_word()` (`unlocks.c`): the
  category, and for the Secrets lists (`UL_CAT_MISC`, one list per owner) a
  word per list: Fishdex entry, Bestiary entry, recipe, fishing rod, Studio
  sound / drum kit / tune, choir layout, concert stage, gold medal, badge,
  title screen, Hell Breach, arena mode, Creator set; every other Secrets
  entry is a trophy. A name that already starts with its kind is shown
  without it ("New Fishdex entry: Plush Koi", not "Fishdex: Plush Koi").
  The line is written when it is told, whole (every unlock's line fits the
  55-character toast; the longest is 42). The first news of a session
  gathers for 20 s from its first unlock (a new island's opening minute is
  one digest), and after news has been told the next gathers for 40 s.
* **The story's first controls hint** ("A Talk  B Run  START Menu  SEL Map")
  is shown in the bottom prompt slot for 4 s once the region banner has gone,
  not as a toast.

Checks: `core/tests/test_toasts.c`, the runner's `hud-watch` (fails if a
toast is ever drawn over the story HUD band), `expect-toast`, `toasts` and
`expect-toasts` commands, and the scripts `clarity_ow_hud.txt` and
`clarity_toasts.txt`.

## 6. Tone
Cute first. The horror and gore are funny *because* the first hour is a
sugary tamagotchi daydream. Nothing mean-spirited toward real people;
violence is cartoon game violence against demons. Killing citizens is
gated behind failed exorcisms or the explicit Monster Mode.
