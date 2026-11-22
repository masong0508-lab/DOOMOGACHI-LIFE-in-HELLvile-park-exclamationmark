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

Test through `test_talk_scene.c` using real UI input: the outing picker
must contain an allowed park at noon, and the play choice must change
resident stats. Keep scripted invitation outcome tests and all minor,
status and per-day action caps in `test_talk_actions.c`. Further work:
showing a specific inventory item, giving it with confirmation, moving
the player's avatar with the guest, and recording the outing as an
episodic memory. Do not label these unimplemented behaviors complete.

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

An accepted Talk invitation records its destination in the unused
`LedP.pad` byte of the existing **day-scoped** Talk outcome ledger.
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
The rendezvous currently expires on day rollover and does not survive
loading a save partway through the day. Those remain honest limitations.

This is a *location-validated interaction*, not yet a scene with both
characters visibly walking together, or full family/group outings.
Future steps: exact outdoor interaction zones (rather than Fun Fair
and pier proxies), place-aware NPC spawn presentation, saved pending
appointments, and several activity choices per venue. The new native action test verifies the right
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

The outer `TALK` save registry still uses version 1, and its 12-byte
entry and maximum chunk length are unchanged. The **payload byte 0** now
uses 3 for memory-only, or 4 when there is also an explicit-trait tail.
Old payloads 1 and 2 continue loading, but their high three
`likes_known` bits are masked out, as in the earlier implementation.
Only v3/v4 saves restore the new actual shared-event markers. This
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

The extra work runs only on a menu selection. It allocates no per-
resident data, does not change the save format, and adds no 3D render
overhead. The new story choice uses callback 131 and the menu reserves
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
