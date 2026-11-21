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
* **Conversation recall**: the existing saved 24-person, 12-byte-per-entry
  Talk LRU can cue the last topic at a neutral opening, but an active
  subject always wins. It does not claim to store a transcript or remember
  every one of 1,000 citizens; eviction is explicit.
* **Related transitions**: the quick-choice generator considers a fresh
  topic only when it is in the current concept's curated association list.
  While a thread is active, quick choices prefer continuing it. Every
  option still goes through `talk_dm_say` / `talk_dm_step`, rather than
  inventing a response outside the dialogue engine.
* **Emotional context**: the existing saved mood and resident need meters
  now influence offered choices for hungry, sick, tired, lonely or scared
  residents without a new per-resident emotion structure. Mood, needs and
  romance/age gates still belong to the simulation.

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
versioned Talk save-chunk round trip and do not enlarge any of the 24
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

The supported physical place menus are cafe/stalls, church, shop,
workshop, concert hall and vape shop. Park and beach invitations still
update the resident's activity, **but are not yet claimable at a place
menu**, because those locations lack an exact corresponding building
trigger. The rendezvous currently expires on day rollover and does
not survive loading a save partway through the day. Those are honest
limitations, not features to pretend completed.

This is a *location-validated interaction*, not yet a scene with both
characters visibly walking together, or full family/group outings.
Future steps: outdoor park/beach interaction zones, place-aware NPC
spawn presentation, saved pending appointments, and several activity
choices per venue. The new native action test verifies the right
venue, single-consumption behavior, changed needs and shared memory.
No 3D raycasting, NPC arrays or per-frame work is added.

### Six-choice dialogue safety

The normal Story place picker has **six** slots (the UI's
`DLG_OPTS` constant); any seventh choice would be silently dropped.
The new meetup row therefore appears only when there is capacity
*after reserving a slot for Leave*. Existing place options always take
precedence, and menu size remains six on all ports. This prevents a
missing Leave option or a soft lock at a crowded building menu.
