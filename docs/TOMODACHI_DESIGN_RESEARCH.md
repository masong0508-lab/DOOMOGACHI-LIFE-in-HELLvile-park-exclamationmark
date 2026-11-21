# DOOMOGACHI: Tomodachi-series lessons, without copying Tomodachi

Research checkpoint: 2026-11-21. This is a design reference, not an asset or code import.
The goal is to make Hellvile's citizens easier to **understand, care for, and get attached to**
while preserving DOOMOGACHI's unsettling setting, distinctive residents, authored stories,
and hardware-appropriate simulation.

## Reference games and the transferable systems

| Reference | Observed design strength | DOOMOGACHI interpretation |
|---|---|---|
| *Tomodachi Collection* (Nintendo DS, 2009) | Expandable building, personal desires, daily shops, recurring town activities and odd little events | Keep resident needs legible, put daily activity at recognizable venues, and let encounters emerge from actual schedules |
| *Tomodachi Life* (Nintendo 3DS, 2013/2014) | Apartments, personality/voice distinctions, advice, friendship drama, favorite foods, concerts, changing local events | Use current personality and memory traits in Talk; show observable consequences rather than hidden scores; connect Town News to real social outcomes |
| *Tomodachi Life: Living the Dream* (Switch, April 16, 2026) | Residents act autonomously, approach others, reveal concerns, explore the town, and form acquaintances that may deepen into relationships | Context-sensitive social action that routes through the existing sim; show *why* a character needs help and what a chosen action actually does |

Primary/secondary references:

- https://www.nintendo.co.jp/ir/pdf/2009/091029e.pdf — Nintendo on DS community relationships.
- https://www.nintendolife.com/news/2014/05/matters_of_import_tomodachi_collection — original DS building, shops, daily events.
- https://www.nintendo.com/en-gb/Games/Nintendo-3DS-games/Tomodachi-Life-871968.html — official 3DS design description.
- https://www.nintendo.com/us/whatsnew/tomodachi-life-living-the-dream-is-here/ — official Switch social interactions and creative facilities.
- https://www.nintendo.com/us/whatsnew/ask-the-developer-vol-21-tomodachi-life-living-the-dream-part-3/ — autonomy, minor quirks, and news as a personality-building device.

## Implemented in this pass: hear the need, then help

The Talk main menu now exposes **On their mind: [real concern]** if and only if the
currently addressed resident has a problem in the sim queue. It is below the existing
quick replies to preserve their one-press budget. The two-option concern page lets the
player listen to the resident's real problem description or follow a contextual action:

- Food / medicine / cleaning / comforting / play route into existing care logic.
- A missing requested item does not disappear or grant rewards just because a player
  looked at the thought bubble. It can be handed in in Talk only once actually held.
- Real friendship and conflict problems use the sim's existing friendship and
  reconciliation behavior; romance advice gives the existing two-way choice, using
  the sim's safety rules.
- Possession does not get a magic Talk cure. The player is directed to the genuine
  exorcism route on the resident's Care screen.
- Each action re-checks the live problem so expired concerns and changed targets
  cannot be exploited for stale rewards.

This reuses the existing `DoomoProblem` queue: **no new save-format field, problem
table, assets, framebuffers, heap allocations, or per-frame NPC scans**.
The same shared Talk source is used by platforms that build the advanced Talk scene;
platforms without that scene keep their existing interaction UX.

## Next steps (proposals, not implemented by this pass)

1. **After-talk consequences:** a small, readable digest of relationship, happiness,
   and newly learned preferences only when those values actually changed.
2. **Social follow-through:** turning an invitation into a visible, scheduled meeting
   with arrival/decline/replanning, rather than an implied offscreen event.
3. **Resident tell:** small, diegetic thought indicators on the island/house view,
   refreshed only on room or roster changes instead of once per frame.
4. **Autonomous town tableau:** pick short, prebudgeted encounters from actual
   locations and interactions; use the news log for rare especially funny outcomes.
5. **Personal quirks:** tiny trait-based speech/gesture variants, respecting safety,
   stage of life and memory budget; no generative network and no long-lived new heap.
6. **Cross-port parity tests:** one canonical scripted journey for need -> Talk ->
   action -> inventory/stat change -> saved/reloaded relationship, with Dreamcast
   emulator shutdown investigated separately from the rendered game loop.

## Acceptance gates

- Native Talk tests, item hand-in tests and Dreamcast Talk scenario stay green.
- No regressions in one-press reply/4-press compliment journeys.
- No new romance-with-minors route or grief-joke bypass.
- No inventory/reward multiplication on a repeated click or stale problem.
- Existing Dreamcast 3D screenshot baselines remain unchanged.
- Inspect worst-case Dreamcast Talk frame before approving any new rendering costs.

**Do not import Nintendo UI, Mii assets, music, exact dialogue, or character designs.**
Reuse the *principles* of legibility, emergent community and meaningful care,
while keeping Hellvile's original tone.
