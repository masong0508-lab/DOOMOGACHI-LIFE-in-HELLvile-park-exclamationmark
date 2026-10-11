# DOOMOGACHI LIFE in HELLvile Park! — Game Design

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
* **Doomogachi**: a tamagotchi pet on an LCD-green screen (see 2.3).
* **Possession**: at night, corruption creeps up (vape exposure, Hellvile
  visits, nightmares, nemeses). At 100 a resident becomes **POSSESSED** by
  a named Hellvile demon: red hollow eyes, creepy portrait, haunting
  others, spreading corruption to friends.

### 2.1 Story Mode (the full arc, ~2–4 hours)
You create **yourself** (a dedicated player record) and move to HaHaland.

**Ch.1 "Welcome to HaHaland"** (extremely cutesy open world, top-down)
* Mayor Gigglesworth (cherub) welcomes you; quest for your house key.
* Nana Pudding (baker) gives you a **Doomogachi egg**. Hatch tutorial:
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

### 2.3 Doomogachi (tamagotchi)
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
minute one. The rest are earned through play, with a "NEW UNLOCK!" toast
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

## 5. Look & feel
**Premium, not cheap pixel art: cute, lively, constantly animated, and
vaguely furry.** Most citizens are soft plush critter-folk (cat, fox,
bunny, bear, pup, mouse, sheep, raccoon and owl folk alongside imps,
ghosts, skeletons and slimes), with fluffy fur tufts, big expressive ears,
fluffy tails and paw hands. Everything breathes, blinks, wiggles and
bounces. Shading is soft multi-tone with coloured outlines and rim light.
See ARCHITECTURE.md "Art direction" for the rules.

## 6. Tone
Cute first. The horror and gore are funny *because* the first hour is a
sugary tamagotchi daydream. Nothing mean-spirited toward real people;
violence is cartoon game violence against demons. Killing citizens is
gated behind failed exorcisms or the explicit Monster Mode.
