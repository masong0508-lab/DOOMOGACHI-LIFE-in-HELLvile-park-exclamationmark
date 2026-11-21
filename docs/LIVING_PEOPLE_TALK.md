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
