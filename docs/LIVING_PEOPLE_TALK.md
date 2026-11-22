# DOOMOGACHI: Living People, Memory-First Talk and Port Budgets

## Player experience / acceptance target

The goal is a *life simulation* in which every created citizen has observable
needs, routines, beliefs, relationships, emotional reactions, and memories.
Residents are simulated people **within game rules**, not sentient AI.

A real conversation must stay coherent for multiple exchanges: question ->
answer -> reason -> follow-up -> possible disagreement or actionable offer.
It should not jump from the player's topic to a random association.
Citizen responses and follow-up options should be sensitive to (a) what the
player just said, (b) the citizen's personality and needs, (c) prior
conversations, (d) the actual world state, (e) their knowledge and
relationships, and (f) the player's choices. Story gates and age safety
rules remain authoritative even when more language data is present.

## Implemented memory-first foundation in this pass

* **Language streaming**: the existing `plat_talk_read` / TALK_<lang>.BIN
  interface gains **one 256 B read-through cache** on non-GBA builds.
  Small 8/16-byte concept and association reads are coalesced. The window
  is invalidated between database/language changes; large reads bypass
  it. A failed page fetch falls back to the exact platform read.
  **No heap allocation, no per-resident language model, no resident record
  growth.** GBA omits the 256-byte cache altogether.
* **Conversation recall** uses the saved 12-byte resident entries. The active
  topic takes priority and a neutral opening can cue the last topic. The
  current caps are 32 entries on GBA, 64 on PS1, 128 on Dreamcast and 48
  in the native test profile. Open commitments are pinned, and completed
  shared experiences take priority over older casual chats at capacity.
* **Related transitions**: the quick-choice generator considers a fresh
  topic only when it is in the current concept's curated association list.
  While a thread is active, quick choices prefer continuing it. Every
  option still goes through `talk_dm_say` / `talk_dm_step`, rather than
  inventing a response outside the dialogue engine.
* **Emotional context**: the existing saved mood and resident need meters
  now influence offered choices for hungry, sick, tired, lonely or scared
  residents without a new per-resident emotion structure. Mood, needs and
  romance/age gates still belong to the simulation.

## First everyday topics

Open **Options → Topic** during Talk. **Band**, **Fishing** and **Gardening**
are entry points. After discussing or selecting a topic, reopen the picker
for related choices. Choose **Ask about …** to ask the resident directly
and continue from their answer. Recent topics and known likes still appear,
and the picker reserves space for the usual stock topics. The links are read when
the picker opens and go through the existing audience and grief checks.

**Ask about their day** can place a direct follow-up on the main Talk
page when the resident mentions a venue and is open to discussion.
The answer names their actual activity. A resident who is tired or upset
can ask for a quiet day instead. The menu clears the offer when the
player switches to another resident or moves the conversation on. Taking
the follow-up starts a normal exchange, which records a topic only if
the resident answers it.

If the resident can receive an invitation there, **Plan a visit** opens
the outing picker with that venue highlighted. The player confirms a
destination and the resident answers through Talk. The shortcut checks
the resident and their activity again when chosen. If they have already
accepted an invitation today, Talk can remind the player of the plan.

| Starting topic | Linked subjects | What to cover in the next writing pass |
|---|---|---|
| Band | Song, lyrics, microphone, concert, encore | Ask what they like to play, give a reason, then follow up about a song or a performance. |
| Fishing | Fishing trip, pond, riverbank, fish | Ask about a favorite spot or a fish they would like to catch. Refer to a past outing only when the simulation records it. |
| Gardening | Garden, watering can, flower pot | Ask what they like about gardening, offer a different preference, and follow up about a plant or a tool. |

Write each first exchange as a question, a direct answer, a reason and a
follow-up. For each topic, try the same opener with two different
personalities, then return to the conversation after a save and reload.
Use preferences the resident already has. Route any actual invitation or
offer through the corresponding action menu.

Keep new directed links in both `assoc_hand_en.tsv` and `assoc_en.tsv` so
they survive later editorial review and reach the shipped bank. Validate
them with `make -C core talk-check` and the controller path in
`core/tests/scripts/home_talk_basic.txt`. See the [Talk authoring guide](TALK.md#4-the-word-network-t1).

### Test an everyday conversation from its opening to the next visit

Start with the resident's day in the simulation. Where are they going?
What do they need? Who did they see? Give the player a chance to notice
the activity, ask about it, and see how a choice affects later visits.
Residents should keep their routines and relationships when the player
leaves. They can have a bad day, decline a plan, or change their mind
for reasons the game can show.

The SNES Fact Book reference groups alternate spellings and names under
one topic. DOOMOGACHI already has stable concept IDs, synonyms, and a
streamed dialogue bank. Use that lookup to understand which subject the
player means. Let the simulation and what each resident knows decide
what they can say. Link a follow-up when their answer gives the player a
reason to ask it. Two residents can know different things about the same
event because they were in different places or heard it from someone.

Start with **Ask about their day** and a resident visiting the park.
The reply names where they actually are. A plant lover might tell the
player they like tending the garden, giving the player a reason to ask
about watering or propose a park visit through the existing outing menu.
Try the same opening with a tired resident who prefers a quieter day.
Return after the scheduled time in two runs, one with a completed outing
and one where it was missed. Save, reload, and verify that each response
follows what happened. In a group conversation, ask one resident about
gardening and another about fishing and verify each recalls their own
subject. Revisit the park after their schedules change and confirm that
the residents' locations, choices, and remarks agree.

Run the route with the PS2 controller from a cold and warm Talk bank.
Track first-reply and follow-up latency, total disc reads, and worst
frame time while the dialogue opens and scrolls. The story and relationship
gates still apply to the linked choices.

### Adaptiveness across one life

Treat these stages as checkpoints in a continuous simulation. Whenever
the player opens Talk, the resident's routine, temperament, relationships,
nearby people, current plans, and confirmed memories should shape the
greeting and follow-up choices. During a conversation, a move, completed
favor, gift, or change in who is present should be reflected when the
next relevant choice or reply is prepared. Meaningful outcomes should
show up again in the town through actions, activity, and later visits.

Keep one path for choosing relevant facts and linking topics across Talk
entry points. Read focused facts on conversation open or a turn, using
the selected resident and nearby participants. Use the existing sim
actions for care, gifts, favors, and outings, and the existing Talk gates
for suitability. Keep short, varied replies specific to the resident.
Reserve milestones for changes the story actually made to the town. The
examples below are writing and playtest targets for this same system.

| Stage | What the resident can react to | First encounter to write and test |
| --- | --- | --- |
| Creator and move-in | Saved traits, actual household, interests, and first meeting | A shy plant lover invites the player to look at a sprout. An outgoing neighbor with the same interest asks for help naming it. Both react to a real planting or gift on the next visit. |
| Preset island or first Story visit | Resident profile, familiar places, and relationships already in the world | A regular at Brimstone Bites points out where the baker works. The baker knows the shop and their own routine, and learns the player's name at their own introduction. |
| Ordinary days | Live activity, needs, weather or time where observed, and completed shared events | A friend changes their plan when tired, then asks to reschedule when their routine makes room. A neighbor passing by can react to what they see at the venue. |
| Days leading to a bad ending | Visible household needs, losses, and absence of close ties | An empty chair prompts a worried check-in when its usual occupant is missing. A visit, act of care, or reconciliation changes the next check-in only if the simulation records it. Keep the finale resolution in the existing story rules. |
| Ending and later visits | Reached ending, surviving residents, cleared chapter gates, and the actual state of the park | A resident remembers a parade they attended, or tells the player why they stayed away. In **New life here**, a different avatar meets the same surviving residents with their own relationships. |
| New Game+ | Run flag and documented carryover of endings seen and their counts | The new run can have an authored opening echo. A first meeting still uses the new world's relationships. Prior friendships and promises require an explicit carryover design before they can surface. |

Replay each stage with two personality profiles, then change one world fact
and check that only residents with grounds to know it change their response.
Check a warm reunion, a missed plan, a resident who needs time alone, and a
visit after save and reload. The test should verify an ensuing action or a
visible change in routine or relationship when an encounter promises one.
Also repeat the test mid-chapter at different times, in different homes
and venues, and after interrupting a group chat. Track time from a button
press to the first response and frame time through all of the transitions.

### Free Play and unlock progression

Cover Empty, Classic, and Random islands with the same Talk rules.
An Empty island starts with the people the player creates. Classic and
Random islands can offer local introductions, familiar places, and
existing relationships from the first day. Give players new reasons to
visit those places as friendships, routines, and shared plans develop.

Completing a care or item concern through Talk or the resident's menu
now counts toward the existing **Helping Paw** achievement and its
Pigtails reward in Free Play. The problem must be resolved. Inspecting
a concern or trying to deliver a missing item leaves it open.

Use that rule for future unlocks that grow the island. A shared activity
could open a club, recurring meetups could make a new outing available,
and completed projects could add shop stock or change a public space.
Show what opened in the game and give participating residents a grounded
reason to mention it. Keep rewards tied to the action and save them
through the unlock system. Test every earned reward after reload, and
exercise repeat actions and the equivalent Talk and menu paths.

## Next milestones, in priority order

1. **Coherence/fact tests**: 10+ turn scripted conversations with explicit
   topic commitments; evaluate factual grounding, response adjacency,
   pronoun/relationship persistence, and graceful unknown answers.
2. **Creator**: expose the existing five sliders, 24 traits and liked/disliked
   topic roots clearly, including instant previews of two characters
   reacting differently to the same prompt. Explicit overrides must save
   and load; today not every console persists the TALK save chunk.
3. **Episodic memory**: event records for promises, gifts, fights, illness,
   rescues, family events, and meaningful player utterances. Prefer
   stable IDs + day + outcome flags over saving transcripts. Add a bounded
   memory budget by port, persistent journal paging and old-save migration.
   A character must know facts through witnessing, learning or disclosure,
   not magically know every event in the town.
4. **Life feedback**: needs -> activity -> emotional appraisal -> Talk ->
   relationship outcome -> routine, mood and choice changes. Simulate in
   round-robin batches, not all residents every frame. All actors must
   have an accessible goal, explainable motivation and perceivable action.
5. **Disc-backed authored model** (PS1/Dreamcast): index grammar, dialogue
   strategies, relation graphs, semantic clusters, voice variants and
   contextual memories in compressed, CRC-checked banks; page into
   small buffers on demand. Keep integer-only bounded inference over the
   retrieved evidence, and stream/preload outside rendering-critical
   frames. Do **not** assume a 64 MB model fits PS1 main RAM.
6. **Port parity**: the shared reasoning/relationship rules must be
   deterministic across PC, PS1, Dreamcast and GBA, but use tiered
   vocabulary, cache sizes, rendering budgets and input UX. First
   confirm that each port actually enables Talk, bank I/O and saves.
   Never declare full port support until tested on that target.

### Million-conversation target (quality gate, not combinatorial marketing)

Measure at least 1,000,000 *reachable distinct conversation paths*,
not 1,000,000 shuffled phrases. Count a path only if all turns have a
contextually valid transition, a consistent emotional/relational state
and any promised world action is actually representable by the simulator.
Record generation seed and trace, enforce deterministic replay, sample
paths for contradiction, and provide human-authored quality regression
scenarios. This is a **future milestone**, not a claim about current output.

### Performance and compatibility gates

* No allocations in a Talk turn, no full-bank load into RAM, no new field
  in the 76-byte DoomoResident, no extra rendering pass.
* Preserve 30 sim ticks/sec on consoles and measure worst-case response
  latency, Talk scratch bytes, ROM data sizes, and disc-read count.
* `make -C core talk-check`, `make -C core unit` and platform CI must
  pass before release. Include true emulator screenshots/ROM checks for
  GBA Bone levels independently.
* Never let fluent responses bypass relationship/consent gates, story
  quests, calendar, inventory or unlock checks.

## Actionable Talk UI: invitations and shared play

The playable Talk screen now distinguishes **Add someone to chat** from
**Ask to go somewhere**. The latter opens a real destination menu with
known language-bank concepts and simulation venue IDs; choices are
filtered for the chosen citizen's live condition, venue opening rules,
and minor/room restrictions. Selecting a destination sends TA_INVITE to
the existing drama manager. An accepted invitation emits TEV_ACT_INVITE,
then automatically closes the chat after the NPC's answer and applies
`sim_nudge_activity` through `talk_outcome_end`. The NPC's activity
actually changes, and the player sees the destination in the toast.
A refusal remains a refusal, with no fake change. A quick reply proposing
a general outing opens the same picker, rather than choosing a random
venue. **The player is not automatically teleported**, nor is a new
navigation path invented; true joint traversal is a later milestone.

**Play together** now invokes `sim_play` using the selected resident and
immediately updates their actual Joy, Energy, Social and relationship
state, showing their actual simulation response. An unavailable resident
is refused by the normal care rules. These UI actions are in the shared
Talk scene; ports without Talk compiled do not magically gain it.

Controller tests in `test_talk_scene.c` check the eight eligible noon
destinations, a focused café invitation through save and reload, and
the stat changes from playing together. `test_talk_actions.c` covers
minor, status and daily action limits. The sections below describe the
inventory actions, shared memories, and the on-site meetup.

## Inventory-backed showcasing (implemented shared core)

A Talk **Show an item** page enumerates *actually held* non-key inventory
items. Children/rooms involving minors exclude adult-restricted items.
On selection, ownership, resident status and age restrictions are
rechecked before displaying the item and an actual response derived from
the resident's saved taste/fears and personality voice. The inventory
count does not decrease: *showing* is not *giving*. No friendship points,
free inventory, story progression or pseudo-item are fabricated. The
scene remains active and keeps the resident's original conversation.
A UI regression test verifies an owned Plushie is offered and remains in
the inventory after showcasing. Explicit real item gifting, as opposed
to showing, is a separate future confirmation/consumption action.

## Inventory gifting through Talk (implemented shared core)

**Give a gift** presents the actual inventory, filtered using the existing
`talk_act_gift_ok` age/item rules. Selection rechecks eligibility and
invokes `sim_gift`, which consumes exactly one held item (unless the
explicit infinite-inventory cheat is on), updates the recipient's needs,
preferences, care and friendship, and displays the real reaction.
This differs intentionally from **Show an item**, which never consumes
or duplicates inventory. Controller-driven tests check both pathways.
The separate care action's normal cooldowns/progression rules apply;
manual gift spam is not a neural conversation substitute.

## Memory-first shared experiences (no save-size expansion)

Three otherwise-unused high bits of the existing `TalkMem.likes_known`
byte now record **completed shared play**, **an actual gift given by the
player**, and **an accepted invitation**. They survive the existing
versioned Talk save-chunk round trip and do not enlarge any of the
12-byte recent-resident records. The low five known-like bits are
unmodified. A room action only records an event after it was processed;
refused invitations must not produce a memory. The distinction matters:
an accepted invitation is **not** proof of having met at the venue.
A citizen with a recorded interaction gains the **Remember when we...?**
response, with a small personality-dependent callback. No shared
memory option appears for a stranger or a resident whose LRU memory has
been evicted. The event markers are summaries, not a transcript or a
neural language model; longer episodic journaling remains a milestone.

Acceptance tests check zero-byte empty saves, unchanged memory-entry
size, remembered play/gift flags across a versioned save reload, and the
Talk UI exposing the memory option only after an event. The same
feature must be separately verified on each port that enables Talk.

## Physical-location meetup (first working town integration)

An accepted Talk invitation records the guest, inviter and destination in
the **day-scoped** Talk outcome ledger.
A player who walks to the matching physical building and opens its
normal place menu can now choose **Meet up with [resident]**. The action
is offered only when that NPC is still actually at the accepted venue,
their invitation has not already been claimed, and it is the same town
day. Completing the meeting restores both participants' Joy/Social,
strengthens their relationship, and records an actual shared-time
memory. A missed meetup or the wrong building makes no change, and
one invitation cannot be claimed twice. The input appears only on
platforms where Talk is enabled; non-Talk ports retain their place
menus. Neither the player nor the citizen is secretly teleported.

Physical meetup points now cover **all eight** invitations:
the Fun Fair inside the park for ACT_PARK; the Pier or Bone Pier
as the beachfront arrival point for ACT_BEACH; cafe/stalls, church,
shop, workshop, concert hall and vape shop for the six indoor venues.
The Fun Fair and piers are **arrival proxies**, not an assertion that
the player was standing at the exact pixel the NPC reached. Each
requires entering a real place menu before the meeting can be claimed.
Same-day plans survive a save and reload. Each plan belongs to the
character who made it. Reloading keeps the guest's saved activity,
including when they have already left the venue. The usual daily cap
still applies, including to plans already used that day. Plans expire
at day rollover.

This is a *location-validated interaction*, not yet a scene with both
characters visibly walking together, or full family/group outings.
Future steps: exact outdoor interaction zones (rather than Fun Fair
and pier proxies), place-aware NPC spawn presentation, and several
activity choices per venue. The native action test verifies the right
venue, single-consumption behavior, changed needs and shared memory.
No 3D raycasting, NPC arrays or per-frame work is added.

### Six-choice dialogue safety

The normal Story place picker has **six** slots (the UI's
`DLG_OPTS` constant); any seventh choice would be silently dropped.
The new meetup row therefore appears only when there is capacity
*after reserving a slot for Leave*. Existing place options always take
precedence, and menu size remains six on all ports. This prevents a
missing Leave option or a soft lock at a crowded building menu.

### Safe save migration for event memories

The outer `TALK` save registry still uses version 1, with the same
12-byte memory entries and declared chunk budget. **Payload byte 0**
uses 3 for memory-only or 4 when there is also an explicit-trait tail.
Payload 5 adds an 11-byte tail for the day's accepted invitations,
including used ones, within the existing chunk budget.
Old payloads 1 and 2 continue loading, but their high three
`likes_known` bits are masked out, as in the earlier implementation.
Payloads 3 through 5 restore the actual shared-event markers. This
prevents uninitialized high bits in old saves from being mistaken for
real meetings or gifts. Native round-trip and old-save tests cover the
new version and unchanged-structure migration rules.

## Care from within Talk (shared simulation)

The Talk main menu now includes **Help with their needs**. A resident
with ordinary living status can receive care using the existing
`sim_comfort`, `sim_clean`, `sim_feed`, and `sim_heal` pathways.
The first two directly affect real Joy/Social/Sanity or Hygiene and
relationships; feeding and treatment enumerate actual inventory items,
apply existing food/medical effects and consume the appropriate item.
No action invents food, medicine or a diagnosis. If the person is already
clean, the wash choice is hidden; if no eligible supplies are available,
the corresponding item menu is hidden. Age-sensitive items are excluded
in rooms with children. The list is built on user interaction, not every
render frame, and introduces zero bytes to each resident record.

Controller-driven checks verify that an available need appears, that
washing changes the saved Hygiene meter, and that offering real food
uses exactly one item. This is not full free-form neural reasoning,
but a concrete way for the resident's simulated life to shape actions
and player decisions. The ghost-play memory guard also now agrees with
`sim_play`: ghosts don't require a minimum Energy meter.

## Regression review after adding memory and care

Native CI exposed two old assumptions in test expectations, not a
resident save-size change: `traits_save_round_trip_and_old_saves`
compared a new writer's payload byte against legacy v1/v2 constants,
and the `home_talk_basic` control script still searched for the old
"Invite someone" label instead of "Add someone to chat". The test now
explicitly simulates v1 when checking compatibility, while comparing
new writes against the correct event-aware v3/v4 payloads. The UI
script expects the renamed control. The care-menu test now bounds
dialogue-dismissal presses at 40 and asserts the box actually closes.
These are test contract changes, not a claim that full console
integration has passed; retest with `make -C core unit scripts`.

## Creator-selected traits alter follow-up actions

The Talk quick-reply selector now checks a resident's **explicitly
creator-selected** trait set when they have one. With the five sliders
held constant, Goofball/Cheerful/Outgoing invite joking and laughter;
Shy/Kind invite polite, thoughtful engagement; Bookworm/Curious/
Ambitious invite questions and serious topics; Creative invites
expressive replies; and Mean/Hotheaded can prompt blunt disagreement
**only** in an eligible adult room. The shared `talk_trait_room` gate
removes edgy traits for minors, and urgent distress, hunger, sickness,
fatigue and fear override the trait-based suggestions. Without custom
traits the pre-existing slider-based choice remains unchanged.

The rule runs only when building player choices; it does not add to
resident records, allocate a language model, load bank-sized data into
RAM or add per-frame rendering work. A native regression asserts two
residents with identical sliders but Goofball versus Shy traits
produce different follow-up actions, and illness still supersedes
humor. Actual per-console creator persistence remains a separate
verification requirement.

## Shared venue activities after a real meetup

Selecting **Meet up with [resident]** at a physical destination now opens
three options: **Catch up and chat**, a named local activity (e.g.
**Share snacks (6 H$)**, **Listen to the music**, **Walk together**), or
**Not now**. Cancelling does not consume the accepted invitation.

A plain chat retains the earlier Social/Happy/relationship benefit.
A local activity modifies additional real resident/player needs by
location: café snacks restore Hunger and Energy while costing **6 H$**;
concerts raise Happy and cost Energy; church visits restore Sanity;
workshop tinkering costs Energy and lifts Happy; walking in parks or
on beaches boosts Happy but uses Energy; browsing at shops improves
Happy without silently granting inventory. The existing venue
restrictions and single-use, same-day appointment gate still apply.
The action is committed only once **at the physical location**,
not at invitation time or while the options are merely visible.

Free Play shows each invited resident who is still at the venue as an
individual meetup choice. Story doors show a single meetup choice when
the place menu has room. Both keep the named guest through confirmation.
If the guest leaves before the player confirms, another resident at the
same venue keeps their appointment. In Talk, **Meet today** checks the
resident you are speaking with and names the actual destination.

After a completed meetup, **Our history** lists remembered venues for
the resident and the character who went with them. The newest distinct
venue comes first. **Remember when we...?** uses their newest matching
visit. The response reflects the resident's current mood and personality.
Recounting a visit adds no relationship points, items, or headlines.

**Options > Our history > Outing journal** lists actual outings with the
selected resident. Entries carry the island calendar date and can be
browsed by page, with shortcuts to the oldest and newest pages. A
return trip on a different day gets its own entry. An identical same-day
trip is recorded once.

The optional `TVIS` v2 chunk holds five bytes per outing. When it fills,
the journal keeps recent trips in full and thins crowded older months.
It keeps a representative visit for each older month when space allows,
preferring to trim repeated outings first. Older `TVIS` entries remain
readable as undated outings, and saves that predate the archive can
import dates for completed visits still in Town News. Cross-console
loads keep the newest compatible records within the destination budget.

| Platform | Outings per save | Approximate span at one outing per game week across the island | Maximum payload |
|---|---:|---:|---:|
| GBA | 112 | 2.3 years | 627 bytes |
| PS1 Talk profile | 384 | 8 years | 2,051 bytes |
| Dreamcast | 768 | 16 years | 4,099 bytes |
| PS2 and native | 1,536 | 32 years | 7,939 bytes |

One game year is 336 days. The span depends on how often the town
records outings. A daily-outing stress test retains an early trip for
8 years in the PS1 profile, 16 on Dreamcast, and 32 on PS2. The archive
preserves samples of those older months as the island gets busier.
Outing dates cover the first 195 game years.

**Options > Life Sim > Elder memory lapses** is off by default. When
enabled, some elders develop occasional difficulty recalling outings.
The game tracks progression from their first shared outing or memory
conversation with the option enabled. Over time, residents can recall an
outing clearly, remember part of it, or ask for help placing it. The
same day's reply is consistent across reloads. Selecting a dated journal
entry can help them recall details. The journal preserves the shared
record, and the simulation updates recall only during relevant actions.

Moving the Talk cursor, changing speakers, and clicking a choice restart
the pause timer. A resident can start a spontaneous exchange after a
full quiet window. The conversation's limit on idle remarks still applies.

The activity itself runs on menu selection. It adds no per-resident
data or 3D render overhead. The story choice uses callback 131 and the menu reserves
its final slot for **Leave** even on crowded screens. Native action
tests check insufficient café funds, real Hunger and money changes,
retained pending invitations after refusal, and no duplicate rewards.

This is a concrete venue-aware activity simulation, not yet a full
walk-together animation or PS1/Dreamcast neural language model.

## Every visible help/give intention reaches a real gameplay action

The original **Be nice -> Offer help** and **Gestures -> Give** actions
could previously send a polite sentence or `*holds something out*` even
when nothing actionable happened. Both now open the same verified
**Help with their needs** and **Give a gift** menus used by dedicated
Talk buttons. Quick replies and recognised typed `TA_OFFER`,
`TA_GIVE`, or `TA_INVITE` intentions route to these menus as well,
instead of implying supplies changed hands or a visit was planned.

The menus check actual resident status, inventory, minor-room rules and
venue eligibility. Opening a menu alone never consumes an item, changes
needs, or records a meeting. Native controller tests exercise both
category paths, including no premature item consumption. The change
uses no new resident save fields, persistent RAM, or per-frame work.

## PS1 disc-backed Talk integration

The experimental PS1 game disc uses `-DDOOMO_PS1_TALK=ON`: the shared
`SCENE_TALK` is compiled with `DOOMO_HAS_TALK`, and the existing
PS1 overlay planner groups its scene code instead of forcing the
entire interface to stay resident during combat. The compiled
`TALK_EN.BIN` language database is placed at the ISO root. A new
`ps1/src/talkdisc.c` implements the shared Talk storage hooks with
one aligned **2048-byte CD sector** in main RAM; it supports reads
across sector boundaries and fails safely if the disc file is missing.
No giant Talk bank is linked into the 2 MB PS1 RAM. Typed/keyboard
input remains disabled on PS1; the controller uses contextual quick
replies, topic pickers, creator traits, gifts, care and outings.

Default PS1 game and engine-test discs exclude Talk. The linker hotfix
combines feature advertising in one `plat_features` definition and uses
the regular progress bar on PS1. CI linked the Talk-enabled ELF at
`478299a4`, then the disc packer rejected its image and BSS end address
of `0x8020ea10`. It needs at least 125,456 B less to reach the 64 KB
stack reserve. Budget work and a controller playthrough remain open.
[Talk-enabled PS1 build log](https://github.com/masong0508-lab/DOOMOGACHI-LIFE-in-HELLvile-park-exclamationmark/actions/runs/44870223640/job/136743009926)

## Dreamcast functionality checks extended

Dreamcast's Flycast-vs-native Talk script now expects the current
**Add someone to chat** label and asserts that **Ask to go somewhere**,
**Play together**, **Show an item**, **Give a gift**, and **Help with
their needs** are reachable with the controller. The repo's existing
`bone-test` has been made mandatory in `dreamcast.yml`: it boots
the shipped CDI, enters Bone Cathedral and Bone Capital, switches
first-/third-person cameras, and captures true emulator screenshots.
It supplements the existing VMU, audio, disc, Talk, story and family
tests rather than replacing them. Passing is required before calling
the Dreamcast package playable; a successful ELF/CDI link alone is
not sufficient.
