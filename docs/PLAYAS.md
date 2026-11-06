# Play-as and the heroes

After the story is cleared, the player can switch who they control at any
time: the hero, any living or ghost resident, any family member, and the
main cast as villagers. In Free play the same switch unlocks after the
game is completed. A post-game "New life" save starts as a villager
(PA6). The hero is **SUNNY** (she, the show's lead), **MARVIN** (he, a
fox) or one you **make yourself**. More hero kinds are reserved for the
co-op heroes of extra player profiles (MP1).

The story always belongs to the hero. While someone else is played, every
story write is refused (the barrier, PA1), and plot content offers itself
only to the hero (PA4).

| package | wave | what |
|---|---|---|
| **PA0** | W2 | this contract: `playas.h`, the weak defaults, the header byte 47 |
| PA1 | W3 | `playas.c`: the `PLAY` chunk, the story barrier, the sim hooks, ACTOR sites. **State half landed (2 Nov 2026)**, see section 7 |
| PA3a | W4 | `@( m \| f \| n )` text codes picked by `playas_you_gender()`. **Landed (2 Nov 2026)** in `vm_fmt_n`: `@(he\|she\|they)` = M \| F \| N, a missing variant falls back to the first; `@P` is the avatar's name while played |
| PA5 | W4 | `SCENE_PLAYAS`, the character switcher. **Landed (2 Nov 2026)**: `core/src/playas_scene.c`; PA4 pushes it from the L/R ring |
| PA2 | W5 | the hero choice: SUNNY, MARVIN or your own (`hero.c`, `hero_preset`). **Presets + new-game default landed (2 Nov 2026)**: `core/src/hero.c`, `story.c default_player`; the creator's three-card first page landed (2 Nov 2026; card texts checked to fit 240 px) |
| PA9 | W6 | `playas_romance_ok`: the hero rule on top of LS0's gate |
| PA4 | W7 | the overworld handoff (tag-in, the hero NPC, the L/R ring). **Landed (2 Nov 2026)** in `overworld.c` (`pa_update`, `pa_ring`): L = back to the last avatar (or the hero), R = SCENE_PLAYAS; the hero stands where you left them and walks back in when you return. Plot content is offered only to the hero (2 Nov 2026): story beats, plot talk, side-quest spots and the nest fight all wait while an avatar is played; an avatar's talk is ambient only. Not yet: `PLAYAS_SWITCH_FADE` across maps, a family member's own body sprite (family ids draw the default body) |
| PA6 | W8 | New life |
| PA7, PA8 | W8 | play-as lines; house, activities and almanac follow the avatar |
| PA10 | W10 | integration, the stress save, the ledgers |
| MP0, MP1 | later | player profiles (`PROF` chunk) and co-op heroes |

## 1. The contract (`core/include/doomo/playas.h`)

Every function is a weak default in `core/src/weak_playas.c` until its
owner lands. The defaults are "you are the hero, nothing is locked,
play-as is not available": the game and its save files are exactly what
they were before PA0. Getters are quiet. The two mutators
(`playas_switch`, `playas_set_hero_kind`) log `STUB: playas_<name>` and
change nothing, so `--assert-no-stubs` catches a caller that comes too
early. Owners add strong definitions in their own files. Nobody edits
`weak_playas.c` or `stubs.c` to implement one (LIFESIM_PLAN R4).

**Ids.** A play-as id is an `int`, stored as `u16` in `PLAY`:

| id | who |
|---|---|
| 0..999 | a resident slot |
| `PLAYAS_ID_HERO` = `DOOMO_PLAYER_ID` = 1000 | the hero (`doomo_town.player`) |
| `PLAYAS_ID_FAMILY_FIRST..LAST` = 1001..1020 | a family pool member, from `family_member()` (life.h, F1; R13) |
| `PLAYAS_ID_NONE` = -1 | nobody |

`town_res()` accepts 0..1000 only. F1's id audit makes the BODY paths
(below) and the sim arrays accept the family pool ids.

**Hero kinds** (3 bits, so they fit the header and the `PROF` records):

| kind | value | |
|---|---|---|
| `HERO_CUSTOM` | 0 | make your own (the creator's arg -3), and every save from before PA2 |
| `HERO_SUNNY` | 1 | female, `PRONOUN_SHE` |
| `HERO_MARVIN` | 2 | male, `PRONOUN_HE`, a fox |
| `HERO_COOP_FIRST..LAST` | 3..7 | reserved: the co-op heroes of guest profiles (MP1). Each gets one id, a preset, a side arc and abilities. The main hero (profile 0) is always 0..2 and drives the plot flags |

**Functions.**

| function | weak default | strong owner |
|---|---|---|
| `playas_avatar()` | `PLAYAS_ID_HERO` | PA1 |
| `playas_is_hero()` | `playas_avatar() == PLAYAS_ID_HERO` | PA1 |
| `playas_story_locked()` | 0 | PA1 (the barrier) |
| `playas_available()` | 0 | PA1: story cleared (`STORY_CH_DONE` + credits seen), a New life save, or Free play after completion |
| `playas_can_be(id)` | 0 | PA1: alive or ghost resident, the hero, a family person; not possessed, soul-lost, dead or empty; a pet only once uplifted (F5) |
| `playas_switch(id, how)` | STUB, 0 | PA1 (state), PA4 (`PLAYAS_SWITCH_TAGIN / FADE / INSTANT` presentation) |
| `playas_gender(id)`, `playas_you_gender()` | from the pronoun: SHE -> `GENDER_F`, HE -> `GENDER_M`, else `GENDER_N` | PA1 if needed |
| `playas_hero_kind()`, `playas_set_hero_kind(k)` | `HERO_CUSTOM`; STUB | PA1 stores it in `PLAY`, PA2 sets it at new game |
| `hero_preset(kind)` | NULL | PA2 (`hero.c`) |
| `playas_recent(i)` | `PLAYAS_ID_NONE` | PA1 (recent ring, PA4's L/R) |
| `playas_hero_pos(&pos)` | 0 | PA1 records it, PA4 spawns the hero NPC there |
| `playas_profile_flags()` | 0 | PA1, then PA6 / MP0 add their bits |
| `playas_romance_ok(a, b)` | `sim_romance_ok(a, b)` | PA9 |
| `playas_profile()`, `playas_profile_avatar(p)`, `playas_profile_hero(p)` | 0; profile 0 is the hero, 1..3 none | MP0 |

`family_count()` and `family_member()` come from life.h (weak in
`weak_life.c`, LS1), not from this header.

## 2. Call sites: BODY, ACTOR and HERO

Every place that names `DOOMO_PLAYER_ID` is one of three kinds. Only BODY
and ACTOR sites follow the avatar. HERO sites stay the hero's.

* **BODY**: whose body is drawn or walks. Becomes `playas_avatar()`.
* **ACTOR**: who did it. The relationship or need change goes to
  whoever acted, which is `playas_avatar()`.
* **HERO**: the story's protagonist (plot flags, story friendships,
  cutscenes, credits, the creator's story path). Stays `DOOMO_PLAYER_ID`.

| site (on `16b6929`) | kind | package |
|---|---|---|
| `overworld.c:883` billboard `bodyspr_image(DOOMO_PLAYER_ID, ...)`, `overworld.c:320, 732`, `overworld_fx.c:637` | BODY | PA4 |
| `combat.c:264-267` `pick_resident` | BODY | PA4 |
| `home_draw.c:683, 1254, 2051, 2054, 2107` | BODY | PA8 |
| `fish_gfx.c:807`, `dream.c:294` | BODY | PA8 |
| `lib_almanac.c:68, 82, 510, 517` ("(you)") | BODY (marks the avatar) | PA8 |
| `portrait.c:680`, chibi caches, `bodyspr_image` | BODY (id range for family pool ids) | F1 audit |
| `sim_care.c:69`, `sim_events.c:151`, `sim_soul.c:281`, `cook_data.c:357`, `exorcism.c:369` `rel_adjust(..., DOOMO_PLAYER_ID, ...)` | ACTOR | PA1 |
| `story_talk.c:279` `talk_ambient` friend-making | ACTOR | PA7 |
| `town.c:187` edge eviction bonus | ACTOR (also for the avatar) | PA1 |
| `sim_social.c:102` | romance gate | PA9 |
| `story.c:138, 150, 229, 232, 536` (default player, `story_befriend`) | HERO | PA2 (default player -> SUNNY preset) |
| `story_script.c:21` (`ME`), `story_ch12.c:158`, `story_places.c:209, 221` | HERO | (PA4 routes `PLACE_HOME` by avatar kind) |
| `scriptvm.c:95, 127` (`VM_SAY_PLAYER`) | HERO | PA3a (`{P}` = avatar name when locked) |
| `cutscene.c:435`, `cutstage.c:551, 555`, `credits.c:143`, `cutscene_ch12.c` | HERO | none |
| `creator.c` arg -3 | HERO | PA2 (three-card first page) |
| `overworld_spawn.c:175`, `overworld_npc.c:527` (NPCs face the player) | BODY (the body on screen) | PA4 |
| `fp_resident.c:900`, `fp_common.c:253` ("You", the caretaker's look) | BODY | PA8 |
| studio, song, concert (the hero sings) | HERO | none |

## 3. The story barrier (PA1)

While `playas_story_locked()` is set, these refuse and log
`PLAYAS: blocked <what>`:

* `story_set_flag`, `story_set_beat`, `story_episode_set_done`;
  `story_befriend` stays the hero's
* side-mission state changes (`story_side.c`); side offers are hidden
* VM ops `SET_BEAT`, `QUEST`, `BEFRIEND`, and starting a story script
* auto events, story triggers and secrets in the overworld (PA4)

Whitelisted (they keep running for anyone): `gag_cool` and the spawner
cadence. The tour test (PA4, Z1) checks `story_fingerprint` and the 256
flags plus STRY, minus the whitelist, are unchanged after playing as
every cast NPC and 10 villagers.

The sim ejects you back to the hero (`PLAYAS_SWITCH_FADE`, with a toast)
when the avatar is possessed or dies. The avatar is exempt from
autonomous activity and neglect death while played.

## 4. The `PLAY` chunk (PA1; registered by K0)

The tag is `'PLAY'`. The descriptor is weak NULL until PA1, so nothing is
written. The maximum is 800 B (`SAVE_EXT_MAX_PLAY`, life_contract.h
`PLAY_SZ_*`): a 32 B header and 16 per-villager pockets of 48 B. Money,
items and health are per villager (user answer 13). The pet follows you.
Planned header (PA1 owns the exact bytes inside the 32):

| off | size | field |
|---:|---:|---|
| 0 | 1 | layout version |
| 1 | 1 | flags (new life, ...) |
| 2 | 2 | avatar id (u16, 0..1020) |
| 4 | 1 | hero kind (`HERO_*`) |
| 5 | 1 | recent count |
| 6 | 2 | switch count |
| 8 | 8 | recent ring, 4 x u16 |
| 16 | 6 | hero position: x s16, y s16, map u8, dir u8 |
| 22 | 2 | pocket count, LRU head |
| 24 | 8 | spare |

A pocket is: resident u16, health u16, money u32, 16 x (item u8,
count u8), 8 B flags and quest progress. An id without a pocket uses what
the sim generates for it. The hero's pocket is the town purse and bag.

Reading: a missing chunk resets to "avatar = hero". An avatar that is no
longer valid on load is clamped to the hero. With the chunk the stream
grows by 42 B (32 + the 10 B chunk header). Everything else stays
byte-identical.

## 5. The save header's byte 47 (PA0; R9)

The 48-byte header (`save.c`) holds the player name in bytes 36..46
(11 chars), so byte 47 was always 0. It is now a play-as summary that the
title and the load screen read without decoding the save
(`DoomoSaveInfo.flags`, from `save_peek` / `save_info_from`):

| bits | meaning | set by |
|---|---|---|
| 0 `PLAYAS_HF_STORY_CLEARED` | the story was cleared in this save | PA1 |
| 1 `PLAYAS_HF_PLAYING_OTHER` | the avatar is not the hero | PA1 |
| 2 `PLAYAS_HF_NEW_LIFE` | a New life save | PA6 |
| 3 `PLAYAS_HF_PROFILES` | guest player profiles live here | MP0 |
| 4-6 `PLAYAS_HF_HERO_MASK` | the hero kind, so menus can say Sunny / Marvin | PA1 / PA2 |
| 7 `PLAYAS_HF_RESERVED` | 0 | none |

It is only a summary. Loading never trusts it: the `PLAY` / `PROF`
chunks and STRY are the truth. An old save reads 0 ("custom hero,
nothing unlocked"). A header with 0xFF there still shows its full
11-character name (`test_contracts_pa0.c`). The header's CRC covers byte
47, as before.

## 6. Budgets

| what | budget | PA0 (measured on `16b6929`) |
|---|---|---|
| save, story slot 0 | `PLAY` <= 800 B; the PA10 stress save <= 32,768 B (36 KB cap minus a 4 KB margin) | 0 B: streams byte-identical (native, in memory and streamed) |
| GBA EWRAM | play-as in total <= 64 B (PA1) | -16 B: `DoomoSaveInfo` 32 -> 36 B, kept twice each in `fp_title.c`'s `T` and `fp_saveload.c`'s `L` |
| GBA ROM | play-as <= +24 KB | +24 B |
| PS1 resident | play-as <= +3 KB; disc >= 72 KB below the stack top | +32 B |
| PS1 `SCENE_PLAYAS` group | <= 16 KB (PA5) | none |

Tests: `core/tests/test_contracts_pa0.c` (ids, hero kinds, flag layout,
a fresh town plays the hero, gender, the romance chain, byte 47 through
memory and the slot, old and 0xFF headers). Every check holds after the
owners land too: it tests a fresh town and compares byte 47 with
`playas_profile_flags()`, not with 0. PA1's own behaviour is
`test_playas.c`.

## 7. PA1: what landed (`core/src/playas.c`, 2 Nov 2026)

* **State**: small zero-initialised statics; the avatar is an offset from the
  hero, so all-zero means "you are the hero". Recent ring of 4 (newest first,
  never the current avatar), hero kind, hero position (`playas_set_hero_pos`,
  for PA4), New life flag.
* **Rules**: `playas_available` = story mode at `STORY_CH_DONE`, Free play
  with any ending seen, or New life. `playas_can_be` = not yourself, the hero,
  a living or ghost resident, or a non-pet family member (pets wait for F5).
* **Barrier**: `story_set_flag`, `story_set_beat`, `story_episode_set_done`
  refuse while `playas_story_locked()` and log `PLAYAS: blocked <what>`. Still
  open: side missions, VM ops, overworld auto events (PA4).
* **Sim**: `playas_check()` runs in `day_rollover`; a lost, possessed or
  removed avatar sends you back to the hero with a toast.
* **`PLAY` chunk**: 32 B header only (no pockets yet), empty while default.
  save.c still writes the 10 B chunk header for everyone (`test_save.c`'s v1
  re-save bound allows for it).
* **Family screen**: "Play as them" switches for real; a "Switch back" row
  shows while you play someone else.
* **Not here**: how a switch is shown (PA4/PA5). Tests: `core/tests/test_playas.c`.

## 8. PA6: New life (`core/src/playas.c`, `playas_scene.c`, 2 Nov 2026)

* `playas_new_life_ok()`: story mode at `STORY_CH_DONE` and not already a New life.
* `playas_new_life(id)`: sets the New life flag (`PLAYAS_HF_NEW_LIFE`, header byte 47 bit 2; it replaces
  `PLAYAS_HF_STORY_CLEARED`), then switches to `id` with a fade. Refuses the hero, a pet, an unplayable id,
  or an unfinished story; on a refused switch the flag is rolled back.
* After a New life play-as stays open for good, the story barrier still holds while you are not the hero, and the
  hero can be switched back to like anyone else.
* UI: `SCENE_PLAYAS` gets a last row "Start a new life..." while `playas_new_life_ok()`. Confirm, pick a name from the
  list (the same rows as Play as), confirm again. B steps back.
* Not built: a title-screen entry, and starting a brand-new world as a villager.

## 9. PA10 part 2: per-avatar pockets (`core/src/playas.c`, 5 Nov 2026)

Money, the bag and health belong to whoever you play (user answer 13). The live fields stay where every shop,
gift and cooking site already reads them (`doomo_town.money`, `.items[]`, `.story.hp`, `.story.armor`), so nothing
outside `playas.c` changed: they are always the **avatar's**. `playas_switch` (and `playas_check`, when a lost
avatar sends you home) saves the outgoing avatar's three things into a pocket and loads the incoming one's.

| who | where the things wait while somebody else is played |
|---|---|
| the hero | one wide stash of **two pocket slots** (the whole 64-kind bag, money, health, armour), only while another avatar is played |
| a villager / family member | a **pocket** of 48 B: id, health, money, the **16 biggest stacks** (ties: lower item id), armour |
| no pocket yet | what the sim generates, never random: from the town seed and the id, a purse of H$40..199, full health and two different snacks |

* **Slots.** 16 in the chunk (`PLAY_POCKETS`): 2 for the hero's stash + **14 villager pockets**, most recently used
  first. Playing a 15th villager drops the oldest pocket (it falls back to the generated one).
* **Wire.** After the 32 B header: the hero stash (2 slots) when header byte 23 bit 0 is set, then the pockets,
  newest first. Header byte 22 = slots that follow. Pocket: id u16, health u16, money u32 at 4, item/count pairs
  from byte 8, armour at byte 40. Hero stash: the 64 counts from byte 8, armour at byte 72. Everything is clamped
  on load (health 0..200 else 100, stacks 0..99, no duplicate or unknown item, no pocket for the hero, the avatar
  or an invalid id). A chunk from before PA10 (header only) reads as before.
* **Old saves mid play-as** have no hero stash: switching to the hero keeps what is carried (the old shared
  behaviour), nothing is lost.
* **Size.** The `PLAY` chunk is `<= 800 B` (`SAVE_EXT_MAX_PLAY`), checked by `pa10_pockets_round_trip_and_the_chunk_stays_under_800`.
  The stress save (`pa10_stress_save_round_trips_under_the_gate`) is 10,123 B against the 32,768 B gate.
* **API.** `playas_pocket_count()`, `playas_pocket_money(id, &m)` (the family and almanac screens can show a
  held purse). The avatar's own things are the live town fields.
* **GBA / PS1.** `.bss` +~890 B in `playas.c` (14 pockets x 36 B + the 72 B hero stash + a few bytes): not measured
  on GBA EWRAM or PS1 here; an EWRAM ledger row is still to be added when it is (the budget row above allows
  play-as <= 64 B for PA1; PA10 needs its own line).
* **Tests** (`test_playas.c`): pockets follow the avatar (money, bag, health, armour, both ways), the generated pocket
  is stable, round trip and the 800 B cap with 20 villagers, a hostile chunk is clamped, an old save mid play-as
  keeps what is carried, a lost avatar sends the hero home with the hero's bag.

## 9. PA10 part 2: per-avatar pockets (`core/src/playas.c`, 5 Nov 2026)

Money, the bag and health belong to whoever you play (user answer 13). The live fields stay where every shop, gift
and cooking site already reads them (`doomo_town.money`, `.items[]`, `.story.hp`, `.story.armor`), so they are always the
avatar's. A switch saves the outgoing avatar's three things into a pocket and loads the incoming one's. No call site changes.

* **The hero's stash**: while somebody else is played, the hero's money, health, armour and the whole 64-kind bag sit in
  one wide stash of two pocket slots (96 B). Switching back to the hero empties the stash into the live fields.
* **A villager's or family member's pocket** (48 B): resident u16, health u16, money u32, the 16 biggest stacks as
  (item, count) from offset 8, armour at offset 40. Smaller stacks are not kept (ties: the lower item id).
* **14 villager pockets**, newest first; the oldest is dropped when full. An id without a pocket gets what the sim
  generates: a purse of H$40..199 from the town seed and the id (never random), full health, two different snacks.
* **Chunk**: header bytes 22 (pocket slots that follow, the stash counts two) and 23 (bit 0: the stash comes first).
  Worst case 32 + 2 x 48 + 14 x 48 = 800 B = `SAVE_EXT_MAX_PLAY`. An all-default chunk is still 0 B. A 32 B header from before
  PA10 reads fine: with no stash, switching to the hero keeps what is carried (the old shared behaviour, nothing lost).
* **Loading never trusts the file**: health over 200 becomes 100, stacks over 99 become 99, bad / duplicate / hero /
  avatar ids and bytes past the end are dropped. An avatar no longer valid on load sends the hero home with the stash.
* **Lost avatar** (`playas_check`): their pocket is kept, the hero's stash comes back.
* **API**: `playas_pocket_count()`, `playas_pocket_money(id, &m)` (what is held, not the live avatar's things).
* **Tests**: `pa10_pockets_*`, `pa10_default_pocket_*`, `pa10_hostile_chunk_*`, `pa10_an_old_save_*`, `pa10_a_lost_avatar_*`;
  the stress save is 10,123 B against the 32,768 B gate.
* **Budget**: about +0.7 KB of `.bss` on every platform (14 x 44 B pockets + a 72 B stash), not yet measured on GBA EWRAM / PS1.
  Add a row to the EWRAM ledger when it is measured.
* **Not here**: showing money, health or the bag of other avatars in a screen, and per-avatar weapons and ammo (the armoury stays shared).

## 10. PA10 part 3: New life from the title, and in a fresh world (5 Nov 2026)

* **Title**: with a finished story save, Story Mode > New story asks "Your story is finished. What now?":
  *New life here* (loads the finished world and opens the "who are you?" list on top of it; B keeps playing the hero),
  *New life, fresh world*, *New Game+* (only when unlocked), *Brand new story*, *Never mind*. A save that already is a
  New life does not offer *New life here* (Continue is that). Before, a finished story with New Game+ locked went
  straight to a new story; now it asks.
* **Fresh world**: builds a new Classic free play island (`fp_new_island`) and opens the same list. The saved island is not
  touched: a new island only autosaves into an empty slot until the player saves by hand, and the title warns first when
  an island is saved. The chosen villager becomes the avatar, the save carries `PLAYAS_HF_NEW_LIFE`, the story is not part of it.
* **API**: `playas_new_life_arm()` (free play only; makes `playas_new_life_ok()` true until a New life starts or
  `town_new` makes another world; never saved). `SCENE_PLAYAS` arg `PLAYAS_ARG_PICK_NEWLIFE` opens straight on the
  list (`S.direct`: B and "no" stay on it); with no New life possible it opens the plain switcher.
* **Tests**: `pa10_a_fresh_free_play_world_can_start_a_new_life`, `pa10_arming_is_for_free_play_and_ends_with_the_world`,
  `pa10_scene_opens_on_the_new_life_list_from_the_title`. The title menu itself has no script yet.
* **Not built**: a fresh world from the Free Play menu without a finished story, a name or look for the new life's
  first day, GBA/PS1 builds (not run here).
