# Hell Breach expansion: 9 main-game levels + 15 New Game+ levels (plan)

Planned 2026-11-08. A planning document only: no game code, map or art has been written for it. Building
joins the work order **after** the current waves (clarity, speed, Dreamcast, creator, Talk) unless the
owner reorders (DECISIONS, "NEW LEVELS", 2026-11-08).

**Owner decisions this plan follows** (DECISIONS.md, latest entries win):

* 9 new 3D (Hell Breach) levels for the main game and 15 for New Game+. **PS1 and Dreamcast get all 24;
  the GBA gets 7 of them in total** (section 7 picks them).
* **Steady 30 fps on every screen and platform** (FPS TARGET, 2026-11-08). Where 30 is not reached, a
  package lands what helps, reports the measured number and the gap, and is never called done at a
  lower number.
* The **combat detail option** (Smooth / Balanced / Max detail, adaptive governor) applies in every level;
  each level says which effects the governor may drop first (section 8.7).
* **Comprehensibility first**: every level opens on the control card (CL6, `combat_card.c`) with one
  plain rule line for its mechanic, and teaches the mechanic in a safe room before it matters.
* **PG-21 adult comedy register** (SCRIPT_STYLE.md section 1.1): raunch and innuendo only where only adults
  (or demons) are in the scene; swearing stays on the game's sparse budget (no new f-words: the game's
  two are spent in the story).
* **Gore only on adults and demons**; the cute filter around kids; **no harm ever shown to a child**.
  No real-world hate. Self-harm and suicide are never a mechanic or a joke (none of the 24 levels uses
  them as a topic).
* **Tone**: the game's own indie-game / indie-art parody humour first (DOOM x tamagotchi x Tomodachi
  Life, cute-vs-brutal, sight gags, game-y jokes); The Walten Files is a parody target and never a
  mechanic (no cipher, no frame decoding, no lore to assemble; SCRIPT_STYLE 1.3); Poochee & Pansy and
  Princess Robot Bubblegum set the shape and the register.
* **Dark levels** (owner, 2026-11-08): about 3 of the 9 main levels and about 6 of the 15 NG+ levels are
  noticeably darker in mood and topic, each with a content note before it starts (section 6). One of
  the NG+ dark levels is the arc's **cold open** (NG+ level 1), so New Game+ says at once that it is
  heavier; the other five sit in the late-middle to the end.
* **NG+ story** (owner, 2026-11-08): a complete, bombastic arc with acts, its own antagonist and stakes,
  payoffs for the cast and the co-op heroes, set pieces in every level, a finale that tops the main
  game's, growing more mature (consequences, loss, moral choices, the town's sins coming due) from the
  late-middle on (section 4).

---------------------------------------------------------------------------------------------------

## 0. In short

* **24 levels.** Main game: **Hell Breach 8-16**, unlocked along chapters 2-4 and the post-game, each
  optional to the locked story flow (entered from a place's Activities row or a side mission, so
  `flow.lock` and the `.dsc` main beats do not move). New Game+: **"Arrears", Notices 1-15**, a
  15-level campaign with its own antagonist, **Duchess Arrears of Hell's Bureau of Unfelt Things**, who
  comes to collect on the debt that paid for the main game's happy ending.
* **24 different mechanics**, one per level (table in 2.4); only three NG+ levels use a remixed rule
  (pacifist, ghost-only, one-hit), each on a new map.
* **Dark levels (9):** main HB9 *The Undercroft*, HB11 *The Glum Pipes*, HB12 *The Wall*; NG+ Notice 1
  *Day One* (cold open), Notice 8 *The Debtors' Prison*, 9 *The Artist's Studio*, 10 *The Ward of
  Forgotten Names*, 11 *The Hollow Mingle*, 13 *The Vault of Unfelt Things*.
* **GBA gets 7:** HB8 *Steam Room Panic*, HB9 *The Undercroft*, HB13 *Boneyard Buffet & Bandstand*, HB15
  *The Department of Contracts*, Notice 1 *Day One*, Notice 7 *The Audit*, Notice 14 *The Ledger*. They are
  the compact, roofed, short-sightline levels with the cheapest mechanics, and they carry the NG+ arc's
  three load-bearing beats (cold open, midpoint, finale). The other NG+ beats reach the GBA as story
  recap cards with the same choices, so the arc and its endings stay complete there.
* **Tech first** (LV0-LV11: records, map format v2, a small level-script VM, three mechanic kits, enemy
  variants and AI, art and audio, per-platform level packs, the detail governor's level hooks, the test
  harness, content notes), then **the 9 main levels in three waves** (LV12-LV21), then **NG+ story and the
  15 levels in five waves** (LV22-LV38).
* **Every level is reachable and tested:** mapc + `test_maps` BFS (extended to the new traversal), the
  autopilot clears it in every mode, a runner script shoots it at 240x160 and 320x240, the GBA harness
  runs the 7 GBA levels, the PS1 harness runs all 24, the Dreamcast harness runs all 24 once DC6 lands,
  and fps is measured per detail mode (section 8.10).
* **Owner questions** are in section 10 (NG+ Chapter 5 and the use of the retired ending slots E03 /
  E05 / E07, W15, memorial names in Notice 13, the set cap).

---------------------------------------------------------------------------------------------------

## 1. What exists today, and what it constrains

Read for this plan: `docs/LEVELS.md`, `core/data/maps/` (README and the 8 shooter maps), `tools/mapc.py`,
`core/src/levels*.c`, `levels_int.h`, `combat*.c`, `actor.c`, `combat_bot.c`, `combat_card.c`,
`core/include/doomo/{map,actor,weld,textures,sprites,unlock_ids,save_ext,story,ending}.h`,
`docs/STORY.md`, `docs/design/story/story_final.md` (Ch1-4, epilogue, post-game, NG+ "HaHaland
Remembers"), `SCRIPT_STYLE.md`, `docs/COOP_HEROES.md`, `docs/CREATOR.md` + the part catalog's sets,
`docs/GBA.md` (raycaster, EWRAM ledger, "3D: Faster / Better"), `docs/PS1.md` (RAM map, overlays, packed
maps, GTE renderer), `docs/DREAMCAST.md` (DC6 status), `ROADMAP.md`, the week-2 speed audit and creator
plan.

| Area | Today | What it means for 24 more levels |
|---|---|---|
| Levels | 7 (`LV_COUNT`): Vape Cellar, Cloud Nine Spire (story), Sewers, Plushie Factory, Graveyard Shift, Bone Cathedral, Underbelly; 7 mini-boss variants (`LB_*`, 6 `LBF_*` mechanic bits in a `uint8_t`) | the tables become 31 levels / 31 bosses; boss flags grow to 16 bits; level text moves out of resident code on the PS1 |
| Records | ranks 3 bits x 7 + best Horde wave in the one saved counter `PROG_BREACH_RANKS` (static assert `LV_COUNT * 3 <= 21`); `PROG_*` has 32 slots, all claimed | new records need their own save-ext chunk (`BRCH`, section 8.5); the old 7 stay where they are |
| Unlocks | 1024 bits, all ranges assigned; the levels range 864..895 has 891..895 spare | trophies and golds of the new levels live in `BRCH`; creator rewards go through CR11's set unlocks (bank 2), weld rewards through the action range's spare 551..563, songs through the story spare 726..735 |
| Maps | ASCII + mapc, at most 64 x 64, 255 things; the live copy `doomo_map` is a fixed 32 KB (GBA EWRAM, PS1 BSS); `DoomoCell` 8 B with all 8 flag bits used; `DoomoThing.flags` all 8 bits used; plain-cell tags 240..243 are conveyors | a map costs no new RAM (the live copy is fixed), but new cell behaviour cannot take another flag bit: map format v2 adds zones, paths, swap sets and a script in ROM side tables (section 8.1) |
| Textures / skies | 48 textures (+ `TEX_NONE`) under a 64-id cap (`textures_fit`), 5 skies | 15 shared ids left; per-level texture banks remap a few ids per map (section 8.6) |
| Things / demons | 10 demon types + Lord Vapula, contiguous (`IS_DEMON` range), 8-frame sprites; `ACTOR_MAX` 128 (shared with shots and effects) | most new enemies are **variants** (scale, tint, palette, glow, AI flags) of existing demons, as the mini-bosses already are; 4 new base types get art |
| AI | 8 personalities (`AI_*`), the autopilot BFS (`combat_bot.c`, 4 KB work area borrowed from the raycaster on the GBA) | new behaviours (freeze, split, steal, sleep, stalk, flee-light, swim, orbit) and new BFS edges for the new traversal |
| GBA speed | combat FP / TP **9.8 drawn fps, 16 logic ticks/s** (week-2 audit); "30 drawn fps in GBA combat is not reachable exactly" without G-C1 / G-C2 (G9); the speed wave (SP) is landing X5, G-C1, G-C2 and the combat detail option | GBA levels wait for those; the 7 GBA levels are chosen and built to the cheapest budgets (section 7.3) |
| GBA RAM / ROM | EWRAM about 3.1 KB free (ledger row 28); IWRAM slack ~96 B; ROM 4.5 MB of a 32 MB cartridge | per-level state lives in one small union; everything else is ROM; ROM is not a constraint |
| PS1 | ~14 KB above the 64 KB stack reserve; scene overlay slot ~116-127 KB, combat + levels is one of the biggest groups; the 12 maps' cells are LZ-packed resident (14 KB); textures, skies and sprites LZ-packed resident and decoded into VRAM caches; combat FP 29.8 / TP 28.1 fps (Redux) | the 24 new levels cannot add resident data: **each level is a pack on the disc** (`BREACH.BIN`), streamed at level start (section 8.8) |
| Dreamcast | 2D layer, controls, sound and the overworld in 3D are done; `plat_view3d` (DC6) is still a stub | DC level runs wait for DC6; RAM and disc are not constraints |
| Story | Ch2-4 use Breach 1-5 and 7 (Ep 2.7, Ep 3.3 nests, Ep 3.6, Ep 4.5); the flow is locked (`flow.lock`); NG+ exists as a restart (`story_ng_plus_arm`, `SF_NGPLUS`, the run count, seen endings carried) with no content of its own; the 15 endings (`ENDING_COUNT` 15) have three retired slots, E03 / E05 / E07 | main levels hook in as optional side content; the NG+ arc needs story-engine work (section 8.9, owner question 1) |
| Control cards | every Breach level, mode and the arena open on a card: goal, mode rule, the controls of the chosen scheme, the HUD legend | each new level adds one mechanic rule line; dark levels add the content note |
| Tests | `test_levels.c`, `test_maps.c` (BFS start -> exit), runner scripts `levels_*` at both sizes, PS1 `fast_*` scripts, GBA speed tour | extended per level (section 8.10) |

---------------------------------------------------------------------------------------------------

## 2. Rules every new level follows

### 2.1 Tone and content

1. **The game's own jokes first.** Each level has one parody angle (spa day, carnival scam, office sim,
   tenement sitcom, repo-reality TV, legal drama, department-store sale...), sight gags and game-y jokes.
   The quiet-part-out-loud line is one tool, rationed (SCRIPT_STYLE 1.1).
2. **The Walten Files** is mocked in HB13 (*Boneyard Buffet & Bandstand*, the in-world chain the owner
   named) and played straight once, in NG+ Notice 9 (owner, 2026-11-08: "the rot under the cute surface
   played straight"). Never a mechanic in either: nothing to decode; tapes are collectibles that are
   jokes (HB13) or plain, sad footage (Notice 9). No real names, characters, locations or creators.
3. **Register.** PG-21: adult innuendo only where only adults or demons are on screen (Grumbleguts, the
   Duchess, towel gags on adult demons, the office party). Mature content off: towels become bathrobes,
   censor-steam gags become plain steam, innuendo lines take their clean alternates (the story VM's
   mature switch). Swearing: none added.
4. **Kids.** Child characters (Pip, Wisp, Bolt, Lulla's kids, any `KID`) appear only in safe spaces
   (behind glass, at home, at the festival), never as targets, hostages in danger or victims. The cute
   filter applies to any shot near them (the existing gore rule). The NG+ antagonist's Bureau "does not
   hold minors' accounts. Policy." (a deadpan line that is also the rule).
5. **Gore** on demons and adult enemies only; body horror only on demons and demon machinery.
6. **Self-harm and suicide** are not a topic in any level. Where a story touches death (Notice 9's
   artist, Notice 13's memorial), the text states the death plainly and without self-harm (the artist
   died of old age at the easel).
7. **Dark levels** trade most jokes for mood (lighting, palette, quiet music, environmental storytelling)
   and keep a few bleak or deadpan laughs so it stays the same game (section 6).

### 2.2 Comprehensibility (clarity rules C4, C10, C11)

* The control card shows the goal, the mode rule and **one plain rule line for the mechanic** (written per
  level below as *Card line*; at most two lines at 240x160, checked by the CL5 text-fit sweep).
* The mechanic is met first in a **safe room** (no demons, or demons behind glass) with a world-space
  prompt ("A: turn the valve").
* The goal arrow (CL6 C7) points at the next objective inside a level (the next valve, key or exit);
  objectives that have several targets show "(arrow, 0/4)".
* Locked levels in the select show greyed with the reason ("After the beach nest (Chapter 3)").
* A dark level's card has a **content note** (section 6.3).

### 2.3 Size, structure, budgets (all platforms)

* Maps 30 x 30 to 64 x 64 cells; at most 200 things; at most 60 demons per level (awake at once: 10 on
  PS1 / DC Balanced, 8 on the GBA, the existing portal cap is 10 alive); a mini-boss or boss at the end
  that opens the exit with its kill event (existing pattern).
* Each level starts cute and turns hellish (`combat_s.hell`) unless the level says otherwise (the dark
  levels start dark).
* 2-3 secrets (existing `secret` cells / things), a themed **collectible set** of 5-13 (new
  `THING_COLLECT`, saved per level), rank by time / kills / secrets as today (S >= 90, A >= 70, B >= 45),
  plus **three feats** per level (saved bits): one of No damage / Pacifist / a level-specific one.
* **Pacifist** where the level offers it means: no demon killed by your shots; the boss is beaten by the
  level's mechanic. The existing pacifist story flag (`USTORY_PACIFIST`) is not touched.

### 2.4 One mechanic per level (no two the same)

| # | Level | Mechanic | Kind |
|---:|---|---|---|
| 1 | HB8 Steam Room Panic | steam fog zones cleared by valves; steam jets that throw you | environment |
| 2 | HB9 The Undercroft | a carried lantern with finite oil; candles you light stay lit and keep shades out | light |
| 3 | HB10 Fright Night at the Fun Fair | moving ride platforms on cycles (carousel ring, ferris-wheel gondolas, swing ride) | movement |
| 4 | HB11 The Glum Pipes | a rising sludge tide; relief valves drain sections | environment |
| 5 | HB12 The Wall | flip between the Wall's two faces (two cell layers); the other face is out of phase | environment |
| 6 | HB13 Boneyard Buffet & Bandstand | the showtime light cycle: animatronics freeze and cannot be hurt while the lights are on | rule |
| 7 | HB14 Hell Pits Highrise | laundry-line ziplines between towers | movement |
| 8 | HB15 The Department of Contracts | stamps open doors; take-a-number queues; stamping a demon files it out | rule |
| 9 | HB16 The Ninth Cloud | bounce pads and low-gravity zones | movement |
| 10 | N1 Day One | two-handed carry: haul bricks (no weapon while carrying) and lay them to bridge or seal | movement |
| 11 | N2 The Dead Letter Office | a pneumatic-tube network with junction levers | movement |
| 12 | N3 Repo Night | weapon repossession: tow hooks take your gun to impound cages | rule |
| 13 | N4 The Interest Engine | doubling enemies: Interest splits if left alive | rule |
| 14 | N5 Lake of Fire Regatta | a raft you steer on lava currents | vehicle |
| 15 | N6 Everything Must Go | buy-back choices with your own H$ (you cannot afford everything) | choice |
| 16 | N7 The Audit | **remix: pacifist**: shots are "Objection!" stuns; debts cannot be killed | remix |
| 17 | N8 The Debtors' Prison | stealth: noise meter, sleeping wardens, quiet weapons | rule |
| 18 | N9 The Artist's Studio | **remix: ghost-only**: no body; possess stunned demons to use their attacks | remix |
| 19 | N10 The Ward of Forgotten Names | rooms you do not name dissolve; name tags restore them | environment |
| 20 | N11 The Hollow Mingle | escort the Mayor or Mother Nox across the dead festival | companion |
| 21 | N12 The Interest | **remix: one-hit** chase from an unkillable pursuer over a collapsing floor | remix |
| 22 | N13 The Vault of Unfelt Things | stand still and "feel" to open vault doors while demons close in | rule |
| 23 | N14 The Ledger | page-flip arenas: each page brings back one Arrears rule; the last page is a defend-the-jar stand | finale |
| 24 | N15 Both Are Better | non-combat: light the sky lanterns of the first real Mingle in eleven years | epilogue |

### 2.5 Numbering and ids

* Main levels **HB8-HB16** are `LV_*` 7..15 (select page 2, "Hell Breach 8-16"); NG+ **Notice 1-15** are
  `LV_*` 16..30 (select page 3, "Arrears", shown once a save has `SF_NGPLUS` or has finished the arc).
* Maps are appended `MAPID_*` ids; bosses append `LB_*` 7..30; the select's three pages are tabs (L / R
  keep switching the mode and SELECT the demo, as today; UP from the top row of cards moves onto the
  page tabs, LEFT / RIGHT there picks the page).

---------------------------------------------------------------------------------------------------

## 3. The 9 main-game levels (Hell Breach 8-16)

### 3.0 Overview: where they sit

| HB | Level | Opens (story) | Entered from | Ties to | Dark | GBA | Size (cells) | Par / gold |
|---|---|---|---|---|:-:|:-:|---|---|
| 8 | Steam Room Panic | Ch2, after Ep 2.6 (weapon chosen) | the Bath House, Activities (night) | Steamy, Ep 2.4-2.5 | | yes | 40 x 34 | 3:20 / 2:10 |
| 9 | The Undercroft | Ch3, after "Fuzz Goes Dark" (beach nest, Breach 4) | the Church of St. Cuddles, Activities (after 20:00) | Father Fuzz, Lady Smog's file, the Huggins dummies | **dark** | yes | 34 x 34 | 4:00 / 2:45 |
| 10 | Fright Night at the Fun Fair | Ch3, after the park nest (Whack-an-Imp) | the Fun Fair gate, Activities (night) | Squeaker, the Fun Fair minigames | | | 52 x 52 | 4:30 / 3:00 |
| 11 | The Glum Pipes | Ch3, after Ep 3.4 "Under the Fountain" | the cracked fountain (plaza) | the Mayor, Rivet (Anvil's mark), Dr. Bloom | **dark** | | 30 x 56 | 3:50 / 2:40 |
| 12 | The Wall | Ch4, after Ep 4.1 "The Gate" | the gatehouse, Activities | Tallyworn, Mascotty and Thumpkin, Signhere's contract | **dark** | | 64 x 20 | 4:10 / 2:50 |
| 13 | Boneyard Buffet & Bandstand | Ch4, after Ep 4.3 (side mission "Dance Routine 4") | the restaurant's door (HaHaland, a "historic site") | Bonejangles (W15 thread) | | yes | 40 x 36 | 3:40 / 2:30 |
| 14 | Hell Pits Highrise | Ch4, after Ep 4.4 "Wisp's Room" | Hell Pits, Activities | Graveley, Wisp's family, the landlord | | | 36 x 36, 100 steps tall | 4:20 / 2:55 |
| 15 | The Department of Contracts | Ch4, after Ep 4.5 (the Pump room; Signhere) | the Underbelly market, Activities | Signhere | | yes | 40 x 40 | 3:30 / 2:20 |
| 16 | The Ninth Cloud | post-game (after the credits) | the jar on your shelf: "Sleep and visit" | Fluffy / Vapula, Dream D2, the Mayor | | | 56 x 56, 80 steps tall | 4:00 / 2:40 |

All nine are optional to the main plot (no main beat waits for them), replayable from the level select
once played, and listed in the Arena's "Hell Breach Levels" activity. Three of them (HB9, HB11, HB12) are
the main game's dark levels; they sit where the story itself is darkest (Ch3's long red night into the
Ch4 gate). Rule lines are the *Card line* of each level.

---------------------------------------------------------------------------------------------------

### HB8. Steam Room Panic

* **Story place / unlock.** Chapter 2, after Ep 2.5 (Steamy's rite, saved or lost) and Ep 2.6 (Rivet's
  weapon). Steamy (saved) or her ghost (lost: "It's nice, actually. Cold out.") gives the side mission
  "The Pipes Are Singing": the vapor that came out of her got into the boilers. The Bath House gets an
  Activities row at night. Designed as the gentlest of the 24 (it may be a player's first Breach).
* **Theme and pitch.** HaHaland's bath house the night after the first possession has become a sauna of
  the damned: a **spa-day parody** with soothing pan-pipe music over shotgun blasts, a laminated "Spa
  Etiquette" sign ("No running. No screaming. No being dragged into the cold plunge."), the "Mixed
  Bathing Fridays" banner edited to "Mixed Bathing FRY-days", cucumber slices on every demon's eyes, and
  a complaints box that is full. PG-21, adults and demons only: lounging demons in towels, and a censor
  cloud of steam that always drifts exactly where it is needed (the mosaic gag of DECISIONS' nudity rule;
  Mature off: bathrobes). The kids' splash hour sign reads "CANCELLED (demons)".
* **Layout.** 40 x 34, floors -4 (pools) to +16 (roof tank). Lobby (ticket booth, rubber-duck display) ->
  changing rooms (a locker maze; locker 13 is a secret door) -> the Great Bath (sunken pools at -3, a
  balcony at +12 around the hall) -> the boiler basement (valve wheels, scalding water) -> the sauna
  (low ceiling, the thickest steam) -> a steam-jet "ladder" up to the rooftop cold plunge (an 8 x 8 open
  roof under the night sky, inside the GBA's sky budget), the boss's hot tub.

```
  (1 char = about 2 cells; N up)
  ####################
  #######RRRR#TANK s2#   R  the rooftop cold plunge: open sky, 8 x 8 cells
  #######R~B~R###^####   B  the Baron's hot tub
  #######RR^RR#FFFFFF#   ^  steam jets: each throws you up a level
  #######|#####F v4 F#   F  the sauna: densest fog, ceiling 10
  #bbbbbbbbb###FF|FFF#   b  balcony (fh 12) round the Great Bath
  #b~~~~~~~b#####|####   ~  pools (fh -3): scalding until their valve is turned
  #b~GREAT~b#BOILERS #   v  valve wheels v1..v4 (switch cells)
  #b~BATH ~|# v1  v2 #
  #b~~~v3~~b#  ~~~~  #
  #bbbb|bbbb#####|####
  #LOCKERS #13#STAFF #   13 locker 13 -> staff room (secret 1)
  ####|######s1##|####
  #  LOBBY   ducks  S#   S  start
  ####################
```

* **Enemies.** Variants of existing demons (no new art): **Towel Implings** (impling, towel palette),
  **Steam Puffs** (puff demon x0.8, translucent, hide in fog), **Bubble Bathers** (slime belcher; its
  globs are soap bubbles that slow), **Wet Dogs** (hellhound pup; shakes water: a short slow ring).
* **Mechanic: steam fog and valves.** Fog zones cut how far you see (the zone's light falls off over a
  few cells; demons in it are drawn dark). Turning a valve (A at a wheel) vents its zone over 2 s and
  turns scalding pools (lava flag, hurts) into warm water (water flag, slows). Steam jets fire every few
  seconds and throw anyone standing on them: one is the way up to the roof.
  *Card line:* "Turn valves (A) to clear the steam. Don't stand in hot water."
* **Boss. BARON VON BATHROBE**, "Lord of the Hot Tub, Undefeated in Lounging" (brute x1.6, robe tint).
  Rings of steam vents erupt round him every 6 s (jump them); he sits in the tub to heal unless you have
  vented it. Pacifist route: vent the four tub valves and the cold shock routs him ("COLD! COLD! I'm
  leaving. I'm leaving a REVIEW."). Taunt "This is a PRIVATE soak." Death toast "The Baron fizzes away
  like a bath bomb."
* **Secrets / collectibles.** Secrets: the staff room behind locker 13 ("13: do not open, it's fine"),
  the roof water tank (from a jet), the lost-and-found (secret door). Collectibles: **12 rubber ducks**
  (each squeaks; the twelfth has horns).
* **Rank goals.** Par 3:20, gold 2:10. Feats: Pacifist (vent the Baron), No damage, All 12 ducks.
* **Rewards.** First clear: H$ as today. Creator: new set **Bath House** (reward set: towel turban,
  shower cap, cucumber eye mask, bathrobe, flip-flops, rubber-duck charm); weld charm "Rubber Duck"
  (squeaks on reload); furniture "Rubber duck shelf".
* **Music.** New **SONG_SPA_BOSSA** (pan pipes, bossa) on the cute side; the hell blend switches to
  SONG_INVASION with the pan pipes left in.
* **Life sim / Talk.** News "The bath house reopens; first bath free"; a one-day town event (every
  resident's Hygiene +20); Steamy +friendship; Mixed Bathing Fridays opens a week early. Talk (PS1 / DC):
  topic *bath house* (residents rate the water; Grumbleguts asks about the towel policy).
* **Platforms.** All, **GBA included**. GBA: fog is darker cells in the zone plus the level's short view
  distance (no renderer change, no steam billboards); at most 2 jet puffs visible; ducks are 8x8 sprites.
  PS1 / DC add drifting steam billboards that the detail governor drops first.

---------------------------------------------------------------------------------------------------

### HB9. The Undercroft (dark level)

* **Story place / unlock.** Chapter 3, after "Fuzz Goes Dark" (the beach nest, Breach 4) and its button
  (the bell rung at 20:00 for the kids' bedtime). The church's undercroft door is left open; the
  Activities row appears after 20:00. Father Fuzz is the thread.
* **Theme and pitch.** Under the church, Fuzz keeps a room nobody visits: every exorcism that failed,
  filed, each with a candle that went out. After the conveyor ("What am I FOR?") the candles are all out,
  and something that feeds on the dark has moved in. You go down with one lantern. It is quiet; **guilt,
  failure and faith that tore are played straight.** The jokes are few and deadpan: the Huggins memorial
  shelf ("Mr. Huggins the 4th. Fell over. Was buckwheat."), the filing system ("FAILURES, A-Z. Mostly H.
  For 'hug'."). The rest is Fuzz's tidy handwriting on files stamped SOUL LOST, the cot where he slept the
  night after Lady Smog, the hymnal he put down very carefully. The names on the newest files are
  residents of your own town whose souls were lost (adults only, from the life sim).
* **Layout.** 34 x 34, almost all roofed (ceilings 10-14), a spiral down from 0 to -24. The trapdoor
  stair -> the Filing Hall (long shelves) -> the Huggins niche and Fuzz's cell -> the flooded ossuary
  (water, oil jars) -> the Chapel of Failures at the bottom: eight candles round a dry font.

```
  #################
  #S stair ########   S  the church trapdoor (fh 0), spiral to -8
  #| ##FILING HALL#   shelves of files, ceiling 10
  #|  |||||||||| #
  #####|####|#####
  #HUGGINS #FUZZ's#   s1 the Huggins niche; s2 Fuzz's cell (the cot)
  #NICHE s1#CELL s2
  ####|######|#####
  #~~~OSSUARY~~~~~#   water at -12; oil jars refill the lantern
  #~~ c ~~~~~ c ~~#   c  candles (lit ones stay lit)
  ######|##########
  #  c CHAPEL OF c#   the Chapel of Failures (fh -24):
  # c  FAILURES  c#   8 candles round the font, THE UNHUGGABLE
  #  c    B     c #
  #   c   E    c  #   E  the stair up into the bell tower
  #################
```

* **Enemies.** **Shades** (vape wraith, near-black palette; a new AI flag *flee-light*: never enters a
  lit cell, strikes from the dark), **Hush Implings** (impling; no fireballs, only grabs), **Mourners**
  (gargoyles asleep on pedestals, `ambush`), **Choir Bones** (bone jester, adult skeletons in ruffs).
* **Mechanic: lantern and candles.** Ambient light is 1. Your lantern lights the cells within 2 of you;
  its oil lasts about 60 s and refills at oil jars. A on a candle lights it for good: its 3 x 3 cells stay
  lit, shades will not enter them, and they mark your way back. Muzzle flashes light the room for a
  moment (the existing flash light).
  *Card line:* "Your lantern keeps shades away. Light candles (A); they stay lit."
* **Boss. THE UNHUGGABLE**, "Every Hug That Didn't Take" (a brute-sized vape wraith variant with four
  too-long arms: body horror on a demon). Each phase it snuffs the candles near it; light hurts it.
  Pacifist route: relight all eight candles of the chapel and it comes apart in the light. Taunt "Go
  on. Hold me." Death toast "It unclenches. For a second it looks like someone."
* **Secrets / collectibles.** Secrets: the Huggins niche, Fuzz's cell (a line on the cot: "Day 1 after.
  I hugged the wrong way."), and a sealed drawer marked "SEALED. DO NOT. -F" (it stays sealed; NG+
  Notice 9 opens it: cryptic seasoning, never a puzzle). Collectibles: **7 prayer cards** (one kind,
  sad sentence of Fuzz's each).
* **Rank goals.** Par 4:00, gold 2:45. Feats: Pacifist (light the eight), No damage, The lantern never
  goes out.
* **Rewards.** Creator: the existing reward set **Exorcist Kit** opens here (garlic necklace, holy-water
  bottle, cute stole). Furniture: "Prayer candle" (counts as a candle for the house's LIT stamp: candles
  count double). Exorcism: +1 to every later rite's odds (Fuzz's prayer cards in your bag).
* **Music.** None while exploring (drips, the lantern's hiss, distant bells); SONG_THIRTEEN_BELLS very
  low near the chapel; the boss gets SONG_EXORCISM slowed.
* **Life sim / Talk.** Fuzz +friendship and a new line set; news "Father Fuzz rang the bell on time
  today". Talk (PS1 / DC): topic *failure* and *the undercroft* with Fuzz (gentle; the safety rules of
  TALK.md apply).
* **Content note** (section 6.3) on its control card.
* **Platforms.** All, **GBA included**. GBA: the lantern relights at most 25 cells in the live map's light
  bytes (the base light is the ambient or a lit candle's, so nothing has to be stored); at most 6 shades
  awake. PS1 / DC: smooth per-vertex fall-off of the lantern (PS1 corner light) and flickering candle
  billboards.

---------------------------------------------------------------------------------------------------

### HB10. Fright Night at the Fun Fair

* **Story place / unlock.** Chapter 3, after the park nest (the story clears it with Whack-an-Imp: "Do
  NOT bonk Father Fuzz."). The Fun Fair gate gets "Fright Night" at night: the rides are still possessed
  after the nest was cleared, and Squeaker the clown wants his fairground back (he honks; it helps).
* **Theme and pitch.** The possessed fairground of Ep 3.3, made playable: the ferris wheel turns
  backwards, the teacups are full of eyes, the carousel horses have opinions. A **carnival-scam
  parody**: "EVERYONE'S A WINNER" (the prize is a demon), ride signs "You must be THIS damned to ride",
  a guess-your-weight booth that guesses your sins, and a haunted-house ride that is somehow the least
  scary thing in the park ("Boo." "...That's it?"). An indie joke: a "hidden object" booth whose hidden
  objects are demons.
* **Layout.** 52 x 52 under the night sky, 0 to +48. The gate and ticket booth -> the midway (stalls,
  ring toss) -> the teacups -> the carousel (the centre) -> the ferris wheel (north, a frame of tall
  blocks with eight gondolas) -> the haunted-house ride (a roofed interior loop) -> the Big Top (roofed
  arena, the exit).

```
  ##########################
  #  FERRIS WHEEL   o  #####   o  gondolas: 8 moving platforms on a cycle, fh 0..48
  #   o  o  o  o  o  #BIG #   BIG TOP: roofed, the exit E
  #  o            o  #TOP E
  # ....midway.......#  |##
  # tt   (CAROUSEL)  ##|###   tt teacups (eyes)
  # tt   ( ring  B ) #HAUNT#  ( ) the carousel ring: platforms that turn
  #      (         )  HOUSE   HAUNT: the haunted-house ride (secret 2)
  # swings  @@@@     ######   @  swing ride (platforms swinging out over the midway)
  # ring-toss s3  stalls   #
  #S  GATE      tickets    #   S  start
  ##########################
```

* **Enemies.** **Painted Ponies** (hellhound pups that charge round the ring), **Cotton Candy
  Belchers** (pink slime belchers; sticky globs slow you), **Teacup Eyes** (small puff demons, swarm),
  **Barkers** (cyber piggies on the stall roofs, firing prize darts).
* **Mechanic: rides on cycles.** The carousel ring, the gondolas and the swing seats are moving platforms
  (lift-like cells that follow a path on a timed cycle). Ride them to reach heights and secrets; demons
  ride them too. The operator booths stop or start each ride.
  *Card line:* "Ride the rides to climb. Booths (A) stop them."
* **Boss. THE CAROUSEL KING**, "Round and Round and Round" (cloud spider x2.2 riding the carousel). He
  circles on the ring and fires outward; stop the ring at the operator booth and he is stranded and open.
  Taunt "Every ride ends where it started, kid." Death toast "The carousel winds down on one flat note."
* **Secrets / collectibles.** Secrets: the top gondola, the haunted house's "staff only", the prize
  cupboard behind the ring toss. Collectibles: **10 prize tickets** (cash them at the Fun Fair for a
  plush).
* **Rank goals.** Par 4:30, gold 3:00. Feats: Ride every ride, No damage, Beat the King without stopping
  the carousel.
* **Rewards.** Creator: Fun Fair set (foam finger, prize ribbon pin, cotton-candy prop). Minigames: a
  "Fright Night" variant of a Fun Fair game. Furniture: "Carousel horse".
* **Music.** SONG_FESTIVAL detuned as a calliope; hell blend: SONG_INVASION with the calliope on top.
* **Life sim / Talk.** News "The teacups are just teacups again"; the Fun Fair's prize stock refills;
  Talk topic *fun fair*.
* **Platforms.** PS1 and Dreamcast. Not the GBA: open sky over a 52-cell park, many moving platforms and
  billboards at once.

---------------------------------------------------------------------------------------------------

### HB11. The Glum Pipes (dark level)

* **Story place / unlock.** Chapter 3, after Ep 3.4 "Under the Fountain" (the midpoint twist: the pipes,
  Anvil's mark, the Mayor's plumbing order, Dr. Bloom says it out loud). The cracked fountain becomes a
  way down ("Follow the pipes"). The Mayor, Rivet and Dr. Bloom are the thread.
* **Theme and pitch.** You follow the plumbing that carried ten years of HaHaland's swallowed feelings
  under the Wall. Down here the glums pool: every frown, unsent apology and cried-into pillow, pressed
  into purple sludge. The pipe walls are papered with what people did not say, in their own handwriting
  ("I miss her." "I'm not okay." "Sorry about the fence."), drawn from the residents of your town. The
  sound is a low breathing. **Grief and the town's deal with Hell, played straight.** The only jokes are
  the plumbing's own deadpan: inspection tags ("Last inspected: never"), a valve labelled "GLUMS OUT" in
  the Mayor's handwriting, a tiny stamp "(it's fine.)" on every joint.
* **Layout.** 30 x 56, a long narrow descent from 0 to -40. The fountain shaft (lifts) -> the junction
  hall (pipes as blocks) -> the overflow basin (the tide) -> the pressure chamber (gauges, a crawl pipe)
  -> the Wall culvert at the bottom, where a grate lets in light from the other side.

```
  ###############
  #S fountain   #   S  under the cracked fountain (fh 0)
  #|  shaft     #   lifts down to -16
  #|##########|##
  # JUNCTION  s1#   s1 the plumber's office (the order form "Keep giggles IN")
  # pipes=blocks #
  ###|#######|###
  #~~~~~~~~~~~~~#   ~  the OVERFLOW basin: the sludge tide rises here
  #~ o ~ o ~ o ~#   o  stepping stones and pipe tops
  #~~~~ r1 ~~~~~#   r  relief valves r1..r3
  #~ o ~~~ o ~~~#
  ######|########
  #PRESSURE  r2 #   gauges; a crawl pipe (secret 2)
  #CHAMBER  ====#
  ####|##########
  #~~~~~~r3~~~~~#   the Wall culvert, the lowest point (fh -40)
  #~~~ B ~~~~~~~#   B  THE SWALLOWED SCREAM
  #  grate  E   #   E  the grate under the Wall: daylight from the other side
  ###############
```

* **Enemies.** **Sorrows** (purple slime belchers that rise out of the sludge), **Sighs** (small vape
  wraiths that drift and burst into a slow field), **Grumblings** (implings), **Swallows** (brutes with a
  scream in the belly: hit it and it lets out a stunning shriek).
* **Mechanic: the glum tide.** While you are in a basin the sludge rises one step every 6 s (a tide zone
  whose cells' floors and the sludge flag move together); it slows and hurts and hides Sorrows. Relief
  valves (A) drain their basin. It keeps you moving forward and up.
  *Card line:* "The sludge rises while you linger. Valves (A) drain it."
* **Boss. THE SWALLOWED SCREAM** (puff demon x2.4, ghostly). It does nothing until you hit it; then each
  phase it screams: the music cuts to silence, a ring shockwave, a screen shake (reduced with Reduce
  flashes). In its last phase the scream comes out in the Mayor's voice: "Hee. Hee." Death toast "It
  finally lets it out. It's quieter than you'd think."
* **Secrets / collectibles.** Secrets: the plumber's office, the crawl pipe, and the oldest joint, where
  a child's crayon note says "fluffy come home" (the Mayor's, thirty-one years ago; cryptic seasoning,
  left hanging). Collectibles: **8 unsent letters**, each one resident's unsaid line.
* **Rank goals.** Par 3:50, gold 2:40. Feats: Never touch the sludge, No damage, Deliver all 8 letters
  (a follow-up side quest, "Return to Sender", on the overworld).
* **Rewards.** Creator: rainy-day pieces in the Sugar Rush Sewers set (slicker, wellies, umbrella).
  Furniture: "Unsent letter (framed)". The side quest's rewards are friendships.
* **Music.** SONG_WALL_LULLABY (music box) in the pipes; SONG_OMINOUS for the boss.
* **Life sim / Talk.** The 8 letters are hash-picked from the town's adult residents; delivering one makes
  that resident say the thing to the person it was for (a news line, a friendship step, the "say it"
  theme of the finale ahead of time). Talk (PS1 / DC): a resident asked about their letter answers in
  character.
* **Content note** (section 6.3).
* **Platforms.** PS1 and Dreamcast. Not the GBA (the tide rewrites many cells per step, and the basins
  are long open views); its slot went to HB9, the other early dark level.

---------------------------------------------------------------------------------------------------

### HB12. The Wall (dark level)

* **Story place / unlock.** Chapter 4, after Ep 4.1 "The Gate" (Tallyworn puts the chalk down, then writes
  "3,661" anyway). The gatehouse gets "Inside the Wall". Tallyworn, Mascotty and Thumpkin (the 3-then-1
  thumps), Signhere's contract.
* **Theme and pitch.** The Wall is hollow. The Hellvile labourers who built it were kept inside it for a
  year, then at the pumps for ten. Their bunks are still here, and their chalk. The level walks **both
  faces of the Wall**: the Hillvile face (pastel bricks, cheerful murals "THANK YOU FOR NOT LOOKING")
  and the Hellvile face (soot, bunks, family photos pinned facing the bricks). **Abandonment and what
  the town's deal cost, played straight.** Mascotty's thumps come through the bricks and Thumpkin
  answers; you follow them. Bleak laughs: the Hillvile maintenance log ("Wall: still there. Good."), a
  suggestion box on the Hellvile face that has never been emptied, full.
* **Layout.** 64 x 20, a long east-west run with three stacked galleries (0, +12, +24). The gatehouse
  (east) -> bunk galleries 1-3 (lifts) -> the pump gallery -> the tally room (a wall of 3,652 chalk
  marks) -> the Foreman's yard at the west end: the top of the Wall under the sky, both towns in view.

```
  W <------------------------- the Wall, west to east -------------------------> E
  ################################
  #YARD (sky) #TALLY  #PUMP   #BUNKS 3#   three galleries: fh 0, 12, 24 (lifts)
  #  B  both  # ROOM  #GALLERY#BUNKS 2#
  #  faces    #|||||||#~~~~~~ #BUNKS 1# S  S  the gatehouse (east end)
  #  in view  #  L    #   L   #   L   #    L  levers: flip Hillvile face <-> Hellvile face
  ################################
  Face A (Hillvile): murals, pastel; open where face B is bricked. Face B (Hellvile): soot, bunks.
```

* **Enemies.** **Mortar Imps** (grey implings throwing bricks), **Bricklayers** (brutes that lay
  barricades: scripted cell swaps), **Wall Sentries** (gargoyles dormant in the murals), **Overseers**
  (vape wraiths carrying contract pages).
* **Mechanic: the two faces.** The Wall's inner rooms exist twice. A lever flips the whole level between
  face A and face B (two swap sets): passages open on one face and are bricked on the other. Demons on
  the other face are out of phase (drawn see-through, cannot hit or be hit) until you flip to them.
  *Card line:* "Levers (A) flip the Wall's two faces. Each face has other doors."
* **Boss. THE FOREMAN**, "Ten Years on the Job" (gargoyle x2.6 made of bricks). Every 8 s he rebuilds a
  wall between you and him (a swap set); flip faces to get round it. Taunt "Nobody leaves before the
  shift ends." (TALLYWORN, on the PA: "It ended.") Death toast "The Foreman crumbles. Somewhere, somebody
  sets down a trowel."
* **Secrets / collectibles.** Secrets: Tallyworn's bunk (his family photo, facing the bricks), the crack
  Mascotty thumps through, the top of the Wall (the vista). Collectibles: **10 chalk tallies** to count
  (Tallyworn's counter goes up as you find them; the last one is a heart).
* **Rank goals.** Par 4:10, gold 2:50. Feats: At most 12 flips, No damage, All 10 tallies.
* **Rewards.** Creator: new set **Wall Crew** (hard hat, work gloves, chalk behind the ear, tally-mark
  print, hi-vis vest). Furniture: "Tally board". Tallyworn +friendship.
* **Music.** SONG_WALL_LULLABY on face A and SONG_HELL_LULLABY on face B, crossfaded at each flip: the main
  game reveals at the Wall's fall that the two towns' songs are one tune; here they are heard turn and
  turn about, a quiet foreshadow.
* **Life sim / Talk.** Hellvile residents' mood +1 band for a day; news "Tallyworn writes 'Day one'".
  Talk (PS1 / DC): topics *the Wall*, *Tallyworn*.
* **Content note** (section 6.3).
* **Platforms.** PS1 and Dreamcast. Not the GBA (a 64-cell corridor with long sightlines, two layers and a
  vista).

---------------------------------------------------------------------------------------------------

### HB13. Boneyard Buffet & Bandstand

* **Story place / unlock.** Chapter 4, after Ep 4.3. Bonejangles' side mission "Dance Routine 4" (he
  cannot turn it off, and the restaurant's old servo room is the only place that can). If the owner does
  not approve W15 (Bonejangles' origin thread), the level stands alone as an Activities row at his
  xylophone school; its lines tell the origin as a joke only.
* **Theme and pitch.** **The Walten Files parody level.** Boneyard Buffet & Bandstand (the in-world chain
  the owner named) was HaHaland's animatronic family restaurant until "the incident", which, the plaque
  says, "was mostly a fridge". Now it is a "historic site" with a gift shop, and the house band is still
  on stage: the Buffet Boys (Drummer Drumstick, Sir Loin on keys, and the frontman **Gravy Train Gary**,
  a gravy-boat animatronic). Every trope gets a pie in the face: solemn VHS training tapes ("EMPLOYEE
  ORIENTATION, TAPE 3 OF 9." Tapes 4 to 9 are also 3), the lore guy with a corkboard and red string who
  now sells the corkboard ($12.99), menu art "drawn in a moving car by someone who'd only heard of
  faces", a party package with "pizza, cake, and one (1) unexplained disappearance". Bonejangles tags
  along, tells his horrifying origin as a funny anecdote, whirs, and does Dance Routine 4 at every
  stinger. No decoding anywhere: every tape is a joke.
* **Layout.** 40 x 36, roofed, 0 to -3 (the ball pit) and +6 (the stage). The gift shop and lobby -> the
  dining hall (booths, party rooms) -> the ball pit -> the arcade (prize counter) -> the kitchen and the
  walk-in fridge -> backstage (the servo room, costume storage) -> the stage.

```
  ######################
  #STAGE   B  #BACK-   #   STAGE (fh 6): the Buffet Boys; GRAVY TRAIN GARY
  # *  *  *   #STAGE   #   *  three spotlights (shoot them to keep Gary in the dark)
  #####|#######servos  #
  #DINING HALL #########
  # booths booths |FRIDGE#  FRIDGE: the walk-in ("mostly a fridge"), secret 2
  # party rooms   |KITCHEN
  #####|#########|######
  #BALL PIT (fh -3)#ARCADE   prize counter; a vent crawl to SECURITY (secret 3)
  # oOoOoOoOo      #  s3 #
  ####|############|#####
  #GIFT SHOP  LOBBY   S  #   S  start: the "historic site" plaque
  ########################
```

* **Enemies.** **Party Pals** (a new base demon: an 80s animatronic mascot in a felt suit, original
  design, cute-uncanny; AI *freezer*), **Ball Pit Lurkers** (slime belchers that surface from the balls),
  **Ticket Implings** (prize-ticket darts), **Cake Brute** (a brute in a party hat).
* **Mechanic: showtime.** Every ~10 s the stage lights blaze ("SHOWTIME!" jingle): every animatronic
  freezes in a pose and cannot be hurt (shots ping off); between shows the lights drop and they move
  fast. Breaker boxes (A) lengthen the dark or the show for the room you are in.
  *Card line:* "Lights on: animatronics freeze and can't be hurt. Lights off: they move."
* **Boss. GRAVY TRAIN GARY**, "Frontman. Forever. Contractually." (Party Pal x2.2). During a show he sings
  (invulnerable) and the band fires; in the dark he charges. Shoot out the three spotlights to keep him
  in the dark. Taunt "Happy birthday to YOU." Death toast "Gary takes a bow. His head keeps bowing after
  the rest of him stops."
* **Secrets / collectibles.** Secrets: the employee lounge, the walk-in fridge (the incident: a fridge,
  one sad sandwich), the security office (its monitors show you). Collectibles: **9 "Bandstand Tapes"**,
  each a gag ("Tape 7: How To Clean The Ball Pit. It does not say how. It sobs."); the ninth is labelled
  "3".
* **Rank goals.** Par 3:40, gold 2:30. Feats: Never shoot a frozen Pal, No damage, All 9 tapes.
* **Rewards.** Creator: the existing reward set **Bonejangles' Party Pals** opens here (staff visor,
  "Hi! I'm ___" pin, party hat, felt mascot face plate, "Employee of the Month" tie). Furniture: "The
  Buffet Boys (sing at parties)", "VHS player". Bonejangles +friendship; a hint line toward the machine
  heart (E09) for players who have not found it.
* **Music.** New **SONG_BANDSTAND** (a cheesy 80s jingle that breaks down a semitone each loop); the
  showtime stinger.
* **Life sim / Talk.** Birthdays in town can be held at the restaurant (the party package; nobody
  disappears); news "Historic site reopens as a historic site". Talk (PS1 / DC): topic *the tapes* (the
  lore-guy register; residents argue about Tape 3).
* **Platforms.** All, **GBA included**. GBA: showtime raises and drops the room's light bytes in the live
  map and plays the stinger; at most 4 Pals awake; the ball pit is a floor texture (no ball billboards).

---------------------------------------------------------------------------------------------------

### HB14. Hell Pits Highrise

* **Story place / unlock.** Chapter 4, after Ep 4.4 "Wisp's Room" (the Hell Pits flat is yours). Graveley,
  Wisp's parents and the building's landlord. Wisp is safe at home throughout; their crayon drawings are
  on the stairwell walls.
* **Theme and pitch.** **Hellvile tenement sitcom**: forty floors of the Hell Pits, an elevator "out of
  order since the Bronze Age", noise complaints written in blood (it is a demon's), the hallway smelling
  of bone soup. Now the Wall is down, the landlord has put the rent up. You cross between the two towers
  on laundry lines of black lace. PG-21 for adults: the couple in 13B who are "very welcoming", a sock on
  every third door handle, Grumbleguts stuck rear-first in the dumbwaiter (record scratch: "Don't look.
  ...You can look."). The kids playing hopscotch on lava in oven mitts are in the courtyard behind glass,
  cheering you on.
* **Layout.** 36 x 36 footprint, 0 to +100 steps. Two towers (west, east) with stairwell lifts, a
  courtyard between them open to the sky, laundry lines at floors 3, 6 and 9, the roof garden (boss).

```
  side view: west tower | courtyard (open sky) | east tower      heights in steps
  fh 100  [ROOF GARDEN B]
  fh  72  [ 9F ]=====line=====[ 9F ]        ===  laundry-line ziplines
  fh  48  [ 6F ]===line===[ 6F ]
  fh  24  [ 3F ]=====line=====[ 3F ]        courtyard: the kids' lava hopscotch, behind glass
  fh   0  [LOBBY S]  .. courtyard ..  [LOBBY]
          stairwell lifts in each tower; the elevator: "OUT OF ORDER SINCE THE BRONZE AGE"
```

* **Enemies.** **Bailiffs** (implings with rent books), **Guard Dogs** (hellhound pups), **Balcony
  Snipers** (cyber piggies), **Cornice Gargoyles**.
* **Mechanic: ziplines.** A at a line's anchor hangs you on it; you slide along it (a path), shoot while
  sliding, jump off anywhere. Demons on lines can be knocked off. Some lines must be hauled up with a
  pulley (A) first.
  *Card line:* "Grab laundry lines (A) to cross. Jump to let go."
* **Boss. LANDLORD LEASEHOLD**, "Rent Is Due. Rent Is Always Due." (cyber piggy x2.0). A shockwave
  ("EVICTION!") pushes you toward the roof edge; Bailiffs come in pairs. Pacifist route: pay the rent at
  the rent box: 500 H$ of your own money ("Pleasure doing business. Don't touch the thermostat.").
  Death toast "The Landlord is evicted. Nobody returns his deposit."
* **Secrets / collectibles.** Secrets: flat 13A (a drawing Wisp left for you), the dumbwaiter shaft, the
  rooftop water tower. Collectibles: **13 lost spare keys** (return them to residents for H$).
* **Rank goals.** Par 4:20, gold 2:55. Feats: Sky route (never touch the courtyard after the first line),
  No damage, Pay the rent (pacifist).
* **Rewards.** Creator: Hellvile Pits Punk pieces (studded collar, safety-pin ear cuff). Housing: the Hell
  Pits flat gains a balcony room. Furniture: "Laundry line", "Do Not Disturb sign".
* **Music.** SONG_HELLVILE with a lift-muzak intro.
* **Life sim / Talk.** News "Rent freeze in the Hell Pits!"; Hellvile residents' money +; Talk topic
  *rent* (everyone has an opinion).
* **Platforms.** PS1 and Dreamcast. Not the GBA (an open-sky courtyard between 100-step towers).

---------------------------------------------------------------------------------------------------

### HB15. The Department of Contracts

* **Story place / unlock.** Chapter 4, after Ep 4.5 (the Pump room: everyone tears one page of the
  900-page labour contract). Signhere is your guide, delighted for the first time to be *withdrawing* a
  signature. The Underbelly market gets an Activities row.
* **Theme and pitch.** Hell runs on paperwork, and Signhere's old department is still open: a **90s
  office-sim parody** (beige, CRT monitors, a screensaver of flying skulls) of the damned. The copier
  copies demons, the water cooler is lava, the coffee is worse. Motivational posters ("TEAMWORK: because
  suffering shared is suffering doubled"), passive-aggressive notes in the break room ("WHOEVER ATE MY
  SOUL: I KNOW"), the dress-down Friday where the demons wear only ties (PG-21, demons only, censor-bar
  gag; Mature off: ties and cardigans). One cryptic seasoning: a filing cabinet labelled "ARREARS - DO NOT
  OPEN UNTIL NEXT TIME" (it does not open; NG+ answers it).
* **Layout.** 40 x 40, roofed, low ceilings, 0 to +24. The lobby (take a number) -> the cubicle maze (low
  partitions you can see over) -> the records stacks (tall shelves, ladders) -> the copy room -> the
  break room -> the lift to the executive floor and the Notary's corner office.

```
  ####################
  #EXEC FLOOR  (fh 24)#   the Notary's corner office (boss)
  #  B    corner      #
  ######L##############   L  lift (needs the APPROVED stamp)
  #RECORDS  #COPY RM  #   RECORDS: tall stacks, ladders; COPY: the Copier [C] spawns copies
  # |||| |||#  [C]    #
  ###|#######|#########
  #CUBICLE MAZE       #   partitions fh 5: you see over them, not through the doors
  # [][][] [][][]  s1 #   s1 supply closet (a nap pod)
  # [][][] [][][]     #
  ####|################
  #BREAK RM#LOBBY   S #   S  start: the ticket machine (your number: 666)
  # (lava) # queue    #
  #####################
```

* **Enemies.** **Clerks** (implings in visors), **Middle Managers** (cyber piggies that call interns:
  implings), **Red Tape** (cloud spiders whose webs slow you), the **Copier** (a spawner prop that copies
  the last demon near it until you unplug it, A).
* **Mechanic: stamps and queues.** Doors marked PENDING need a stamp: RECEIVED, APPROVED, FILED, each held
  at a guarded desk. Some counters make you take a number and wait 20 s (you fight while the number
  comes up). A melee hit with a stamp on a stunned demon files it out (removed, not killed).
  *Card line:* "Collect stamps to open doors. Stamp a stunned demon (melee) to file it away."
* **Boss. THE NOTARY**, "Witness. Seal. Doom." (bone jester x2.0; its orbs are notarised pages). Each
  phase it seals the doors; the forms on its desk take your three stamps to unseal them. Pacifist route:
  stamp its own resignation letter. Taunt "Sign here. And here. And here." (SIGNHERE: "That's MY line.")
  Death toast "The Notary is notarised. Witnessed by everyone."
* **Secrets / collectibles.** Secrets: the supply closet, the executive washroom, the ARREARS cabinet's
  top (a red envelope, unopened). Collectibles: **8 "Employee of the Month" photos** (all of the same
  demon, Gary from Accounting).
* **Rank goals.** Par 3:30, gold 2:20. Feats: Stamp-only (pacifist), No damage, Never take a number.
* **Rewards.** Creator: new set **Office of the Damned** (lanyard, visor, clip-on tie, cardigan, coffee mug
  prop). Furniture: "Water cooler (lava)", "Motivational poster". Signhere +friendship.
* **Music.** New **SONG_FILING** (office bossa; the hell blend turns it to metal with a fax-machine lead).
* **Life sim / Talk.** Residents with office jobs get a "four-day week" mood bump; news "Signhere
  withdraws a signature. Weeps."; Talk topic *paperwork*.
* **Platforms.** All, **GBA included**. GBA: the copier makes at most 6 copies; webs are floor zones;
  the partitions' long views get a short view distance (fog at 14 cells).

---------------------------------------------------------------------------------------------------

### HB16. The Ninth Cloud

* **Story place / unlock.** Post-game, after the credits. The jar on your shelf (Fluffy, cupcake-sized)
  gets "Sleep and visit". It is Dream D2 "The Ninth Cloud" made playable and the ROADMAP's "Ninth Cloud
  map". Fluffy / Vapula, the Mayor, Nox, Vex.
* **Theme and pitch.** Fluffy is small now, but the cloud he was is still up there: the Ninth Cloud, a
  sky of unsent letters and "it's fine"s drifting the wrong way. You sleep, you go up, you bounce. The main
  game's **warm-down**: playful, floaty, a victory lap with a sigh. The Heavy Sigh (the very last of
  Vapula's leftover breath) is up there being dramatic about it. At the end one letter drifts past that
  is from nobody in town: a red envelope marked FINAL NOTICE. (The NG+ hook; it sets a flag that Notice 1
  reads.)
* **Layout.** 56 x 56 under the sky, 0 to +80. The dream bed (start, 0) -> the low-gravity ring (+24..40)
  -> the lantern isle and the letter stream (+48..64) -> the summit cloud (+80). Falling drops you to
  the cloud below, never far.

```
             (SUMMIT CLOUD)  B          fh 80
            /              \
   (LANTERN ISLE)      (LETTER STREAM)  fh 48..64     ^ bounce pads link the islands
            \              /
           (LOW-GRAVITY RING)           fh 24..40     . low-gravity zones: long floaty jumps
                   |
            (THE DREAM BED)  S          fh 0
```

* **Enemies.** **Cloud Spiders** (native), **Puff Demons**, **Sighs** (vape wraiths), **Storm
  Gargoyles** diving from above.
* **Mechanic: bounce pads and low gravity.** Cloud pads launch you up and across (an impulse); in
  low-gravity zones jumps are long and slow and mantles reach higher.
  *Card line:* "Bounce pads launch you. Jumps float in the soft clouds."
* **Boss. THE HEAVY SIGH** (a grey Lord Vapula x0.8). It blows gusts that push you while you float, and
  fires "it's fine" pellets. Pacifist route: catch the seven letters of the stream and read them to it;
  it rains and drifts off. Taunt "Ugh. You AGAIN." Death toast "The Heavy Sigh lets go. It sounds like
  relief."
* **Secrets / collectibles.** Secrets: under the dream bed, the lantern isle's top, the far letter.
  Collectibles: **7 letters** (the Mayor's, Nana's and Fuzz's from D2, and four from the town).
* **Rank goals.** Par 4:00, gold 2:40. Feats: Never fall, No damage, Read the letters (pacifist).
* **Rewards.** Creator: Cloud Nine pieces (cloud slippers, extra halos). Furniture: "Fluffy's rain
  cloud" (waters the garden: "he rains on sunflowers on request" becomes a daily house action). The FINAL
  NOTICE envelope (a keepsake; NG+ Notice 1 opens it).
* **Music.** SONG_MINGLE (the music box); the boss gets a gentler SONG_BOSS.
* **Life sim / Talk.** Garden plants watered daily; Talk (PS1 / DC) topic *Fluffy*.
* **Platforms.** PS1 and Dreamcast. Not the GBA (open sky, wide views, floaty camera).

---------------------------------------------------------------------------------------------------

## 4. NG+ story arc: "ARREARS"

### 4.1 The premise in one breath

Ten years ago, when the Mayor had the fountain re-plumbed to "keep the giggles IN and pump the glums
OUT", the glums had to go somewhere, and somewhere has an accounts department. **Hell's Bureau of Unfelt
Things** took ten years of HaHaland's swallowed feelings on account, at interest, underwritten by
**Duchess Arrears**, Head of Collections. The main game spent that account: the Wall came down, the
contract was torn, everyone commingled in Hellvile Park. But in Hell, **tearing a contract is signing it**.
The happy ending was a loan. In New Game+ the loan comes due, the town is asked to pay in joy, and the
only currency that can actually settle it is the one HaHaland never learned to spend: feeling it.

The main game's moral was "say it out loud". The arc's is the next, harder step: **feel it, and own what
it cost other people**. The main game's villain was a lost pet made of fear; this one is an
accountant made of nothing at all.

### 4.2 The antagonist: Duchess Arrears

* **Who.** Duchess Arrears, Head of Collections, Accounts Receivable, the Bureau of Unfelt Things (Ninth
  Circle, mezzanine). A demon (adult). Original design: very tall and thin, moth-grey, a gown-cut suit
  tailored from overdue notices with red-ink trim, a hat folded like a sealed envelope with a wax seal,
  glasses on a chain, a fountain pen that writes in breath, long fingers that are always counting.
* **Voice.** An automated phone line, perfectly polite, never raised: "Press one to pay. Press two to
  suffer. Your call is important to us." Her boxes type at a constant speed with no babble variance (the
  only character who does), so she reads as a machine.
* **Register.** Deadpan menace, PG-21 adult comedy at the edges: Grumbleguts flirts with her and she
  bills him for it ("I bill by the wink." "Put it on my tab." "You ARE the tab."). She is never crude
  herself. Two policies, both rules of this plan: "The Bureau does not hold minors' accounts." and
  "Pets? Priceless. Policy." (no child and no pet is ever collateral).
* **Method.** She never hurts a cast member on screen: she **files** them (a folder, a stamp, a soft
  sound, then silence). Her demons are the Bureau's staff: Late Fees, Repo Men, Bailiffs, Auditors,
  Wardens, Foreclosers, the Interest.
* **Wound / twist.** She is the Bureau's oldest unfelt thing: the very first debt nobody ever felt, given
  a desk. She has never felt anything ("It's very efficient."). She collects feelings because she has
  none. In the finale the pet beeps at her, and she flinches: the first thing she has ever felt (the main
  game's "the demons flinch at the beep" running gag, paid off at the top).

### 4.3 The stakes

* **The town.** Pay in full by the end of the reckoning or the Bureau repossesses HaHaland: every laugh,
  every name, the park, and the people who signed (the Mayor first). Ten years of kept labour, the
  plumbing and the Wall come due as consequences the town has to sit in (Notices 8, 10, 11).
* **You.** The midpoint shows that you co-signed (page 412). From then on the collateral is personal:
  whatever **your last ending** gave you (4.9) is on the Bureau's books, and the arc's choices put
  residents of your own town at stake (4.7).
* **The cost of winning.** Every Act III level asks something real: who to free (Notice 8), whose name to
  give (Notice 11), what you can afford to buy back (Notice 6), what you can bear to feel (Notice 13). The
  ending decides who comes home.

### 4.4 Where it runs (and why the locked flow does not move)

* **Notice 1, *Day One*, is the cold open** of every first NG+ run: it plays before the prologue P0, ten
  years ago, with you as Tallyworn. It tells the player at once that New Game+ is heavier than the main
  game (owner, 2026-11-08). Later NG+ runs offer "Skip" on its card.
* **Notices 2-6 interleave with the replayed Chapters 1-4** as NG+-only side content (Activities rows and
  NG+ side missions at the places named below). They never block a main beat; the replayed story's
  scenes keep their NG+ "HaHaland Remembers" lines and gain a few Bureau asides (red envelopes in the
  mailboxes, the eruption's "Who's paying for this?").
* **Notices 7-15 are a new NG+-only Chapter 5, "Arrears"**, that opens the morning after the replayed
  festival (day 17), once Notices 1-6 are cleared. It is new story-engine content after the replayed
  credits, so the main chapters' `flow.lock` and `.dsc` beats are untouched (owner question 1).
* Every Notice stays replayable from the level select's "Arrears" page after it is cleared, in free play
  too.

### 4.5 Act structure

| Act | Notices | Where in NG+ | What the act does | Tone |
|---|---|---|---|---|
| **Cold open** | 1 *Day One* (dark) | before P0, ten years ago | shows the debt being made: the Wall's last brick, the labour contract and the plumbing order stamped UNDERWRITTEN; the first tally mark | heavy, quiet; the floor of the arc |
| **I. Past Due** (setup) | 2 *The Dead Letter Office*, 3 *Repo Night* | Ch1 and Ch2 replay | red envelopes; the Bureau's comic staff; the Duchess on an answering machine, then in person at the eruption: "Who's paying for this?" | comic, with dread seasoning |
| **II. Collections** (escalation) | 4 *The Interest Engine*, 5 *Lake of Fire Regatta*, 6 *Everything Must Go* | Ch3 and Ch4 replay | the Bureau's grip on both towns; bigger set pieces; the first moral weight (you cannot buy everything back); the Duchess meets Grumbleguts | comic, stakes rising |
| **Midpoint turn** | 7 *The Audit* | Chapter 5 opens (day 17) | page 412: you co-signed; the happy ending was a loan; the Mayor is filed | the joke turns on you |
| **III. Default** (late descent) | 8 *The Debtors' Prison* (dark), 9 *The Artist's Studio* (dark), 10 *The Ward of Forgotten Names* (dark), 11 *The Hollow Mingle* (dark), 12 *The Interest*, 13 *The Vault of Unfelt Things* (dark) | Chapter 5 | the town's sins come due: complicity, failure, forgetting, abandonment, grief; two hard choices; the descent to the bottom of the Bureau | mature, heavier, few jokes (bleak and deadpan) |
| **Finale** | 14 *The Ledger* | Chapter 5 climax | the whole cast storms the Ledger; three-phase final boss; the Great Rain; the final choice | bombastic, cathartic |
| **Epilogue** | 15 *Both Are Better* | after the ending card | the first real Mingle since the park closed; who came home depends on the ending | warm |

The maturity curve: Act I's comedy is the game's normal register; Act II adds stakes and the first
choice; from the midpoint the writing gets adult in theme (complicity, guilt, consequences that stay,
choices that cost someone, grief), the jokes thin out to bleak and deadpan ones, and the register stays
PG-21 adult comedy where there are jokes (SCRIPT_STYLE 1.1). The hard limits do not move (section 2.1).

### 4.6 Synopsis, Notice by Notice

1. **Day One** (cold open, dark). Ten years ago. You are Tallyworn hauling the last bricks up the Wall's
   scaffolds; grey Glumlings, the first glums the new pipes pumped, crawl up after you. You lay the last
   brick; the gate is sealed from the Hillvile side to a brass band; the foreman reads page 1 of 900; a
   very tall woman in a gown of envelopes stamps the contract UNDERWRITTEN without looking up, and the
   plumbing order signed "Giggs" too. Tallyworn makes the first chalk mark. Hard cut to the pink bus.
2. **The Dead Letter Office** (Ch1 replay). After the Giggling Fog, every mailbox holds a red FINAL
   NOTICE. The post office's dead-letter vault fights back; the Duchess leaves a message on the answering
   machine. Among the dead letters: Tallyworn's five from Day One, and one in your handwriting, postmarked
   "last time".
3. **Repo Night** (Ch2 replay). The repo crew takes the town's things, your sofa and Grumbleguts' belt;
   you raid the impound lot. Act I ends in the NG+ eruption: the Duchess steps out of the crack behind
   Vapula with a clipboard. "Hold on. Before you start. Who's paying for this?"
4. **The Interest Engine** (Ch3 replay). Under the invaded plaza, a cathedral of gears compounding the
   debt onto Anvil's old pipes, painted beige. Rivet shuts it down; the numbers spin backwards. The
   Interest learns to grow.
5. **Lake of Fire Regatta** (Ch4 replay). A lien on the Lake of Fire; Hellvile's fishers race the
   dredgers with Ember piloting your raft and cooking on the lava. The dam bursts; the lake comes back.
6. **Everything Must Go** (Ch4 replay). The Pawn Palace auctions what was taken: laughs in jars, the
   Mayor's hat, Grumbleguts. The Duchess presides and bills Grumbleguts per wink. You buy back what you
   can afford. You cannot afford everything.
7. **The Audit** (midpoint, Ch5 opens). The Audit Hall rises out of the bandstand. Signhere argues the
   town's case. Page 412: in the main game you tore the breach clause; in Hell, tearing is signing. The
   happy ending was a loan, and you co-signed it. The Duchess files the Mayor and the bandstand goes dark.
8. **The Debtors' Prison** (dark). The bunks where the labourers slept for ten years now hold Hillvile's
   signatories. Tallyworn walks you in and does not use his chalk. One master key: free the Mayor, or the
   twelve clerks who filed the plumbing order. The other stays.
9. **The Artist's Studio** (dark). The Bureau's oldest account is a soul: the artist whose tribute became
   Bonejangles. Under the Buffet's fridge, in the studio, you are a ghost; Fuzz admits the failed rite
   was his, and with Nox finishes it thirty years late. The portrait finishes itself.
10. **The Ward of Forgotten Names** (dark). Names are collateral now; the town forgets itself. In the
    Bureau's ward the unnamed rooms dissolve; Rook's ten-year log brings them back, one name at a time.
11. **The Hollow Mingle** (dark). The Bureau forecloses on the park and rewinds it to the night it
    closed. You walk the Mayor (or Nox) across the frozen Mingle to set two bowls; the purple cloud of
    eleven years ago moves for the first time. The Duchess offers a deal: one name, and the Interest
    stops. Refuse, or pay with someone.
12. **The Interest** (chase). Ten years of unfelt everything, the size of a building, chases you down into
    Accounts Receivable while the Wall rebuilds itself out of notices. Mascotty's thumps show the gaps;
    at the bottom she takes her head off again. Vex does not drop the jar.
13. **The Vault of Unfelt Things** (dark, the darkest). Shelves of boxes, each a feeling someone did not
    feel; a memorial wall with the names of your own town's dead. Wick carries the light. Doors open only
    when you stand still and feel what is in them. The Collateral comes apart into a thousand small
    boxes, some of them open.
14. **The Ledger** (finale). Everyone storms the book; the Duchess falls back page by page, merges with
    the Interest, and in the Great Rain the town finally feels ten years at once. The pet beeps; she
    flinches. You choose.
15. **Both Are Better** (epilogue). The first real Mingle since the park closed. Lanterns, two bowls and a
    cupcake, and whoever came home.

### 4.7 The finale and the choices

**Notice 14, *The Ledger*,** tops the main finale on purpose: the main game's Say It sends giggle bubbles
through a fountain and drops one Wall to two songs that turn out to be one tune; the Ledger has the whole
cast in action at once, a three-phase boss that becomes a sky-filling colossus, and the **Great Rain**, in
which Fluffy (cupcake-sized, in Vex's jar) rains for real over both towns and every resident of your town
stands in it, while **four** themes (HaHaland Day, Hellvile, the Mingle music box and the Wall Lullaby)
resolve into one chord. The last blow is not a gun: it is the **fourth player line** (the main game has
three, story_final 8.4): "I'm sorry." / "We felt it." / "Your turn." / "You're forgiven." (saved as
`STRY.arrears_line`; the epilogue quotes it).

The choices that feed the ending:

| Where | Choice | Consequence |
|---|---|---|
| Notice 6 | buy back up to 5 joys with your own H$ (money for about 3) | bought ones return now; the rest are held until the ending |
| Notice 8 | free the Mayor **or** the twelve clerks | Notice 11 escorts the Mayor (freed) or Mother Nox (not); the other stays held until the ending |
| Notice 11 | give the Duchess one adult resident's name **or** refuse | gave: Notice 12 is not one-hit, that resident is held; refused: Notice 12 is one-hit |
| Notice 14 | **Forgive** her / **Collect** her / (secret) **Audit yourself** | the arc's ending (4.8) |

### 4.8 The arc's endings

Proposal for the owner (question 1): use the three **retired ending slots** E03, E05 and E07 (they exist
in `ending.h`, never trigger and have no design) for the arc, so `ENDING_COUNT` stays 15 and the list's
"n/11" becomes "n/14" again. If the owner prefers, these are NG+-only epilogue variants that do not count
as endings and `ending.h` does not change.

| Ending | Kind | Trigger | What happens |
|---|---|---|---|
| **E03 "Paid in Full"** | good | Forgive | Feeling is the currency: the town paid in kind in the Great Rain. Everyone held comes home (the Mayor or the clerks, the named resident, the unbought joys). The Duchess feels for the first time ("...Oh. Is this what it's like? It's terribly inefficient.") and stays as HaHaland's bookkeeper: an adult resident with a roster shadow record (like Grumbleguts), friendable and datable under the normal rules; she bills per date. Title screen: the Ledger closed with a bookmark. |
| **E05 "Collateral"** | bad | Collect | You file her into her own vault. The debt is closed by force and nothing is forgiven: whoever is held stays held. They can be visited at the Bureau's visiting hours (a weekly free-play visit) and one can be redeemed at each yearly Mingle event, so the loss is slow and real, never cruel. The town is safe and a little colder. Title screen: the Ledger padlocked. |
| **E07 "Clean Slate"** | secret | Audit yourself (only offered if you refused the deal in Notice 11, felt every memory in Notice 13 without interruption, and collected all of Rook's log in Notice 10) | You close the account in your own name by handing back the first run's ending: the polaroid in the suitcase frame (E2) goes into the Ledger. Everyone comes home, the Duchess walks free, unfiled and unforgiven, to work out what she is. In the epilogue the frame is empty again, and at its end the town takes a new photo with you in it. |

The NG+ replay's own finale (Ep 4.7) still resolves its ending as today (E01 / E02 / E06 / E11 / a route
ending); Chapter 5's ending is a second card at the end of the arc. The household tier 2 grant
(`UL_LIFE_HOUSEHOLD_T2`, cap 20, GBA 10) stays where it is (the second finish) and is also granted by the
arc's ending card for saves that skipped it.

### 4.9 Ties to the 15 endings: the collateral

Notice 2's red envelope itemises the collateral, read from the previous run's **last ending**
(`ngp_` carries the seen endings and their counts today; LV22 adds the last one, 1 byte). The finale
returns it (E03, E07) or keeps it held (E05). Each item is a running thread through the arc:

| Last ending of the previous run | On the Bureau's books | How the arc uses it |
|---|---|---|
| E01 / E02 (the finale, good) | the festival polaroid from the suitcase frame | the Duchess has it pinned to her desk; returned in N14 |
| E04 "Family Portrait" | your family portrait (the house's photo) | it hangs in the Pawn Palace (N6), price: more than you have; returned or not by the ending |
| E06 (empty parade) | nothing: "Your account is empty. Even we feel bad." | the bleakest joke of the arc; she sends a fruit basket |
| E08 (the fourth bad ending) | the Days Since Last Fire sign | it reads 0 in every Notice until the ending |
| E09 "Bonejangles Bows" | Bonejangles' machine heart | it is the key to the Artist's Studio (N9) |
| E10 "Every Sparkle" | the town's secret sparkles | they go dark on the overworld during Ch5 |
| E11 (NG+ with the bottle and suitcase) | the bottle and the suitcase themselves | the suitcase is on the auction block (N6) |
| E12 (Grumbleguts route) | Grumbleguts | he is thrilled to be held ("Finally, someone wants to keep me.") and auctioned in N6 |
| E13 (Vex route) | Vex | he is filed in N7 with the Mayor; the jar passes to Rivet until N12 |
| E14 (any adult partner) | your partner | held from N7; your household's shared rooms show their empty chair |
| "Nope." | the envelope just says "Nope." | the Duchess: "...Rude." The arc runs with the E01 collateral |

### 4.10 Ties to the life sim

* **Held residents.** "Held by the Bureau" is a new *away* reason on the existing away-trip state (F9),
  not a death: held residents leave the island, keep their records, get a news line (`NT_VANISHED` plus
  new `NT_HELD` / `NT_RETURNED` / `NT_DEBT_PAID` templates) and come home or stay held by the ending.
  Children and pets are never held (the Bureau's policies).
* **Names forgotten** (Notice 10): during that stretch of Chapter 5 the island and the news show "???" for
  names; restored when Notice 10 is cleared.
* **Money.** Notice 6 spends your real H$; Notice 2's red envelope puts the town in a one-day "debt dread"
  mood band that clearing lifts.
* **Memorial.** Notice 13's memorial wall reads the town's dead residents (adults, elders and pets; the
  life sim never kills children). The graveyard gets a lantern per name afterwards. Option to hide names
  (section 6.4).
* **The Duchess as a resident** (E03): an adult with a shadow record, so she can befriend, date, move in,
  be played as (play-as, post-game) and appear in Talk.
* **Talk (PS1 / DC).** New topics: *the Bureau*, *the debt*, *the Mingle*, *names*, each resident's
  returned feeling (Notice 13), the Duchess (E03); child-safety and PG-21 rules as in TALK_DESIGN.md.
* **Household.** Tier 2 (cap 20, GBA 10) granted as in 4.8.
* **Co-op heroes.** Each gets one NG+ quest part tied to a Notice (below); a co-op guest can play the
  Notices like any level (the main hero still drives the plot flags).

### 4.11 Payoffs for the cast and the co-op heroes

| Character | Where | Payoff |
|---|---|---|
| **Mayor Giggs** | N1 (his plumbing order stamped), N7 (filed), N8 (freed or not), N11 (escorted) | sits in the cell he made others sit in; at the bandstand sets two bowls without being told to; in N14 signs his own name under the debt: "I signed it once to make it go away. I'm signing it now to keep it." |
| **Mother Nox** | N9 (the backwards rite), N11 (escorted if he was not freed) | walks across the park she waited at for eleven years; "I've waited ten years. I can walk across a park." |
| **Father Fuzz** | N9 | says it out loud: the failed rite was his; finishes it with Nox thirty years late; forgives himself (the Mr. Huggins line, inverted: "The hug was for me. It took."). |
| **Rivet and Anvil** | N4, N14 | shut down the engine built on Anvil's pipes; weld the Ledger shut on the last page |
| **Vex** | N3 (shop repossessed), N12 (the jar), N14 | in the Great Rain asks the Duchess the one question nobody asked him: "Who listened to YOU?" |
| **Grumbleguts** | N3 (belt), N6 (auctioned), N14 | flirts with the Duchess until she bills him; belly-bumps the Bailiffs off the margins in N14; under E03, the first resident she bills for a date |
| **Bonejangles** | N9 | the origin he told as a joke, told sincerely once; the artist's soul let go; "Clack." |
| **Tallyworn** | N1 (you play him), N8, N14 | makes the first mark in N1; in N8 holds the chalk and does not use it; in N14 counts the balance down to zero and draws a heart |
| **Signhere** | N7, N14 | loses the Audit; finds the clause in the finale: "Payable in KIND. Feelings, not joy!" |
| **Dr. Bloom, Grampy Bean, Sootkin** | N10 | the ward run by someone who knows two-door clinics; Grampy, who has forgotten before, keeps everyone calm; the sunflower faces both ways |
| **Mascotty and Thumpkin** | N12 | her head goes back on in Ch5; at the bottom it comes off for good: "I'm not going back in a box." |
| **Lady Smog, Hattie and Haze, Steamy, Nana** | N13 | each opens their own box in the vault; Hattie coaches the Duchess's first laugh in N15 (E03) |
| **Pip and Wisp** (kids) | N15 only (and safe at home) | float paper boats with IOUs that turn into "I owe you nothing" |
| **The pet** | N13 (its file), N14 | its beep is the first thing the Duchess ever feels |
| **Fluffy / Vapula** | N14 | the Great Rain |
| **Ember** (co-op) | N5 | pilots the raft and feeds the regatta: the cooking that was "too warm" for Hellvile is exactly right; feeds the front line in N14 |
| **Rook** (co-op) | N10 | his ten-year log, never read, restores the town's names; read aloud in N14 to strike false entries |
| **Wick** (co-op) | N13, N15 | carries the light through the vault; lights the first Mingle lantern themselves, a small one |

### 4.12 The GBA's version of the arc

The GBA has Notices 1, 7 and 14 as levels (section 7). The other twelve reach the GBA as **story recap
cards** (the cutscene engine's comic panels, 6-10 panels each, with the level's set piece as the panel
art) that carry the same flags and offer the same choices as menus (Notice 6 as a shop list, 8 and 11 as
choices), so the arc, its choices and its three endings are complete on the GBA. Notice 15 plays as a
story beat on the existing United Park overworld map. The recap cards cost ROM only.

---------------------------------------------------------------------------------------------------

## 5. The 15 New Game+ levels (Arrears, Notices 1-15)

### 5.0 Overview

| Notice | Level | Act | Opens | Dark | GBA | Size | Par / gold |
|---|---|---|---|:-:|:-:|---|---|
| 1 | Day One | cold open | first NG+ run, before P0 | **dark** | yes | 36 x 30 | 3:00 / 2:10 |
| 2 | The Dead Letter Office | I | NG+ Ch1, after C1_WALL | | | 36 x 32 | 3:00 / 2:00 |
| 3 | Repo Night | I | NG+ Ch2, after Ep 2.3 | | | 48 x 44 | 3:40 / 2:30 |
| 4 | The Interest Engine | II | NG+ Ch3, after the first nest | | | 44 x 44 | 3:50 / 2:40 |
| 5 | Lake of Fire Regatta | II | NG+ Ch4, after Ep 4.2 | | | 56 x 56 | 4:20 / 3:00 |
| 6 | Everything Must Go | II | NG+ Ch4, after Ep 4.5 | | | 40 x 40, 4 floors | 3:40 / 2:30 |
| 7 | The Audit | midpoint | Ch5 opens (day 17; Notices 1-6 cleared) | | yes | 32 x 40 | 3:00 / 2:15 |
| 8 | The Debtors' Prison | III | Ch5 | **dark** | | 40 x 40 | 4:00 / 2:45 |
| 9 | The Artist's Studio | III | Ch5 | **dark** | | 32 x 36 | 4:10 / 2:50 |
| 10 | The Ward of Forgotten Names | III | Ch5 | **dark** | | 36 x 36 | 3:50 / 2:40 |
| 11 | The Hollow Mingle | III | Ch5 | **dark** | | 48 x 40 | 4:30 / 3:10 |
| 12 | The Interest | III | Ch5 | | | 24 x 64 | 2:40 / 1:50 |
| 13 | The Vault of Unfelt Things | III | Ch5 | **dark** | | 44 x 44 | 4:30 / 3:10 |
| 14 | The Ledger | finale | Ch5 | | yes | 64 x 40 (7 pages of 16 x 16) | 9:00 / 6:30 |
| 15 | Both Are Better | epilogue | after the arc's ending card | | | 48 x 48 | no par |

Each level's intro card kicker is its notice ("NOTICE 7: AUDIT"; Notice 14 is "FINAL NOTICE", Notice 15
"PAID IN FULL"). Every Notice has a scripted **set piece** (camera, captions, music cue and a map change
by the level script, section 8.2), which the main levels have only at their bosses.

The Bureau's staff (enemies shared across the Notices): **Late Fees** (new base demon: a clerk imp with a
clipboard and visor; its stamped envelopes stick a "late fee", -10% damage until you roll it off),
**Repo Men** (new base demon: a hulking tow-hook demon, AI *thief*), **Bailiffs** (brute variant),
**Auditors** (impling variant), **Wardens** (hellhound variant, AI *sleeper*), **Foreclosers** (gargoyle
variant, snatchers), **the Interest** (slime belcher variant, AI *splitter*; in Notice 12 the pursuer,
AI *stalker*), **Overdrafts** (puff demon variant), **Red Tape** (cloud spider variant), **the Blanks**
(faceless vape wraiths), **Collateral** (body-horror brute / slime variants: demon flesh only).

---------------------------------------------------------------------------------------------------

### Notice 1. Day One (dark level; the cold open)

* **Place.** Cold open: plays at the start of a first NG+ run, before P0 "The Egg Crossing"; "Skip" on
  later runs; replayable from the Arrears page. Ten years ago. You play **Tallyworn** (a level hero
  override: his look in third person, a bricklayer's pick, a nail gun found on site).
* **Pitch.** The Wall is one row short. The Hellvile crew were promised that when the last brick goes in,
  they go home. You haul the last bricks up the scaffolds while the sky turns, and the first glums the
  fountain's new pipes are pumping (small grey Glumlings) crawl up after you. **Abandonment and a
  contract that traps people, played straight**; fantasy labour, no real-world imagery (no uniforms,
  insignia, slogans or camps). Bleak laughs: the site sign "DAYS SINCE LAST ACCIDENT: we stopped
  counting"; the foreman's "Good news: there's a pension. Bad news: it's in the contract."
* **Set piece.** Laying the final brick at the top as the sky goes purple; the Hillvile ribbon-cutting
  heard through the bricks (a brass band, a ribbon, "Hee. Hee."); below, the gate sealed; the foreman
  reading page 1 of 900; the Duchess's silhouette stamping UNDERWRITTEN on the contract and on a plumbing
  order signed "Giggs"; Tallyworn's first chalk mark (players of HB12 have seen the 3,652nd). Hard cut to
  the pink bus and the cute prologue.
* **Layout.** 36 x 30, 0 to +40, mostly roofed. The brickyard sheds -> scaffold tunnels inside the
  unfinished shell (hoists as lifts) -> the hod ramps -> the top (a 3-cell walkway between tall parapets,
  the only open sky) -> the inner stair down to the new Pump House, whose doors close.

```
  ####################
  #TOP: walkway (sky)#   the last row: lay the final bricks here (fh 40)
  #==== B ====  gap  #   B  the Overseer (mini-boss)
  ###H###########|####   H  hoists (lifts) inside the shell
  #SCAFFOLD  #  hod   #   hod ramps (fh 8..32); gaps to bridge with bricks
  #TUNNELS gp# RAMPS  #   gp a gap in the shell
  #   |    ###|##  s1 #   s1 the lunchbox (a note from home)
  ####|#########|######
  #BRICKYARD S # PUMP  #   S  start among the sheds; PUMP HOUSE: the doors close (E)
  # hods  sheds# HOUSE E#
  ######################
```

* **Enemies.** **Glumlings** (impling variant, grey, weak: the first glums), **Scaffold Gargoyles**,
  **Overseers** (vape wraiths carrying contract pages: the Bureau's).
* **Mechanic: two-handed carry.** A picks up a brick hod: no weapon, slower, no jump while carrying; B
  sets it down to fight. At a marked gap, A lays the bricks: a swap set closes the gap, or builds the step
  or bridge you need. Every brick you lay closes the Wall a little more: the irony is the point.
  *Card line:* "Carry bricks (A) to the gaps. B sets them down so you can fight."
* **Boss.** **THE OVERSEER** (vape wraith x2.0) on the top: it scatters your hods. Then the scripted end
  (no fight with the Duchess).
* **Secrets / collectibles.** Secrets: Tallyworn's lunchbox, his family photo (he pins it facing the
  bricks: the photo HB12 shows). Collectibles: **5 letters home** he writes on breaks and never sends (they
  turn up in Notice 2's dead-letter vault).
* **Rank goals.** Par 3:00, gold 2:10. Feats: Never fight while carrying, No damage, All 5 letters.
* **Rewards.** Wall Crew piece (Tallyworn's flat cap); furniture "The first tally mark (framed)".
* **Music.** None. The ribbon-cutting band muffled through the bricks (SONG_HAHALAND_DAY, low-passed);
  the stamp lands on the first phrase of the new **SONG_LEDGER**.
* **Life sim / Talk.** Tallyworn's bio line in the NG+ town: "Counting since day one."; Talk topic *day
  one* with Tallyworn.
* **Content note** (section 6.3).
* **Platforms.** All, **GBA included**. GBA: the top is a narrow walkway between tall parapets with fog at
  12 cells; the hod is a held-item sprite; at most 6 Glumlings awake.

---------------------------------------------------------------------------------------------------

### Notice 2. The Dead Letter Office

* **Place.** NG+ Chapter 1, after C1_WALL: the fog leaves a red envelope in every mailbox (the mailbox
  running gag: "junk mail from The Other Side" becomes FINAL NOTICE). The Post Office gets "Dead Letters"
  at night. Ch1 has no weapon yet, so Rivet leaves a crate on your doorstep ("Just in case. Don't make it
  weird. Again."): the NG+ loaner rule. Rook noticed the envelopes first (his log).
* **Pitch.** The post office's dead-letter vault, where letters nobody would read went for ten years, has
  woken up hungry. A **postal-service parody**: "We deliver! (Eventually.) (Not to you.)", a queue that
  has not moved since the Wall went up, pneumatic tubes that mostly deliver demons. The Duchess's first
  contact is an answering-machine message ("This is a message for: HAHALAND. Your account is: OVERDUE.
  Press one to pay. Press two to suffer."). Seasoning: Tallyworn's five letters, and one addressed in
  your own handwriting, postmarked "last time".
* **Set piece.** The Mail Storm: every tube in the sorting hall fires at once while the answering machine
  plays over the PA and demons pour out of the canisters.
* **Layout.** 36 x 32, roofed, 0 to +16: the counter and queue -> the sorting floor -> the tube room ->
  the dead-letter vault (stacks) -> the pigeon loft (a short open-sky roof).
* **Enemies.** **Late Fees**, **Return-to-Senders** (puff demon variant: bounces one shot back), **Mail
  Hounds** (hellhound pups).
* **Mechanic: pneumatic tubes.** A at a tube station sends you through the network (a 12-tick whoosh) to
  the station the junction levers point at; demons arrive by tube too.
  *Card line:* "Tube stations (A) send you where the levers point."
* **Boss.** **POSTMASTER PAIN**, "Neither Rain Nor Sleet Nor Mercy" (gargoyle x2.0): dives into a tube and
  pops out of another station; re-route the junctions to trap him in a loop and catch him at the exit.
  Taunt "Sign for this." Death toast "Postmaster Pain is returned to sender. Postage due."
* **Secrets / collectibles.** 3 secrets; **10 commemorative stamps** (a stamp album page in the Library).
* **Rank goals.** Par 3:00, gold 2:00. Feats: At most 6 tube rides, No damage, All 10 stamps.
* **Rewards.** Weld charm "Stamp"; furniture "Pneumatic tube"; mailbox skin "Red letter".
* **Music.** SONG_LEDGER as hold music (a lounge arrangement).
* **Life sim / Talk.** News "Every mailbox gets the same letter"; one day of "debt dread" (mood band -1)
  lifted by the clear; Talk topic *the red envelope*.
* **Platforms.** PS1, Dreamcast. GBA: recap card.

---------------------------------------------------------------------------------------------------

### Notice 3. Repo Night

* **Place.** NG+ Chapter 2, after Ep 2.3 (things move in the house; this time it was the repo crew: your
  sofa is gone). Tow-truck tracks lead to a new place, the **Impound Lot**. Vex ("Finally. Somebody take
  it."), Grumbleguts (his belt).
* **Pitch.** The Bureau's repo crew has taken the bakery's oven, the playground slide, the Mayor's spare
  hat, Vex's shop sign and your sofa, and stacked it all in a floodlit lot. A **repo-reality-TV parody**
  ("REPO OF THE DAMNED, Season 666") with imp camera crews narrating your every move in confessional
  asides. PG-21: Grumbleguts' belt is repossessed mid-scene (record scratch; "That's not the only thing
  holding this together, sugar.").
* **Set piece.** Riding the crusher's conveyor down the cottage-sized stack as it is eaten; then the NG+
  eruption: the Duchess steps out of the crack behind Vapula with a clipboard. "Hold on. Before you
  start. Who's paying for this?" (end of Act I).
* **Layout.** 48 x 44, a floodlit yard under the night sky: stacks of repossessed things as cover (a whole
  cottage, gumball machines, the Fun Fair's spare carousel horse), the office shack, the crusher, the
  gate.
* **Enemies.** **Repo Men** (new base demon), **Camera Imps** (harmless implings that call your position
  to the crew), **Floodlight Gargoyles**, **Tow Brutes**.
* **Mechanic: repossession.** A Repo Man's hook that hits takes your equipped weapon and tows it to an
  impound cage; fight on with what is left (fists, the loaner) and break the cages (A, or shoot the lock)
  to take your guns back.
  *Card line:* "Hooks steal your weapon. Break the cages to get it back."
* **Boss.** **TOW-TRUCK TERRY**, "Everything Must Go (With Me)" (brute x1.8 on a tow-truck prop): his winch
  drags you in; shoot the winch. Taunt "Nice weapon. Shame if it got... repossessed." Death toast
  "Terry's truck gets towed. By Terry's other truck."
* **Secrets / collectibles.** 3 secrets; **8 repo receipts** ("Repossessed: one (1) giggle. Reason: ten
  years unpaid.").
* **Rank goals.** Par 3:40, gold 2:30. Feats: Never lose a weapon, No damage, All 8 receipts.
* **Rewards.** Your sofa back, re-upholstered; weld underbarrel "Tow hook" (pulls a small demon to you);
  Vex +friendship.
* **Music.** SONG_INVASION with a TV-theme sting.
* **Life sim / Talk.** Residents' repossessed furniture comes home after the clear; news "Repo crews in
  HaHaland"; Talk topic *the repo men*.
* **Platforms.** PS1, Dreamcast. GBA: recap card.

---------------------------------------------------------------------------------------------------

### Notice 4. The Interest Engine

* **Place.** NG+ Chapter 3, after the first nest. Under the invaded plaza. Rivet (her mother's pipes).
* **Pitch.** The Bureau has bolted a cathedral of gears and counters onto the glum pipes: the Interest
  Engine, compounding the town's debt every second, its tally board the size of a church. **Accountancy
  as industrial horror** ("Compounding Daily! Hourly! Constantly!"), and Rivet at the welds: "That's
  Mum's pipe. They PAINTED over her mark. In BEIGE." The first Bureau monster that actually scares.
* **Set piece.** Shutting the engine down: the digits of the tally board (a rolling wall texture) spin
  backwards across the whole hall while the gears grind to a stop and Rivet hits the main shaft with a
  wrench.
* **Layout.** 44 x 44 industrial, 0 to +32: the counting floor, gear pits, the pressure tower, the tally
  board hall.
* **Enemies.** **Interest** (splitter), **Compounders** (brute variant), **Late Fees**, **Gear
  Gargoyles**.
* **Mechanic: doubling.** An Interest left alive splits in two every 20 s (at most 12 alive); a hall
  counter shows the debt climbing; clearing a hall resets it.
  *Card line:* "Interest doubles every 20 seconds. Clear it fast."
* **Boss.** **COMPOUND**, "The Interest, Fully Accrued" (slime belcher x2.8): splits at 50% into two and
  at 25% again; kill them all within 30 s or they merge back at full. Taunt "I grow while you sleep."
  Death toast "Compound is paid down to nothing. It looks almost relieved."
* **Secrets / collectibles.** 3 secrets; **8 of Anvil's hammer marks** to scrape clean (a Rivet line
  each).
* **Rank goals.** Par 3:50, gold 2:40. Feats: Never more than 4 Interest alive, No damage, All 8 marks.
* **Rewards.** Weld paint "Anvil's mark"; furniture "Gear clock"; Rivet and Anvil +friendship.
* **Music.** SONG_INVASION over a ticking click track.
* **Life sim / Talk.** News "The debt clock in the plaza stops for one night"; Talk topic *the engine*.
* **Platforms.** PS1, Dreamcast. GBA: recap card.

---------------------------------------------------------------------------------------------------

### Notice 5. Lake of Fire Regatta

* **Place.** NG+ Chapter 4, after Ep 4.2. Ember (co-op) pilots and cooks; Gerald (in the lava, fine);
  Hellvile's fishers.
* **Pitch.** A lien on the Lake of Fire: the Bureau's dredgers are draining it to sell the lava by the
  barrel, and the fishers race them in a regatta. A **sports-broadcast parody** with a two-imp commentary
  booth ("And the Hillvile kid is going the WRONG WAY. Bold. Stupid. Bold."). Ember cooks fish on the lava
  as you sail and tells you about the lake when she was small; the cooking that was "too warm" for
  Hellvile is exactly what the fishers want.
* **Set piece.** The dam break: the boss's death bursts the dredgers' dam and the lake refills in one
  wave you surf back to the start past the whole cheering regatta.
* **Layout.** 56 x 56 open lake (hell sky): islands, the dredger platform, the narrows (rapids), the dam.
* **Enemies.** **Dredgers** (brutes with buckets), **Lava Lurkers** (slime belchers that submerge, AI
  *swimmer*), **Buoy Imps**, Repo boats (prop billboards crewed by Repo Men).
* **Mechanic: the raft.** You stand on a raft that drifts with the lava currents (paths) and steer it with
  the stick; shoot from it, hop rafts; capsizing puts you back at the last buoy.
  *Card line:* "Steer the raft with the stick. Don't fall in."
* **Boss.** **LEVIATHAN LIEN**, "A Legal Claim With Teeth" (slime belcher x3.0 in the lava): surfaces and
  submerges, raises lava waves; anchor at buoys to ride them out. Taunt "This lake is COLLATERAL." Death
  toast "The Lien is lifted. So is a lot of lava."
* **Secrets / collectibles.** 3 secrets; **6 rare lava fish** (to your fishing log).
* **Rank goals.** Par 4:20, gold 3:00. Feats: Never capsize, No damage, All 6 fish.
* **Rewards.** New creator set **Regatta** (sailor cap, life vest, oar back-piece, Ember's apron); a new
  lava fish and rod skin (Fish & Cook); Ember's co-op quests gain a fifth part, "Lake of Fire".
* **Music.** SONG_HELLVILE as a fast regatta arrangement.
* **Life sim / Talk.** The new fish in Fish & Cook; news "Regatta saves the lake"; Talk topic *regatta*.
* **Platforms.** PS1, Dreamcast. GBA: recap card.

---------------------------------------------------------------------------------------------------

### Notice 6. Everything Must Go

* **Place.** NG+ Chapter 4, after Ep 4.5. The Pawn Palace in the Underbelly market. Grumbleguts (on the
  block), Vex, the Duchess (first scene in person in the present).
* **Pitch.** A clearance sale of everything the Bureau took: laughs in jars, the bakery oven, Hattie's
  best joke, the Mayor's hat, and Grumbleguts ("Start the bidding high, sugar."). A **department-store
  parody**: escalators, perfume counters spraying sulfur, mannequins that are demons, the PA ("Clean-up on
  aisle six. It's a soul."). The Duchess presides and bills Grumbleguts per wink. You buy back what
  matters with your own H$, and you cannot afford everything: the arc's first real weight.
* **Set piece.** The live auction: bid by shooting paddles while the Duchess presides from a balcony;
  every "SOLD!" shakes the store.
* **Layout.** 40 x 40, four floors (0 / 12 / 24 / 36) with escalators (conveyors, lifts), an atrium under a
  skylight, the auction hall on top.
* **Enemies.** **Mannequins** (bone jester variant: they hold poses between attacks, throw price guns),
  **Clearance Imps**, **Security Brutes**, **Red Tape** on the escalators.
* **Mechanic: buy-back.** Five display cases hold repossessed joys; A shows the price in your real H$;
  buying one returns it now (in the level: a weapon back or a route opened; in town: that resident's
  joy). There is money for about three. The rest stay held until the ending.
  *Card line:* "Buy back what was taken (A). You can't afford it all."
* **Boss.** **THE AUCTIONEER**, "Going Once, Going Twice, Going for Your Throat" (bone jester x2.2): a gavel
  shockwave on every third "SOLD!", bids call adds. Pacifist route: outbid him (a big H$ sum). Death toast
  "SOLD. To nobody."
* **Secrets / collectibles.** 3 secrets; **6 price tags** (discounts in the town's shops).
* **Rank goals.** Par 3:40, gold 2:30. Feats: Buy back 3 joys, No damage, Outbid the Auctioneer
  (pacifist).
* **Rewards.** One rare furniture piece from the sale; Grumbleguts +friendship; Collections Couture pieces
  go on sale at the Pawn Palace afterwards.
* **Music.** SONG_SHOP as mall muzak, then its hell remix.
* **Life sim / Talk.** A money sink with consequences; residents' moods; news "Pawn Palace sale:
  everything 90% off your soul"; Talk topic *the auction*.
* **Platforms.** PS1, Dreamcast. GBA: recap card (the buy-back as a shop list).

---------------------------------------------------------------------------------------------------

### Notice 7. The Audit (midpoint)

* **Place.** NG+ Chapter 5 "Arrears" opens the morning after the replayed festival (day 17), once Notices
  1-6 are cleared: the bill arrives. The Audit Hall rises out of the bandstand. Signhere (counsel), the
  Mayor, you.
* **Pitch.** A courtroom-accounting-office the size of a cathedral: scales the height of a tower weigh
  HaHaland's laughs against ten years of glums, an audience of Bureau clerks with calculators. A
  **legal-drama parody**: Signhere is the town's counsel ("I've read all 900 pages. Twice. For fun."),
  objections are buttons. Then the turn: page 412. In the main game everyone tore one page; you tore the
  breach clause, and in Hell tearing a contract is signing it. The happy ending was a loan, and you
  co-signed it. Payment begins now.
* **Set piece.** The scales tipping with you on a pan; page 412 projected across the vault; the Mayor
  filed (a soft stamp, then silence); the Duchess's first duel, which you cannot win; the bandstand goes
  dark.
* **Layout.** 32 x 40, compact and roofed (a 320-unit vault): the scales in the centre, galleries, the
  bench.

```
  ################
  #  BENCH   D   #   D  the Duchess at the bench (first duel, 90 s)
  #==gallery=====#   galleries (fh 12) with Auditors
  #  [pan]  [pan]#   the scales: stand stunned debts on a pan to tip it
  #     \SCALE/  #
  #      |  |    #
  #==gallery=====#
  #  floor   s1  #   s1 the evidence locker (Exhibit E)
  #  S  doors    #   S  start (from the bandstand)
  ################
```

* **Enemies.** **Auditors**, **Bailiffs**, **Late Fees**.
* **Mechanic: remix, pacifist.** Your weapon fires "Objection!" (stun only); debts cannot be killed, and
  every hit you take is "billed" (damage). Move stunned debts onto the scale pans to tip them.
  *Card line:* "Debts can't be killed. Objection! (fire) stuns. Tip the scales."
* **Boss.** **DUCHESS ARREARS**, first encounter: survive 90 s while Signhere argues; she cannot be
  harmed; the scripted end.
* **Secrets / collectibles.** 2 secrets; **Exhibits A-E** (Signhere uses them in the finale).
* **Rank goals.** Par 3:00, gold 2:15. Feats: Never billed (no damage), All 5 exhibits, A 10-Objection
  combo.
* **Rewards.** First Collections Couture piece (the red-envelope hat); weld charm "Objection!".
* **Music.** SONG_LEDGER in full (orchestral metal); silence at page 412.
* **Life sim / Talk.** The Mayor is held through Chapter 5 (news "Mayor Giggs taken into Collections");
  residents' moods; Talk topic *the Mayor*.
* **Platforms.** All, **GBA included**. GBA: the scales are two big billboards, the Duchess one (scale
  capped), the audience a wall texture, at most 6 debts awake.

---------------------------------------------------------------------------------------------------

### Notice 8. The Debtors' Prison (dark level)

* **Place.** Chapter 5, after the Audit. The prison under Hellvile where the labourers slept for ten years
  now holds Hillvile's signatories: the Mayor and twelve clerks who filed the plumbing order (adult
  residents of your town with office jobs). Tallyworn walks you in.
* **Pitch.** **The cost of the town's deals, learned by sitting in it, played straight.** The cells are
  the same bunks. Tallyworn has the chalk out again and does not use it. The Mayor's cell has "Hee. Hee."
  scratched in the wall, crossed out. A deadpan Bailiff apologises at every door ("Sorry. Sorry. Policy.").
  One master key: free the Mayor, or the twelve clerks whose families wait at the Wall.
* **Set piece.** The cascade: once the key turns, the block's doors open one after another and the alarm
  floods the yard red; you fight your way out with the freed walking behind you.
* **Layout.** 40 x 40: three tiers of cells round a central yard (open red sky), the bunk galleries, the
  infirmary, the Warden's office.
* **Enemies.** **Wardens** (sleepers), **Bailiffs**, **Chain Wraiths** (vape wraith variant), **Turnkeys**
  (cyber piggy variant).
* **Mechanic: stealth.** A noise meter: loud weapons, sprinting and doors fill it; at full the block
  wakes (Wardens rise, gates lock, Bailiffs come). Suppressed weapons, melee and the bubble wand are
  silent.
  *Card line:* "Stay quiet. Loud shots wake the block. Suppressed and melee are silent."
* **Boss.** **WARDEN WITHHOLD**, "Keeper of Keys, Withholder of Everything" (brute x2.0): his alarms lock the
  arena; shoot the keys off his belt. Taunt "Payment plans start at life." Death toast "Withhold lets go
  of the keys. It's the first thing he's ever let go of."
* **Choice.** The Mayor or the clerks (4.7).
* **Secrets / collectibles.** 3 secrets; **12 of Tallyworn's old tally marks** under the paint.
* **Rank goals.** Par 4:00, gold 2:45. Feats: Never raise the alarm, No damage, All 12 tallies.
* **Rewards.** Wall Crew pieces; furniture "Tallyworn's last chalk".
* **Music.** None (ambient); SONG_HELL_LULLABY as metal for the boss.
* **Life sim / Talk.** Freed residents come home; the others stay held; news; Talk.
* **Content note** (section 6.3).
* **Platforms.** PS1, Dreamcast. GBA: recap card with the choice.

---------------------------------------------------------------------------------------------------

### Notice 9. The Artist's Studio (dark level)

* **Place.** Chapter 5. The Bureau's oldest account is a soul: the artist whose tribute became
  Bonejangles. Under the Boneyard Buffet's walk-in fridge (HB13's "incident") is the room the restaurant
  was built over. Bonejangles, Fuzz, Nox. Coordinated with W15 (the origin reveal is W15's; this level is
  where it is said sincerely).
* **Pitch.** **The Walten rot played straight** (owner, 2026-11-08). It is not funny down here:
  canvases of skulls painted with love, a half-finished portrait of a skeleton playing a xylophone, the
  cot where Fuzz knelt thirty years ago when the hug did not take. The possession went into the machine;
  the soul went into the Bureau's books. Bonejangles comes along and, for once, does not tell it as a
  joke ("Clack," quietly). The studio's tapes are plain, sad footage (nothing hidden, nothing to decode).
  HB9's sealed drawer is opened here. Fuzz says it out loud: the rite was his; it failed; he never told
  Bonejangles. One bleak laugh: Bonejangles' warranty card, filed with the soul: "Void if possessed."
* **Set piece.** The rite finished: Fuzz forwards, Nox backwards (Ep 4.3's two-faith rite) in the studio
  while the Tribute tears the canvases; when it ends, the half-finished portrait finishes itself.
* **Layout.** 32 x 36, roofed: the fridge door, the stair down, the gallery of canvases, the studio of
  easels, the chapel of the rite (a font), the Bureau's bindery.
* **Enemies.** **Tribute Puppets** (Party Pal variant: rotten animatronic frames; body horror on machines
  and demons only), **Paint Wraiths**, **Easel Gargoyles**, a brute called **the Critic**.
* **Mechanic: remix, ghost-only.** The Bureau takes your body at the studio door as collateral. You are a
  ghost: no weapon, see-through, able to pass soft cells (canvas walls). "Boo" (fire) stuns; A on a
  stunned demon possesses it for 20 s (its attack, its body).
  *Card line:* "No body, no gun. Boo (fire) stuns; A on a stunned demon possesses it."
* **Boss.** **THE TRIBUTE**, "Built to Remember Him. Remembered Wrong." (Party Pal x2.6: its strings yank
  possessed demons out of your control); finished by the rite (the exorcism's prayer rhythm, forwards and
  backwards, inside the level). Death toast "The Tribute sits down at the easel. It doesn't get up."
* **Care note.** The artist died of old age at the easel; no line or image says or suggests otherwise.
* **Secrets / collectibles.** 3 secrets; **6 sketches**, each captioned by Bonejangles, sincerely.
* **Rank goals.** Par 4:10, gold 2:50. Feats: Possess 5 kinds of demon, No damage, All 6 sketches.
* **Rewards.** The last Party Pals pieces; furniture "Portrait of Bonejangles (finished)"; an E09 hint if
  that ending is still open.
* **Music.** SONG_THIRTEEN_BELLS; the rite on SONG_EXORCISM's two channels.
* **Life sim / Talk.** Bonejangles' Talk topic *origin* turns sincere after this; Fuzz's too; news.
* **Content note** (section 6.3).
* **Platforms.** PS1, Dreamcast. GBA: recap card.

---------------------------------------------------------------------------------------------------

### Notice 10. The Ward of Forgotten Names (dark level)

* **Place.** Chapter 5. The Bureau repossesses names: until this level is cleared, the island and the
  news show "???" for residents' names and Talk residents cannot name each other. Dr. Bloom, Grampy Bean
  (he has forgotten before), Rook (the log), Sootkin.
* **Pitch.** **Identity, isolation and dread, played straight.** Rows of beds with blank tags. Grampy Bean
  is the calmest person here ("I've been here. The tea's bad."). Rooms you do not name dissolve behind
  you. Rook's log, written every night for ten years and never read, has everyone's name in it. Bleak
  laugh: the Bureau's name-tag printer is out of toner.
* **Set piece.** The roll call: at the ward's centre you read Rook's log aloud and the dissolved wing
  rebuilds room by room around you, every bed's tag filling in with a resident's name from your town.
* **Layout.** 36 x 36, roofed, white and grey: the wards, the records room, the waiting room (clocks
  stopped), the courtyard with Sootkin's sunflower.
* **Enemies.** **The Blanks** (faceless vape wraiths), **Orderlies** (brute variant), **Static Implings**
  (TV-static visors).
* **Mechanic: naming.** Unnamed rooms fade to fog walls and collapse a while after you leave them; name
  tags and Rook's log pages let you say a room's name at its plaque (A), and a named room stays.
  *Card line:* "Unnamed rooms fade. Say their names at the plaques (A) to keep them."
* **Boss.** **THE BLANK**, "No Name, No Face, No Hurry" (vape wraith x2.4): erases a room each phase;
  restore it by calling names from the log. Death toast "The Blank gets a name tag. It reads 'HELLO MY
  NAME IS'. It's a start."
* **Secrets / collectibles.** 3 secrets; **12 log pages**.
* **Rank goals.** Par 3:50, gold 2:40. Feats: Lose no room, No damage, All 12 pages.
* **Rewards.** Rook's co-op quest finale; Elders pieces (reading glasses on a chain); furniture "Rook's
  log".
* **Music.** SONG_SAD; SONG_OMINOUS for the boss.
* **Life sim / Talk.** Names come back (news "Everyone remembers their neighbour's name. Mostly."); Talk.
* **Content note** (section 6.3).
* **Platforms.** PS1, Dreamcast. GBA: recap card.

---------------------------------------------------------------------------------------------------

### Notice 11. The Hollow Mingle (dark level)

* **Place.** Chapter 5. The Bureau forecloses on Hellvile Park and rewinds it to the night it was lost,
  frozen at "Close the park." You escort the Mayor (if freed in Notice 8) or Mother Nox ("I've waited ten
  years. I can walk across a park."). Lady Smog, young Fuzz's memory, Mascotty.
* **Pitch.** **Abandonment and regret, played straight.** The lanterns are dead, both sides' tables set
  and empty, the bandstand turned into the Bureau's foreclosure block. At each memory along the way the
  one you escort stops and says one true thing. At the bandstand the Duchess offers a deal: one name, any
  adult in town, and the Interest stops chasing you. ("Minors? No. The Bureau does not hold minors'
  accounts. Policy.")
* **Set piece.** The frozen Mingle: the purple cloud of eleven years ago hangs motionless over the park;
  when the two bowls are set it moves for the first time in eleven years.
* **Layout.** 48 x 40, open night park (stars, very dark): the two sides' tents, the dead lantern lines,
  the bandstand.
* **Enemies.** **Foreclosers** (snatchers), **Lantern Wraiths**, **Late Fees**, **Bailiffs**.
* **Mechanic: escort.** They walk at their own pace, stop at memories, and can be grabbed by a
  Forecloser: shoot it within 6 s. They talk about the Mingle as they go.
  *Card line:* "Keep them safe. If one is grabbed, shoot the grabber fast."
* **Boss.** **THE FORECLOSER** (gargoyle x2.2) and a 60 s stand at the bandstand while the bowls are set;
  then the deal.
* **Choice.** Give a name (pick an adult resident; they are held) or refuse (4.7).
* **Secrets / collectibles.** 2 secrets; **8 unlit Mingle lanterns** (lit in Notice 15).
* **Rank goals.** Par 4:30, gold 3:10. Feats: Never grabbed, No damage, All 8 lanterns.
* **Rewards.** First Mingle Lanterns pieces; furniture "Two bowls".
* **Music.** SONG_MINGLE slowed and broken.
* **Life sim / Talk.** The named resident (if any) is held; news; Talk.
* **Content note** (section 6.3).
* **Platforms.** PS1, Dreamcast. GBA: recap card with the choice.

---------------------------------------------------------------------------------------------------

### Notice 12. The Interest

* **Place.** Chapter 5: the descent into Accounts Receivable. Mascotty (her head went back on in Ch5),
  Thumpkin, Vex with the jar.
* **Pitch.** You run. The Interest is ten years of everything the town did not feel plus everything since,
  the size of a building; the Wall is being rebuilt behind you out of overdue notices. Pure dread as an
  action set piece, with one laugh: Vex running beside you with the jar over his head, "I'm NOT dropping
  him. I've dropped him ONCE." At the bottom Mascotty takes her head off again: "I'm not going back in a
  box."
* **Set piece.** The Wall rising across your path brick by brick while Mascotty's thumps show the gap; the
  ink bridge collapsing section by section; the Interest's face filling the screen behind you.
* **Layout.** 24 x 64, a long descent from 0 to -120: ramps and drops, the rebuilt Wall corridor, the ink
  bridge.
* **Enemies.** **The Interest** (the pursuer, unkillable), **Late Fees** in the way, **Bricklayers**
  (brutes; shoot them to stop a wall).
* **Mechanic: remix, one-hit chase.** An unkillable pursuer follows the path behind you (faster if you
  dawdle, a little slower if you fall far behind); floors collapse into ink after you pass; one hit from
  anything puts you back at the last checkpoint (one every ~20 s). If you gave a name in Notice 11 it is
  not one-hit, and the card says why.
  *Card line:* "Run. One hit and you're back at the last checkpoint." (or, after the deal: "Run. Someone
  paid so you can take a hit.")
* **Boss.** The Interest itself: the chase's climax across the ink bridge (no fight).
* **Secrets / collectibles.** 1 secret; **5 name bricks** (Mascotty's thumps lead to them).
* **Rank goals.** Par 2:40, gold 1:50. Feats: No checkpoint used, Mascotty's route, All 5 bricks.
* **Rewards.** Mascotty +friendship; a Wall Crew piece.
* **Music.** New **SONG_ARREARS** (the NG+ boss and chase theme).
* **Platforms.** PS1, Dreamcast. GBA: recap card.

---------------------------------------------------------------------------------------------------

### Notice 13. The Vault of Unfelt Things (dark level; the darkest)

* **Place.** Chapter 5: the bottom of the Bureau, where ten years of the town's unfelt things are stored
  as collateral. Vex (opens the boxes, as he opened the bottles), Wick (carries the light), the pet (its
  leftover breath is on file), Steamy, Hattie and Haze, Lady Smog, Nana (each at their own box).
* **Pitch.** **Grief and loss**, the darkest and quietest level in the game. Shelves to the horizon, each
  box a feeling someone did not feel: "Nana: grief, Charlotte. Unopened." "Pip: scared of the dark.
  (Filed under: normal, actually.)" (children's boxes are gentle and never harmful). The memorial wall
  carries the names of residents who have died in your own town (adults, elders and pets only; optional,
  6.4) and the grief nobody had time for. Wick carries the light. A vault door cannot be shot open: it
  opens only if you stand still and feel what is in it. Bleak laugh: the ticket machine ("Now serving:
  everyone").
* **Set piece.** The memorial wall: the names of your town's dead light one by one to a music-box phrase,
  and the boxes on the shelves open by themselves in a wave toward the vault's heart.
* **Layout.** 44 x 44 vault (dark, cold blue; tall shelving, narrow aisles): the stacks, the memorial
  chamber, the pet's file room, the heart.
* **Enemies.** **Collateral** (body-horror demons of compressed glum: brute and slime variants, demon
  flesh only), **Clerks**, **the Unfelt** (wraith variant that copies your last weapon).
* **Mechanic: feel.** Stand in a vault door's circle and hold A for the memory's length (5-12 s) while it
  plays; moving or being hit resets it. Demons close in: clear the room first, or feel in bursts.
  *Card line:* "Vault doors open if you stand still and hold A. Clear the room first."
* **Boss.** **THE COLLATERAL**, "Everything You Put Up" (a brute-scale body-horror demon assembled from the
  town's unfelt things; no human or child imagery; it absorbs adds to heal). Pacifist route: feel the four
  pillars round it and it comes apart into letters. Death toast "It comes apart into a thousand small
  boxes. Some of them are open now."
* **Secrets / collectibles.** 2 secrets; **10 boxes** to open (each returns one resident's feeling).
* **Rank goals.** Par 4:30, gold 3:10. Feats: Feel every memory without being interrupted, No damage,
  Pacifist boss.
* **Rewards.** Weld charm "Horned heart"; furniture "Memorial lantern" (a lamp that counts for LIT);
  Wick's co-op quest part.
* **Music.** None; a single phrase of SONG_WALL_LULLABY at each vault door; the boss runs SONG_SAD into
  SONG_ARREARS.
* **Life sim / Talk.** The graveyard gets a lantern per name; residents' moods; Talk (each resident's
  returned feeling).
* **Content note** (section 6.3; with the memorial-names option).
* **Platforms.** PS1, Dreamcast. GBA: recap card.

---------------------------------------------------------------------------------------------------

### Notice 14. The Ledger (FINAL NOTICE)

* **Place.** Chapter 5's climax. Everyone comes.
* **Pitch.** The Bureau's tower is a book the size of a mountain and every page is a room; the Duchess
  holds the town's account open at the last page. The main finale turned past eleven: the whole cast
  storms the Ledger page by page. Ember's field kitchen feeds the front line under fire; Rook reads his
  log to strike out false entries; Grumbleguts belly-bumps Bailiffs off the margins ("Put it on my TAB."
  "You ARE the tab."); Rivet and Anvil weld the vault shut; Fuzz and Nox sing the rite forwards and
  backwards; Signhere finds the clause ("Payable in KIND. Feelings, not joy!"); Tallyworn counts the
  balance down to zero and draws a heart; Vex asks the Duchess who ever listened to her. Then the Duchess
  herself, in three phases, and the Great Rain.
* **Set pieces.** The march up the book's spine under red-ink rain; every page turn a camera sweep as the
  next arena rises; phase 2's ink colossus filling the sky; **the Great Rain**: Fluffy, cupcake-sized,
  rains for real over the Ledger and both towns on every resident of your town (a crowd shot), and four
  themes resolve into one chord; the pet's beep; her flinch; the fourth player line.
* **Layout.** 64 x 40: seven page arenas of 16 x 16 joined by the spine bridge; ink sky (a new sky).

```
  (1 char = 2 cells)
  ################################
  #P1 tubes #P2 repo  #P3 interest#   each page: a 16 x 16 arena with one Arrears rule
  #  ()  () #  [cage] #   ~~ ~~   #
  ####||########||########||#######
  #==== the spine bridge (fh 24) ======== S
  ####||########||########||#######
  #P4 noise #P5 ghost #P6 names #P7  the last page: the Duchess,
  #  zzz    #  boo    # [plaque] # J  J = the jar to protect in the rain
  ################################
```

* **Enemies.** Every Bureau type, with elites.
* **Mechanic: page-flip arenas.** Each page brings back one Arrears rule, named at the top of the screen:
  P1 tubes (N2), P2 repo hooks (N3), P3 Interest (N4), P4 noise (N8), P5 ghost (N9), P6 naming (N10); P7,
  the last page, is a stand: protect the jar for 90 s in the rain.
  *Card line:* "Each page has its own rule (shown at the top). Last page: protect the jar."
* **Boss.** **DUCHESS ARREARS**: phase 1 "Head of Collections" (her office: a bullet-hell of overdue
  notices); phase 2 "Compounded" (merged with the Interest into an ink colossus); phase 3 "The Great Rain"
  (she cannot be hurt; protect the jar while the rain washes her ledger blank). The pet beeps; she
  flinches. The fourth player line. The choice (4.7, 4.8).
* **Rank goals.** Par 9:00, gold 6:30. Feats: No page failed, No damage on the last page, Nobody downed.
* **Rewards.** Collections Couture complete (her gown-cut suit, the envelope hat, the chained glasses);
  title-screen variant per ending; SONG_GREAT_RAIN in the jukebox; household tier 2 (4.8).
* **Music.** SONG_ARREARS, then the new **SONG_GREAT_RAIN** (the four themes in one chord).
* **Platforms.** All, **GBA included**. GBA: each page is a walled 16 x 16 arena with short sightlines;
  the colossus is one billboard (scale capped at 2.5x) with an ink tint at level 2; the crowd and the
  rain-over-both-towns shots are 2D story panels (the cutscene engine), not billboards; at most 6 adds.

---------------------------------------------------------------------------------------------------

### Notice 15. Both Are Better (PAID IN FULL; the epilogue)

* **Place.** After the arc's ending card: the first real Mingle since the park closed. No combat.
* **Pitch.** The park at dusk, both sides' tables pushed together, two bowls and a cupcake. You walk the
  festival with nothing to shoot and light the Mingle lanterns while residents release sky lanterns, one
  for each thing they finally felt. **Wick lights the first one themselves**, a small one. Pip and Wisp
  float paper boats with IOUs that turn into "I owe you nothing". Who is there follows the ending: under
  E03 the Duchess sits at the bookkeeping table learning to laugh on the wrong beat while Hattie coaches
  her; under E05 the held residents' chairs stand empty with name cards; under E07 the suitcase frame is
  empty, and at the end the town takes a new photo with you in it.
* **Set piece.** The sky filling with lanterns over both towns while the Mingle music box plays in full
  for the first time in the game.
* **Layout.** 48 x 48, the Mingle grounds at dusk: the bandstand, the pushed-together tables, lantern
  lines, the Lake of Fire's shore, the paper-boat stream.
* **Mechanic: sky lanterns.** Lanterns rise from the crowd; your shots (the confetti cannon, cute style
  forced) light them; light every lantern.
  *Card line:* "Nothing to fight tonight. Light the lanterns (fire)."
* **Boss.** None.
* **Collectibles.** **12 polaroids**, one per cast pair; the last goes into the suitcase frame (E2).
* **Rank goals.** No par. Feats: Every lantern, Every polaroid, The last dance (stand at the bandstand when
  the song ends).
* **Rewards.** Mingle Lanterns set complete; furniture "Mingle lantern"; the title screen's final variant.
* **Music.** SONG_MINGLE in full, then SONG_FESTIVAL.
* **Platforms.** PS1, Dreamcast. GBA: a story beat on the existing United Park overworld map (the
  polaroids as a slideshow).

---------------------------------------------------------------------------------------------------

## 6. Dark levels

Owner (2026-11-08): some levels are noticeably darker than the rest of the game, in mood **and** in
topic: about 3 of the 9 main levels and about 6 of the 15 NG+ levels, one or two of the NG+ ones near the
start as a cold open and the rest in the late-middle to the end. Each has a content note before it
starts. The hard limits of DECISIONS.md do not change.

### 6.1 The nine dark levels

| Level | Where | Topics | How it is sold | What is never shown |
|---|---|---|---|---|
| **HB9 The Undercroft** | main, Ch3 | guilt, failure, faith that tore, souls that could not be saved | ambient light 1 and one lantern; Fuzz's files and his empty cot; no music, only drips and bells; jokes: the Huggins shelf, the filing system | no harm to anyone but demons; lost souls are names on files, never bodies |
| **HB11 The Glum Pipes** | main, Ch3 | grief, things never said, the town's deal with Hell | purple sludge and low breathing; residents' unsaid lines in their own handwriting; a music box; jokes: inspection tags, "GLUMS OUT", "(it's fine.)" stamps | no self-harm in any unsaid line (the line pool is filtered); children's lines are only gentle ones |
| **HB12 The Wall** | main, Ch4 | abandonment, people kept by a contract, what the town's comfort cost | two faces of one wall, pastel and soot; bunks, chalk, photos facing the bricks; two lullabies crossfading; jokes: the maintenance log, the full suggestion box | fantasy labour only: no real-world uniforms, insignia, slogans or camp imagery |
| **N1 Day One** (cold open) | NG+, before P0 | abandonment, a contract that traps people | you are Tallyworn laying the last bricks; a ribbon-cutting heard through the wall; no music; jokes: the site sign, the pension line | as HB12 |
| **N8 The Debtors' Prison** | NG+ Ch5 | complicity, the cost of the town's deals, guilt; a choice that costs someone | the same bunks, now holding Hillvile's signatories; red alarm light; silence; jokes: the apologetic Bailiff, the crossed-out "Hee. Hee." | no violence to prisoners; the Bureau "files", it does not hurt |
| **N9 The Artist's Studio** | NG+ Ch5 | death, a failed exorcism, grief; the Walten rot played straight | canvases, the cot, plain sad tapes; Bonejangles without his jokes; bells; joke: the warranty card | the artist died of old age at the easel (stated plainly); body horror on demons and machines only; nothing to decode |
| **N10 The Ward of Forgotten Names** | NG+ Ch5 | forgetting, losing who you are, isolation, dread | white and grey ward, blank tags, rooms that dissolve, stopped clocks; jokes: Grampy's tea, the printer out of toner | no medical procedures shown; no named real illness |
| **N11 The Hollow Mingle** | NG+ Ch5 | abandonment, regret, the night the park closed; a choice that costs someone | a dead festival frozen at "Close the park"; the purple cloud of eleven years ago; a broken music box; joke: "The Bureau does not hold minors' accounts. Policy." | children are not at the frozen Mingle (the kids' tent is empty and closed); no child is ever offered or taken |
| **N13 The Vault of Unfelt Things** (the darkest) | NG+ Ch5 | grief and loss; the town's dead | cold blue vault, boxes of unfelt feelings, the memorial wall with your own town's dead, standing still to feel; one music-box phrase; joke: "Now serving: everyone" | children's boxes are gentle and harmless; the memorial lists adults, elders and pets only (the life sim never kills a child); no self-harm anywhere in the vault's text |

Counts: **3 of 9** main (HB9, HB11, HB12, all in Ch3-Ch4 where the story itself is darkest) and **6 of 15**
NG+ (Notice 1 as the cold open; Notices 8, 9, 10, 11 and 13 in the late-middle to the end). Notice 12
(the chase) is intense but an action set piece with jokes, so it is not tagged dark.

### 6.2 Rules for every dark level

* **Mood over jokes.** Ambient light 1-6, desaturated or cold palette ramps, quiet music or none, slower
  pacing (fewer demons, longer quiet rooms). Two to four bleak or deadpan laughs per level, placed by
  the script lint (a dark level whose text has none fails; one with more than four is flagged), so it is
  still the same game.
* **Environmental storytelling** (handwriting, files, bunks, boxes, tapes) is optional to read and never a
  puzzle or a requirement (the no-ARG rule).
* **Hard limits** exactly as DECISIONS.md: no sexual content in any dark level (the PG-21 raunch stays out
  of them entirely), gore only on demons, no harm shown to children, no real-world hate, self-harm and
  suicide never a topic, a mechanic or a joke. Text that touches death states it plainly and kindly.
* **Real residents' names** (HB9 files, HB11 letters, N8 clerks, N10 tags, N13 memorial) are used with
  respect: never as a punchline, never for a child's death or harm.
* **Reduce flashes** (`OPT_REDUCE_FLASH`) halves every scream shake, tint pulse and alarm flash, as the
  existing boss tints do.
* **The dark levels end lighter**: each finishes on a small warm beat (daylight through the culvert grate,
  the heart as the last tally, a portrait finished, a name tag that says "HELLO", the bowls set, some
  boxes open), so no level leaves the player at the bottom.

### 6.3 Content notes

Shown on the level's control card as its first page, before the controls: the first visit waits for A
(B goes back to the select, so the player can choose not to play it); later visits show it as one line
above the goal. The level select marks dark levels with a small moon badge and the same note in the
card's footer. Text (at most three lines at 240x160):

| Level | Content note |
|---|---|
| HB9 The Undercroft | "Content note: this level is darker than most of the game. It deals with guilt, failure and souls that could not be saved." |
| HB11 The Glum Pipes | "Content note: a quieter, sadder level about grief and the things people never said." |
| HB12 The Wall | "Content note: this level is about people trapped by a contract and left behind by the town they worked for." |
| N1 Day One | "Content note: New Game+ starts darker. This level is about workers trapped by a contract and left behind." |
| N8 The Debtors' Prison | "Content note: imprisonment, guilt, and a hard choice that costs someone." |
| N9 The Artist's Studio | "Content note: death, a failed exorcism and grief, with disturbing images of demons and broken machines." |
| N10 The Ward of Forgotten Names | "Content note: forgetting, losing who you are, and being alone." |
| N11 The Hollow Mingle | "Content note: abandonment and regret, and a choice that costs someone in your town." |
| N13 The Vault of Unfelt Things | "Content note: the darkest level in the game. Grief and loss; it shows the names of residents who have died in your town (Options can hide them)." |

### 6.4 Options

* **Content notes** (Game page): "Every time" (default) / "First time only". The first visit always shows
  the note.
* **Show memorial names** (Game page, appears once Notice 13 is reachable): On (default) / Off. Off
  replaces the names on the memorial wall with "Someone we lost." (owner question 3).

---------------------------------------------------------------------------------------------------

## 7. The GBA's 7 levels

### 7.1 The pick

| Level | Why it fits the GBA | What it brings |
|---|---|---|
| **HB8 Steam Room Panic** | 40 x 34, roofed but for an 8 x 8 rooftop; rooms of 8-12 cells; the fog is just darker cells and a short view distance (cheaper than a normal room) | the Chapter 2 level, reachable early; the gentlest of the 24 |
| **HB9 The Undercroft** | 34 x 34, all roofed, ceilings 10-14, ambient 1 (dark pixels cost the same, but nothing far is visible: a short view distance); the lantern only rewrites up to 25 light bytes | a main-game dark level on the GBA |
| **HB13 Boneyard Buffet & Bandstand** | 40 x 36, all roofed, booths and short sightlines; showtime only rewrites the room's light bytes; frozen animatronics cost no AI | the Walten Files parody, the owner's main parody target |
| **HB15 The Department of Contracts** | 40 x 40, low ceilings, cubicles; stamps and queues are doors and timers | the office comedy; Signhere |
| **N1 Day One** | 36 x 30, mostly roofed shell and tunnels; the only sky is a 3-cell walkway between tall parapets | the NG+ cold open (and the second GBA dark level) |
| **N7 The Audit** | 32 x 40 compact hall; pacifist (no player kills: fewer gibs, effects and corpses); few big billboards | the NG+ midpoint |
| **N14 The Ledger** | seven walled 16 x 16 arenas; the spectacle is boss scale, tint and 2D story panels, not vistas | the NG+ finale |

The other 17 are out because of what they are: open sky over wide spaces (Fun Fair, Highrise, Ninth Cloud,
Repo Night, Regatta, the Hollow Mingle, Both Are Better), long corridor or basin views (the Wall, the Glum
Pipes, the Interest), many moving cells at once (the tide, rides, rafts, collapses), or many simultaneous
awake demons (the Interest Engine's doubling, the Pawn Palace's floors). Notices 2-6 and 8-13 reach the GBA
as recap cards (4.12), Notice 15 as an overworld beat; HB10, HB11, HB12, HB14 and HB16 are not on the GBA
and their side missions are compiled out there (`plat_features()`), so no GBA player sees an entrance to a
level that is not on the cartridge. GBA trophies count "n of 7" new levels.

### 7.2 GBA budgets (checked by `tools/levelcheck.py` for every map marked `gba yes`)

| Budget | Value | Why |
|---|---|---|
| map size | at most 40 x 40 | raycaster walks and the bot's BFS (4 KB work area) |
| roofed open cells | at least 75% | roofed cells close the column early |
| open sky | no sky region wider than 8 x 8 seen at once | blocks under the sky make rays walk on |
| view distance | per level, 12-16 cells (a header key; `R3D_MAXDIST` is 40) | cuts the column walk on every frame |
| awake demons | 8 (Balanced), 6 (Smooth), 10 (Max detail, governor-lowered) | billboards and AI were ~23 ms of the invaded frame |
| things | at most 120 per map, 48 actors live | `ACTOR_MAX` 128 is shared with shots and effects |
| textures per room | 4 dominant | the VRAM texture cache holds 4 |
| tint | at most level 2 sustained (pulses higher, halved with Reduce flashes) | about 1 ms per tint level |
| translucent billboards | at most 2 on screen | checkerboard cost |
| boss billboard | scale at most 2.5x | sprite fill cost |
| per-level runtime state | at most 64 B in the `LvRun` union | EWRAM |
| ROM | at most 300 KB per level (map, script, text, level art) | the cart has ~27 MB free: not a limit, a tidiness rule |
| EWRAM | the whole expansion at most +256 B (a ledger row) | ~3.1 KB free |
| IWRAM | 0 B | ~96 B slack |

### 7.3 fps on the GBA

The week-2 audit measured GBA combat at 9.8 drawn fps and 16 logic ticks/s, and found that 30 drawn fps in
GBA combat is not reachable exactly; the speed wave lands X5 (combat billboards without `fx_dist`), G-C1
(the half-resolution governor) and G-C2 (G9: hardware 2x scaling, HUD and weapon as OBJs) and the combat
detail option. **The GBA levels start after those land** (LV9). Each GBA level is built cheaper than the
Vape Cellar (a 12-16 cell view instead of 40, fewer billboards, roofed); the target is steady 30 in every
mode, the gate is measured, and a level whose numbers miss 30 is reported with its gap per mode (owner
rule), never called done at a lower number. Logic must hold 30 ticks/s in every mode.

### 7.4 GBA-specific simplifications

| Level | GBA version |
|---|---|
| HB8 | fog = darker cells + short view distance (no steam billboards); at most 2 jet puffs visible; ducks 8 x 8 |
| HB9 | the lantern rewrites at most 25 cells' light bytes (base light recomputed from the ambient and lit candles, nothing stored); candle flames are 2-frame sprites; at most 6 shades awake |
| HB13 | showtime rewrites the room's light bytes and plays the stinger; at most 4 Party Pals awake; the ball pit is a floor texture |
| HB15 | at most 6 copier copies; webs are floor zones; fog at 14 cells over the partitions |
| N1 | the walkway between tall parapets with fog at 12 cells; the hod is a held-item sprite; at most 6 Glumlings |
| N7 | the scales are two big billboards, the audience a wall texture, the Duchess one billboard; at most 6 debts awake |
| N14 | 16 x 16 walled pages; the colossus is one billboard (2.5x) with an ink tint at level 2; the crowd and rain shots are 2D story panels; at most 6 adds |

---------------------------------------------------------------------------------------------------

## 8. Shared tech

### 8.1 Map format v2 (mapc, `map.h`)

The 8-byte cell, the cell flags and the thing flags are full, and the GBA raycaster compares cells as raw
8 bytes, so nothing new goes into them. Everything new is **side tables in ROM** hung off
`DoomoMapDef` (all optional: `NULL` / 0 for the 12 existing maps, whose generated files stay
byte-identical):

| mapc statement | Stored as | Used by |
|---|---|---|
| `zone X0 Y0 X1 Y1 KIND [arg=N] [tag=N]` | up to 32 rectangles: kind, arg, tag | fog, tide, low gravity, darkness, noise, face A / B, naming wards, chase lanes, safe rooms |
| `path ID x,y x,y ...` | up to 8 paths of up to 32 points | ziplines, ride cycles, raft currents, the escort's route, the chase pursuer, tube links |
| `swap NAME ... end` (a sparse grid block like `floorh`) | up to 16 named sets, at most 256 cell overrides in all | the Wall's faces, laid bricks, collapses, rebuilt walls, dissolving rooms, page turns, drained basins |
| `on EVENT do ACTION ...` | level-script bytecode, at most 2 KB (8.2) | every scripted moment and set piece |
| `texbank N TEXNAME` | up to 8 level-local textures on ids 56..63 | per-level art without using up the 64-id cap |
| `tflags2=...` on a thing | a parallel byte per thing | escort, collect, sleeper, splitter, frozen-at-start, usable kind |
| header keys `fog N`, `view N`, `gravity N`, `dark yes`, `note TEXTID`, `gba yes|no`, `detail a,b,c`, `hero LOOK LOADOUT` | a small level header | view distance, content note, platform manifest, governor drop order, hero override (Notice 1) |

mapc refuses: zones or paths outside the map, overlapping exclusive zones, swap overrides on walls the
level never opens, a script over budget, a `gba yes` map that breaks section 7.2, a `dark yes` map without
`note`. `test_maps` BFS learns the new edges: tube links, zipline and ride paths, raft currents, bounce-pad
arcs, swap-set states reachable by levers, laid-brick bridges, named-room persistence and stamp doors, so
every map's exit is proved reachable from its start in every state the script allows.

### 8.2 The level script VM (`core/src/lv_script.c`)

A small interpreter instead of per-level C code (smaller overlays on the PS1, data on the disc, testable
on the host). Events: a trigger tag, a kill-event group cleared, a timer, entering a zone, using a usable
thing, a variable reaching a value, a boss phase. Actions: open / close a remote tag, apply / revert a
swap set, set a zone parameter, spawn a dormant group, caption or bark (text id), music cue, tint pulse,
**camera sweep** (the select screen's orbit camera on a path for N ticks, input frozen: the set pieces),
set / add a variable (at most 32 byte variables in `LvRun`), give / take a weapon, checkpoint, offer a
choice (`ui_choice`, the NG+ choices), set a story flag (NG+ arc), end the level. Deterministic (the
session's rng), bounded (at most 16 actions per tick), and every action has a host test.

### 8.3 Mechanic kits

| Mechanic | Kit | State (bytes) | Renderer impact | Bot (autopilot) support |
|---|---|---:|---|---|
| fog zones + valves (HB8) | B | 8 | darker live cells + per-level view distance; PS1 / DC steam billboards | uses valves on its route |
| lantern + candles (HB9) | B | 12 | live light bytes near the player | refills oil, lights candles en route |
| ride cycles (HB10) | A | 16 | moving floor heights (as lifts) | waits for the platform, then boards |
| glum tide (HB11) | B | 8 | floor heights + flags of a zone, stepped | prefers valves before basins |
| two faces (HB12) | B | 4 | swap sets; out-of-phase actors translucent | BFS over both faces with lever edges |
| showtime (HB13) | C | 6 | the room's light bytes (live map) | holds fire during shows |
| ziplines (HB14) | A | 8 | none | path edges |
| stamps + queues (HB15) | C | 8 | none | fetches stamps, waits at counters |
| bounce pads + low gravity (HB16) | A | 4 | none | arc edges |
| two-handed carry (N1) | A | 6 | held-item sprite | carries bricks to marked gaps |
| tubes (N2) | A | 6 | a 12-tick fade | link edges, sets levers |
| repossession (N3) | C | 8 | none | breaks cages, re-equips |
| doubling (N4) | C | 4 | none | targets the oldest Interest first |
| raft (N5) | A | 12 | raft billboard under the player | follows the current path |
| buy-back (N6) | C | 4 | none | buys the cheapest case (test-only policy) |
| pacifist "Objection!" (N7) | C | 4 | none | stuns and pushes debts to the pans |
| stealth noise (N8) | C | 4 | noise meter HUD | uses the quiet weapon |
| ghost possession (N9) | C | 8 | translucent player, soft cells | stuns and possesses on a fixed plan |
| naming (N10) | B | 8 | swap sets (fog walls) | names rooms as it enters them |
| escort (N11) | C | 8 | NPC actor | stays within 3 cells, shoots grabbers |
| chase + collapse (N12) | B / C | 8 | swap sets behind the player | runs the lane, no detours |
| feel (N13) | C | 4 | circle decal | clears the room, then holds A |
| page arenas (N14) | C | 8 | per page | the page's own plan |
| sky lanterns (N15) | C | 4 | lantern billboards | aims at the nearest lantern |

All mechanic state sits in one union in `LvRun` (at most 64 B per level, 96 B with the script variables);
nothing is allocated. A mechanic whose bot support is missing blocks its level's tests, so each kit ships
its bot edges with it.

### 8.4 Enemies, AI and bosses

* **Variants first.** A `DemonSkin` table (base type, name, bestiary blurb, scale, palette ramp or tint,
  glow, flags, AI flags) makes almost every new enemy from the 10 existing demons: about 45 skins across
  the 24 levels, each with its own Library bestiary entry.
* **Four new base demons with art** (8 frames each, both gore styles): **Party Pal** (HB13, N9), **Late
  Fee** (Bureau clerk imp), **Repo Man** (tow-hook demon), and **Duchess Arrears** (a large boss set plus a
  cutscene portrait). New `THING_*` ids are appended after the props and `IS_DEMON` becomes two ranges.
* **Three generic thing types** instead of one per prop: `THING_COLLECT` (arg = collectible index),
  `THING_USE` (arg = valve, lever, booth, breaker, pulley, plaque, stamp desk, display case, rent box,
  candle, oil jar, tube station, cage), `THING_LPROP` (arg = a prop image in the level's bank).
* **New AI flags:** *freezer* (showtime), *splitter* (Interest), *thief* (steal and flee), *sleeper* (wake on
  noise), *stalker* (unkillable path follower), *flee-light* (avoids lit cells), *swimmer* (submerge and
  surface), *orbiter* (rides a path), *poser* (mannequins hold still between attacks).
* **Bosses.** `lv_boss` grows to 31 entries; `LevelBoss` gains a mechanic id (`LBM_*`: steam, darken,
  orbit, scream, rebuild, showtime, evict, seal, drift, tube, tow, split, wave, gavel, lockdown, puppet,
  erase, absorb, phases) beside today's flags. Boss Rush gets three sets ("Classic 7", "Breach 16",
  "Arrears"); Horde draws new mini-bosses from the main set.

### 8.5 Records and saves

* A new save-ext chunk **`BRCH`** (owner: levels): per new level 4 B (rank 3 bits, gold 1, feats 3, cleared
  1, secrets 8, collectibles 16) = 96 B for the 24; the arc's state about 48 B (act, 16 flag bits, the
  buy-back mask, the two choice bytes, up to 16 held resident ids, the previous run's last ending, the
  fourth player line); version byte; about 150 B in all (GBA: sized for its 7 levels, about 80 B).
  Checked against `save_budget` and the GBA slot-1 bound; a registry slot per the K0 rules
  (`SAVE_EXT_SLOTS` is 13: LV0 checks one is free or raises it, with the GBA snapshot's +2 B).
* The old 7 levels stay in `PROG_BREACH_RANKS` (no migration). A save without `BRCH` loads with nothing
  cleared.
* NG+ carry-over: `story_ng_plus_arm()` also carries the last ending (1 B in `ngp_`).

### 8.6 Unlocks, rewards, art and audio

* **Trophies, golds and feats** live in `BRCH` (the 1024 unlock bits are all assigned).
* **Creator rewards** through CR11's set unlocks in bank 2 (reserve 1408..1439 for the levels, agreed in
  CR11's registry). **Six new themed sets**: Bath House, Wall Crew, Office of the Damned (main); Regatta,
  Collections Couture, Mingle Lanterns (NG+), 5-8 parts each (~40 parts, drawn in the creator's art
  batches under CR4's age and content rules; nothing in them is adult-only). That makes 31 of the 32 sets
  the picture grid's chips allow until CR4 widens them. Existing reward sets get their unlock source here:
  Exorcist Kit (HB9), Bonejangles' Party Pals (HB13); others get extra pieces (Fun Fair, Sugar Rush
  Sewers, Hellvile Pits Punk, Cloud Nine, Elders).
* **Weld rewards** (charms Rubber Duck, Stamp, Objection!, Horned heart; underbarrel Tow hook; paint Anvil's
  mark) through the action range's spare ids 551..563 (6 of 13).
* **Furniture** rewards arrive as gifts in the house's storage (no unlock id).
* **Textures.** 15 ids are free under the 64 cap: 7 shared new textures on ids 49..55 (steam tile, pipe
  steel, office carpet, cubicle fabric, felt, ledger page, vault metal) and ids 56..63 reserved as **level
  slots** that each map's `texbank` fills from its own bank. **Skies:** +3 (OVERCAST for the dark levels,
  INK for the Ledger, DUSK_LANTERNS for the Mingle): `SKY_COUNT` 8.
* **Sprites.** The four new demons in the global table; props, usables and collectibles in per-level
  banks (at most 16 images a level) outside `DOOMO_SPR_MAX_IMAGES`.
* **Music.** Six new songs: SONG_SPA_BOSSA, SONG_BANDSTAND, SONG_FILING (main), SONG_LEDGER, SONG_ARREARS,
  SONG_GREAT_RAIN (NG+), jukebox ids from the story's spare 726..735. The dark levels reuse the quiet
  existing ones (Wall Lullaby, Hell Lullaby, Thirteen Bells, Sad, Ominous, the Mingle music box).
* **Sound effects.** About 12 new (valve, tube, stamp, showtime stinger, zipline, raft, tow hook, till,
  gavel, scream, page turn, rain) inside `AUDIO_SFX_BUDGET` (48 KB; measured first); on the PS1 in
  `SFX.BIN`.

### 8.7 The combat detail option in levels

Every level's header lists the effects the governor may drop, in order (`detail steam,rain,crowd,decals`).
**Smooth**: the speed path (GBA half resolution under load, fewer effects, the low awake cap). **Balanced**
(default): full resolution, the normal cap. **Max detail**: every listed effect plus extra particles,
decals and billboards where the platform has headroom, with the adaptive governor lowering detail only on
frames that would drop below 30 and raising it again. Per platform: GBA Smooth / Balanced / Max as section
7.2; PS1 Max adds steam, rain and crowd billboards and more decals; Dreamcast Max adds everything.

### 8.8 Per-platform delivery and budgets

| | GBA | PS1 | Dreamcast |
|---|---|---|---|
| levels | the 7 (a manifest in `core/data/maps/` picks what mapc emits for the GBA build) | all 24 | all 24 |
| where a level lives | ROM (maps, scripts, text, banks as const) | a **level pack** in `BREACH.BIN` on the disc: LZ cells decoded straight into `doomo_map` (as `packmaps` does today), things, zones, paths, swaps, script, text, texture bank, sprite bank | the same pack format on the GD-ROM |
| RAM | EWRAM at most +256 B for the whole expansion (one ledger row); IWRAM 0 | resident growth at most 2 KB; the pack's tables and text (at most 12 KB) in a level arena borrowed while the level runs; the combat + levels overlay group grows at most 8 KB (else the level select moves into its own group) | not a constraint (16 MB) |
| video memory | the 4-texture VRAM cache (4 dominant textures per room) | the level's texture bank pinned in the world-texture slots (at most 8 of 80); its sprite bank pinned in one 256 x 256 image-cache page | PVR texture cache, not a constraint |
| load | none (ROM) | at most 1 s of CD reads behind the intro card | GD-ROM read behind the intro card |
| art budget per level | at most 300 KB ROM | pack at most 96 KB on disc (about 2 MB for 24) | as the PS1 pack (an optional 2x texture bank later) |
| prerequisites | the speed wave (X5, G-C1, G-C2, combat detail option) | X5 and P-C1 (combat TP 28.1 -> 30) | **DC6** (`plat_view3d` through the `dc3d` batches) |

### 8.9 Story and NG+ engine work

* **Main levels:** side missions and Activities rows that unlock HB8-16 at their beats (`ACTF_STORY`,
  `min_chapter`, story flags), objectives that say where (C7 wayfinding, `story_wayfind.py`), rewards,
  news templates, Talk topics; `flow.lock` and the main `.dsc` beats untouched.
* **NG+:** the cold-open hook in `story_new_game()` (NG+ only, before P0); NG+ asides in the replayed
  chapters (envelopes, the eruption line); **Chapter 5 "Arrears"** (beats `BEAT_C5_*`, only with
  `SF_NGPLUS`, after the replayed credits; its own lock file); the arc's flags (from the 28 reserved story
  flag bits 228..255) and choices; held residents as an away reason (F9); the arc's endings (owner
  question 1); `ngp_` carrying the last ending; the fourth player line (`STRY.arrears_line`); title-screen
  variants; the household tier grant at the arc's card; the GBA recap cards; a new story pack `ngp` and
  the level strings, written to SCRIPT_STYLE.md and translated after the script is final (DECISIONS:
  script first, translate after).

### 8.10 Every level reachable and tested

For each of the 24 levels (the platform rows where they apply):

1. **mapc** `--check` and **`test_maps`**: the exit is reachable from the start in every state the
   script allows, with the new edges (8.1).
2. **`tools/levelcheck.py`** (new): sizes, things, awake caps, the GBA rules on `gba yes` maps, a content
   note on `dark yes` maps, the card line fits 240x160, every secret and collectible reachable.
3. **`core/tests/test_levels2.c`**: the autopilot (`LVF_DEMO`) finishes the level in every mode (Hell
   Breach, Time Trial, Cute-Only, Brutal-Only) at Plushie, Normal and Ultra-Hellvile within a tick cap;
   the mechanic's unit tests; ranks, feats and collectibles saved and reloaded through `BRCH`; the content
   note shows; **the unlock path works** (a story debug jump to the unlocking beat shows the level
   unlocked in the select and its Activities row present).
4. **A scripted playthrough**: `core/tests/scripts/levels2_<id>.txt` (the intro card, the control card
   and content note, first- and third-person shots in three rooms, the mechanic in action, the set piece,
   the boss intro, the results stamp), which the runner shoots at **240x160 and 320x240** like the
   existing `levels_*` scripts, compared with goldens.
5. **GBA harness** (the 7): `gba/harness/scripts/breach_<id>.txt`: `push BREACH (level | LVF_DEMO)`,
   `sync on`, three `fps` windows (130 ticks after 60 of settling: the start room, the busiest room, the
   boss) in each detail mode, `vhash-seq` for determinism, shots; `make -C gba test-breach2`; `expect-fps`
   floors from the first measurement (the speed tour's 85% rule) with 30 as the reported target; an EWRAM
   ledger row.
6. **PS1 harness** (all 24): `ps1/harness/scripts/breach_<id>.txt` on the game disc (`--exe ... --cue
   ...`), the same three windows per detail mode with the frame-gap histogram and shots; a CI step.
7. **Dreamcast harness** (all 24, once DC6 lands): `dc/harness/dcrun` scripts with the same windows and
   shots on Flycast, plus the host software-PVR comparison DC7 uses.
8. **Story**: `story-check`; the NG+ arc journey `ngplus_arc.txt` (`story_debug_step` with autowin from the
   cold open through Chapter 5 to each of the three arc endings); the choice-consequence tests; the
   life-sim hooks (held residents come home under E03 and stay under E05); the GBA recap-card path.
9. **fps table**: `tools/breach_fps.py` gathers every harness log into one table (platform x level x
   detail mode: drawn fps, logic ticks/s, worst frame) for each package's report; a level is done at a
   steady 30 or with its gap reported to the owner.

Scripts 4-7 are generated from one list by `tools/breach_scripts.py`, so a level cannot ship without all
of them; the 7 existing levels get the same generated scripts first (LV10) to prove the generator.

---------------------------------------------------------------------------------------------------

## 9. Work packages in build order

Sizes: S (a day or less), M (a few days), L (a week or more). Each level package contains: the map, its
script, its variants and boss data, strings and card line, rewards, the runner / GBA / PS1 / DC scripts
(generated), tests and a docs row; its test plan is section 8.10 steps 1-9 for the platforms it ships on,
plus what the table names.

### Wave T: tech first

| ID | Size | What | Files | Depends on | Test plan |
|---|---|---|---|---|---|
| **LV0** | M | Level registry and records: `LV_COUNT` 31 (main 7..15, NG+ 16..30), `lv_info` data split into `levels2.c`, unlock rules by story beat and flag (not only by previous level), the `BRCH` chunk, select pages (Breach 1-7 / 8-16 / Arrears), platform manifest, unlock and set-id reservations | `levels_int.h`, `levels.c`, new `levels2.c`, new `levels_rec.c`, `levels_select.c`, `save_ext.h`, `save_budget.c`, `unlock_ids.h` (comments), `maps.c` | K0 registry slot; CR11 id range agreed | `BRCH` round trip, absent chunk, old saves load unchanged; select pages at both sizes; GBA build lists only its levels |
| **LV1** | L | Map format v2: zones, paths, swap sets, texbank, `tflags2`, header keys; the BFS edges | `tools/mapc.py`, `doomo/map.h`, `map.c`, `maps.c`, `core/data/maps/README.md`, `test_maps.c`, new `tools/levelcheck.py` | LV0 | mapc tests for every statement and refusal; the 12 existing generated maps byte-identical; a BFS test map per edge kind |
| **LV2** | M | Level script VM | new `lv_script.c`, `levels_int.h`, `mapc.py` | LV1 | a host test per action; two runs hash the same; budget asserts |
| **LV3** | L | Mechanic kit A, movement: ride cycles, ziplines, bounce pads and low gravity, carry, tubes, raft; player ride state; bot edges | new `levels_mech_move.c`, `combat.c`, `combat_bot.c` | LV1, LV2 | unit tests; the bot crosses a kit test map; FP / TP shots at both sizes |
| **LV4** | L | Mechanic kit B, environment and light: fog zones, tide, lantern and candles, two faces, showtime light swap, collapse, dissolving rooms; per-level view distance in every renderer | new `levels_mech_env.c`, `gba/src/r3d.c` (one variable), `ps1/src/view3d.c`, `core/native/raycast.c` | LV1, LV2 | renders without holes at both sizes; GBA speed tour `vhash-seq` unchanged for the old levels |
| **LV5** | L | Mechanic kit C, rules and companions: stamps and queues, repossession, doubling, buy-back, pacifist mode, noise, ghost possession, feel, escort, chase pursuer, page rules, sky lanterns, hero override | new `levels_mech_rule.c`, `combat.c`, `actor.c`, `combat_hud.c` | LV2, LV6 | unit tests; bot plans; HUD shots |
| **LV6** | L | Enemies, AI, bosses: `DemonSkin` table, 9 AI flags, 4 new base demons (ids, `IS_DEMON` ranges), 31 bosses with mechanic ids, Library bestiary, Boss Rush sets | `actor.c`, `combat_int.h`, `doomo/actor.h`, new `levels_foes.c`, `levels.c`, `arena_modes.c`, `lib_bestiary.c` | LV0, LV7 (art) | a host test per AI flag; bestiary entries; Boss Rush runs every set |
| **LV7** | L | Art and audio kit: 7 shared textures, level-slot texture banks, 3 skies, 4 demon sprite sets and the Duchess portrait, usable / prop / collectible banks; 6 songs, ~12 SFX | `core/art/*`, `core/gen/art_data.c`, `textures.h`, `sprites.h`, songs (MML), `tools/sfxgen.py` | none (parallel with LV1) | `check-gen`; an art review sheet at both sizes; the SFX budget |
| **LV8** | L | Platform delivery: PS1 `BREACH.BIN` packs (writer, reader, VRAM pinning, overlay group plan), GBA manifest and ROM / EWRAM rows, Dreamcast pack reader | new `tools/packbreach.py`, new `ps1/src/breachpack.c`, `ps1/tools/ovl_plan.py`, `gba/Makefile`, `dc/src/` | LV1, LV7 (DC part: DC6) | PS1 disc links with the 64 KB stack reserve and loads a pack on Redux; GBA memreport row; DC host test |
| **LV9** | M | Combat detail option in levels: per-level drop lists and awake caps per mode on each platform | `combat.c`, `levels.c`, `gba/src/video.c`, `ps1/src/view3d.c` | the speed wave's combat detail option; LV1 | fps windows per mode on the Vape Cellar (baseline) and a kit test map |
| **LV10** | M | Test harness for levels: `test_levels2.c` skeleton, `tools/breach_scripts.py` (runner, GBA, PS1, DC scripts from one list), `tools/breach_fps.py`, CI steps | new files under `core/tests/`, `tools/`, `gba/harness/scripts/`, `ps1/harness/scripts/` | LV0 | generated scripts for the 7 existing levels pass on every harness first |
| **LV11** | S | Content notes and dark-level presentation: the card's note page, the select badge, the two options, text | `combat_card.c`, `levels_select.c`, `options.c`, `options_store.c` | LV0 | the card fits 240x160 (clarity tests); option round trip |

### Waves M1-M3: the 9 main-game levels

| ID | Size | Level | Ships on | Depends on | Extra test notes |
|---|---|---|---|---|---|
| **LV12** | M | HB8 Steam Room Panic | all (GBA) | LV3-LV11 | GBA per-mode fps; the Mature-off towel fallback shots |
| **LV13** | M | HB15 The Department of Contracts | all (GBA) | LV3-LV11 | the copier cap; stamp-only pacifist run by the bot |
| **LV14** | L | HB13 Boneyard Buffet & Bandstand (Party Pal art) | all (GBA) | LV3-LV11; W15 decision for its hook | the showtime stinger timing; SCRIPT_STYLE parody lint (no ARG mechanic) |
| **LV15** | M | HB9 The Undercroft (dark) | all (GBA) | LV12 (the GBA budget proven) | content note; lantern light writes on GBA; the dark-level joke-count lint |
| **LV16** | M | HB11 The Glum Pipes (dark) | PS1, DC | LV15 | the letters' filter (no self-harm, gentle children's lines); "Return to Sender" quest |
| **LV17** | L | HB12 The Wall (dark) | PS1, DC | LV15 | BFS over both faces; the crossfade |
| **LV18** | L | HB10 Fright Night at the Fun Fair | PS1, DC | LV12 | PS1 display-list peak under 40 KB in the midway |
| **LV19** | L | HB14 Hell Pits Highrise | PS1, DC | LV12 | the rent pacifist route spends H$ (life-sim money test) |
| **LV20** | M | HB16 The Ninth Cloud | PS1, DC | LV12 | sets the FINAL NOTICE keepsake flag |
| **LV21** | M | Main-story hooks and rewards: the 9 side missions / Activities rows, wayfinding, rewards (sets via CR11, weld, furniture, songs), news, Talk topics, Boss Rush "Breach 16" | `story_side*.c`, story packs, `levels2.c`, `unlocks_*.c`, `sim_events.c`, Talk tables | LV12-LV20, CR11 | `story-check`; every level's unlock path from a story save |

### Waves N1-N5: New Game+

| ID | Size | Level / work | Ships on | Depends on | Extra test notes |
|---|---|---|---|---|---|
| **LV22** | L | NG+ arc story layer: the cold-open hook, NG+ asides, Chapter 5, flags, choices, held residents (F9 away reason), the arc's endings, `ngp_` last ending, the fourth line, title variants, GBA recap cards, the `ngp` pack | `story.c`, `story_int.h`, story packs, `ending.c`, `ending.h` (if approved), `life.c`, `fp_title.c`, `cutscene*.c` | owner question 1; LV0; the script rewrite wave's style | `ngplus_arc.txt` to each ending; recap-card path; save round trip mid-arc |
| **LV23** | M | N1 Day One (dark, cold open; hero override) | all (GBA) | LV22, LV3 (carry) | plays before P0 on a first NG+ run, skippable later |
| **LV24** | M | N2 The Dead Letter Office (Late Fee art) | PS1, DC | LV22 | the loaner crate in Ch1; the red-envelope collateral text per last ending |
| **LV25** | L | N3 Repo Night (Repo Man art) | PS1, DC | LV22 | the eruption variant line; weapons always recoverable |
| **LV26** | M | N4 The Interest Engine | PS1, DC | LV25 | the 12-alive cap; PS1 fps with 12 Interest |
| **LV27** | L | N5 Lake of Fire Regatta | PS1, DC | LV25 | Fish & Cook entries; Ember's quest part |
| **LV28** | L | N6 Everything Must Go | PS1, DC | LV25 | buy-back with real H$; the held / returned split |
| **LV29** | M | N7 The Audit (midpoint; Duchess art) | all (GBA) | LV28 | the Mayor held; pacifist mode (no kill possible) |
| **LV30** | M | N8 The Debtors' Prison (dark) | PS1, DC | LV29 | both choice branches |
| **LV31** | L | N9 The Artist's Studio (dark) | PS1, DC | LV29, W15 | the care note (old age); no decoding (parody lint) |
| **LV32** | M | N10 The Ward of Forgotten Names (dark) | PS1, DC | LV29 | names "???" on the island and restored after |
| **LV33** | M | N11 The Hollow Mingle (dark) | PS1, DC | LV30 | escort branch (Mayor / Nox); the deal never lists a child |
| **LV34** | M | N12 The Interest | PS1, DC | LV33 | one-hit on / off by the deal |
| **LV35** | L | N13 The Vault of Unfelt Things (dark) | PS1, DC | LV33 | memorial from the life sim (adults, elders, pets only); the names option |
| **LV36** | L | N14 The Ledger (finale) | all (GBA) | LV35 | three endings; GBA 2D panels for the crowd and rain; per-page rules |
| **LV37** | M | N15 Both Are Better (epilogue) | PS1, DC (GBA: overworld beat) | LV36 | ending-dependent guests; polaroid into the E2 frame |
| **LV38** | M | Integration: all 24 on every harness, the fps table, Boss Rush "Arrears", docs (`LEVELS.md`, `GBA.md`, `PS1.md`, `DREAMCAST.md`), ROADMAP | docs, CI | LV12-LV37; DC6 for the DC rows | the full fps table; every level green on its platforms |

A wave ships when its levels pass section 8.10 on their platforms; levels of one wave can land one by one.

---------------------------------------------------------------------------------------------------

## 10. Risks and owner questions

### Owner questions

1. **NG+ Chapter 5 and the endings.** May NG+ get a Chapter 5 "Arrears" after the replayed credits (new
   story-engine content with its own lock file), and may the arc's three endings use the retired slots
   E03 / E05 / E07 (the list becomes n/14 again)? Fallback: NG+-only variants that do not count.
2. **W15** (Bonejangles' origin in the game) decides HB13's story hook and Notice 9's reveal.
3. **Memorial names** in Notice 13: On by default (with the Options switch), or Off by default?
4. **The cold open** plays automatically at the start of the first NG+ run (before P0). Or only from the
   Arrears page?
5. **Six new creator sets** (31 of the 32 until CR4's wider chips), or fold some into existing sets?
6. **GBA recap cards** for the 12 NG+ levels that are not on the cartridge (the plan's default), or no NG+
   arc on the GBA?
7. **The Duchess as a resident under E03** (adult, datable, bills per date): in register?
8. **Build order:** after the current waves (default), or sooner?

### Risks

* **GBA combat speed.** Today 9.8 fps; the plan depends on the speed wave and builds the 7 GBA levels
  cheap, but a steady 30 is measured, not assumed; misses are reported with their gap.
* **PS1 RAM.** ~14 KB of headroom; the pack design adds no resident level data, but the combat + levels
  overlay group must not grow past its budget (the select can move to its own group).
* **Dreamcast** level runs wait for DC6.
* **Tone of the dark levels and the NG+ arc**: needs the owner's tone check before the levels are built
  (like the disaster scripts); the lint enforces the joke budget and the hard limits on text, a human
  review does the rest.
* **Scope.** 24 levels: kept affordable by variants instead of new art (4 new demon bases), three shared
  mechanic kits, one script VM, generated tests and wave-by-wave shipping.
* **Bot coverage.** Every mechanic needs autopilot support or its level cannot be tested; it ships with
  its kit.
* **Translation.** All new text is written first and translated after (DECISIONS).
* **Save size.** `BRCH` about 150 B; checked against `save_budget` and the GBA slot bound in LV0.
