# Co-op heroes: Ember, Rook and Wick (MP1 content, 2026-11-05)

The three co-op heroes of the guest profiles (`HERO_COOP_FIRST..+2`, `core/src/hero.c`). A guest who picks one plays an
adult housemate made from the hero's look seed. This file is the content: final names and looks, story lines, side quests
and abilities. The data is in `hero_coop_info(kind)` (`playas.h`); a test locks it to the look seeds.

## Rules (all three)

* **Side arcs only.** The main hero (profile 0, Sunny / Marvin / custom) drives the plot. A co-op hero never sets a story
  flag, beat or ending (the `story_fingerprint` stays the same, like play-as).
* **Adults.** Each is `LIFE_ADULT`; romance follows the normal gate (`sim_romance_ok`). Register is the story's PG-21: warm,
  odd, a little bawdy at most; nothing mature, nothing with a child in it. The kid-cast list is untouched.
* **Quests**: one per chapter, played in that chapter's days, optional, repeatable in New life. Rewards are H$, friendship
  and a cosmetic (never a plot item). Each quest is one scene of the house / town, no new map.
* **Abilities**: one passive, one active (once per game day). Small numbers on the existing 0..100 stats, so they help
  the household and never replace a system (no skip of exorcism, combat or the story).

## Names and looks (final)

| Hero | Name | Pronoun | Species / ears / tail | Seed | Palette | Why |
|---|---|---|---|---|---|---|
| Ember | **Ember** | she | Imp / Pointy / Devil | `0x00C0E01D` | ember orange, soot, cream apron | fire; "ember" is also a Hellvile pet name already in the catalog |
| Rook | **Rook** | he | Owlkin / Owl Tufts / Feather Fan | `0x00C0E010` | slate and dusk blue, a brass lantern | a watcher; a rook sits on the high wall |
| Wick | **Wick** | they | Mothkin / none / none | `0x00C0E009` | warm cream, lamp yellow, soft grey | a wick is what a lantern burns; moths go to light |

**Change from the placeholders:** the third hero was "Pip". **Pip is the show's child** (a bunnykin kid, and the family
pool uses the name too), so a grown co-op housemate called Pip would blur the kid cast. The third is **Wick**. Ember and
Rook keep their names. The placeholder seeds made two raccoonkin and a pupkin; the new seeds give an imp, an owlkin and a
mothkin, three different silhouettes. To keep "Pip" instead, change one table row in `hero.c` and the name checks in
`test_hero.c`. Saves that already hold a co-op guest keep the seed they were saved with.

## EMBER, the Hearth Imp (she)

**Story line.** Ember crossed from the Hellvile side before the Wall came down, carrying a family recipe and a letter of
introduction nobody on this side would read. She wants a table people sit at without checking the exits. The wound: in
Hellvile her cooking was "too warm". The payoff: she feeds the united park.

**Quests** (one per chapter)
1. **Wrong Side of the Mat** (Ch1): she is locked out of the house you share. Find her a key (the muffin gag turns up again), and she
   cooks the housewarming muffins badly on purpose so the town will forgive her.
2. **Something's Cooking** (Ch2): she smells the vape in the new shop before anyone. Follow the smell to Grape's stock, bring back
   three samples, and cook the one thing that makes the room go quiet.
3. **Keep the Stoves Lit** (Ch3): the red night. She holds the kitchen while the house is boarded up; keep the pilot light alive
   through three waves (the house defence loop), and feed whoever comes in.
4. **One Table, Both Sides** (Ch4): the Feast scene (S186). Cook one dish from each side of the Wall and seat two rivals together.

**Abilities**
* **Pilot Light** (passive): at home at night, every housemate near her gains +1 Sanity per game hour.
* **Hearth Blast** (active, once a day): a cooked dish that restores +30 Hunger to the whole household and puts a possessed
  neighbour off the idea for an hour (no damage).

**Lines.** "I don't do *ready-made*. I do *ready.*" / "Sit. Sit! It's not a trap, it's a casserole."

## ROOK, the Night Watch (he)

**Story line.** Rook is an owlkin who stopped sleeping the night the Wall went up and has kept a log ever since: doors that
were open and are shut, a fountain that hums. Nobody reads it. He wants to be believed, and the wound is that he
saw something early and said nothing. The payoff: his log is read at the Wall's fall.

**Quests**
1. **The Wrongness Log** (Ch1): find four oddities from the wrongness catalogue (a door, a smile, a shadow, a fountain) and write them
   up. Each one found is a line in his log.
2. **Things Move at Night** (Ch2): the haunted-house chapter. Sit up with him; spot three objects that moved and
   leave them as they are. He shows you the pattern.
3. **The Long Watch** (Ch3): the siege night. He takes the roof; call out the next wave one minute early so the household
   can board up the right side.
4. **Last Entry** (Ch4): at the Wall he reads the log out loud to the park, and for the first time somebody writes back.

**Abilities**
* **Night Eyes** (passive): at night, a villager within a few steps shows their Corruption (it falls by 2 each time he
  watches them, so he can spot a possession starting).
* **Roost** (active, once a day): a housemate who naps beside him recovers +35 Energy.

**Lines.** "Not asleep. Just *thorough*." / "Write it down. If it's nothing, it's a very small nothing."

## WICK, the Lantern Moth (they)

**Story line.** Wick follows the brightest thing in any room, a lantern, a fireworks stall, the glow of a vape. It is
how they got here and how they nearly get lost. They want a light that is theirs. The wound: they have never chosen where to go,
only what shone. The payoff: at the park they light the first lantern themselves, and it is a small one.

**Quests**
1. **Moth to a Lantern** (Ch1): at the Fun Fair (lantern string) they follow every light. Gently lead them home past four, and
   hang one lantern for them.
2. **Bright Idea** (Ch2): the vape glow lures them toward the shop. Get there first, steer them out, and bring the
   Corruption they picked up back down (the exorcism hugs help).
3. **Lamp Post** (Ch3): in the long red dark they stand still at the door and hold the lantern; the household crosses the
   street by their light. Keep it lit until dawn.
4. **Their Own Light** (Ch4): the park festival. They make the first lantern, a bad small one, and let it go up.

**Abilities**
* **Lantern Wings** (passive): in dark rooms and at night, housemates near them gain +2 Sanity per game hour.
* **Dust Hush** (active, once a day): a friend in reach loses 15 Corruption (it adds to an exorcism hug; it never exorcises alone).

**Lines.** "Ooh. Shiny. No. *Home.* Home is shiny." / "I've got it. I've got it. It's very small."

## What is built and what is not

* **Built (core, tested)**: final names, pronouns, look seeds, `hero_coop_info()` (title, one-line story, four quest titles,
  two abilities with the stat and amount), a test that the seeds make the species, ears and tail above.
* **Built (2026-11-05, second patch)**: **Ember's Ch1 quest "Wrong Side of the Mat"** as a script, `coop_ember_1`
  (`core/data/story/coop/ember.dsc`, pack `zc_embr`, episode kind, 0 minutes). She is the speaker `target`, so it plays with the
  co-op housemate as the conversation's subject. Two routes (the muffin key, or knock on a PULL door) and a decline; the test
  `coop_ember_quest_plays_both_routes_and_leaves_the_plot_alone` plays both and checks `story_fingerprint` is unchanged.
  Adding the pack renumbers the generated text files (pack ids sort by name), so `core/gen/` and `flow.lock` are regenerated.
* **Built (third patch): the trigger.** Co-op guests only exist post-game (play-as unlocks when the story is done), and a household member
  is not a person you can talk to on the story map, so a co-op quest does not wait for a chapter. When a guest **plays** Ember on the
  overworld, `story_coop_tick()` (story.c, called from `overworld.c`) starts her next unplayed quest once. The played bit is the
  guest's own saved PROF flag `PROF_FLAG_COOP_QUEST(q)` (bit 8+q): no save-lock change, no new field. The quest sets no story
  flag. Test: `mp1_a_guest_playing_ember_gets_her_first_quest_once`. New accessor `prof_hero(profile)`.
  A family id has no portrait, so her lines show in a plain box for now.
* **Built (fourth patch, W6 step 3, not yet test-run)**: a reward on every quest (H$, a small item and `befriend` +2 with a town character tied to the quest, e.g. Ember Ch2 Steamy, Rook Ch4 the Mayor; the Ch4 quests also grant a cosmetic furniture piece: Ember a Teacup Table, Rook a Cherry Clock, Wick a Honey Lamp; never a plot item); Ember's quests 2-4
  and all of Rook's and Wick's (12 scripts, all in pack `zc_embr` so no pack id moves), released one per game day by `story_coop_tick`
  (day stamp in `prof_quest(p, 0)`); family speakers now draw their own portrait (`dlg_res` in ui_dialog.c, `res_of` in portrait.c);
  the abilities in play (`core/src/story_coop.c`): the passive runs hourly on the overworld, the active is the Y button once per game
  day (`prof_quest(p, 1)` = the day, PROF flag 12). Rook's and Wick's range checks use the overworld NPC positions.
* **Not built**: local simultaneous multiplayer, helping in another world, GBA / PS1 builds, Spanish rows. Run `make test` and
  `make story-check` on this patch before pushing.
