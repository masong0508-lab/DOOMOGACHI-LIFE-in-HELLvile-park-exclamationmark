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

## Implemented: a promised visit now has a playable destination

The previous Talk integration already had day-scoped accepted invitations and a
real two-choice meetup at a Story Mode place door. This pass extends that
same event to **the illustrated Free Play City Map**, including the park,
beach, café, concert hall, workshop, church, and shop.

- After a resident accepts an invitation, their Talk menu and **Our history**
  show the actual venue. A button gives a route hint. Reading it does not
  spend the appointment.
- Opening the matching location on the Free Play City Map now reveals
  **Meet [name] here!** only if the invited resident is still physically
  doing that activity. Choose a free conversation, a local experience,
  or leave without accepting.
- Paid café snacks cost 6 H$ (no payment or consumption if funds are missing).
  Local venue actions reuse `talk_outcome_meet_kind`: actual stats and
  relationship changes, real visit memory, and a one-shot consumed appointment.
- The same reservation is accessible from Story Mode and Free Play: using
  either one consumes it on both. A missed visit can be distinguished
  from a still-valid plan; it never teleports an absent citizen.
- Tests cover venue inspection, changed schedules, day rollover, one-time
  attendance and the actual Free Play controller/menu interaction.

**Limitations:** pending invitations still use the existing non-saved,
day-scoped Talk outcome ledger. They do not survive application restart.
A completed experience is remembered in the existing saved shared-memory
flags. Future work on persisted next-day plans must explicitly version the
save format and verify every port's save budget before adding bytes.

No per-frame crowd scan, new sprite, dynamic allocation, full-screen texture,
or new simulation scheduler is introduced.

## The Tomodachi-feel pass: resident quirks with real follow-ups

Nintendo's *Ask the Developer* Vol. 21, Parts 2-3, describes two useful
principles: the funny details are **independent of broad personality types**,
and even players who do nothing but observe should enjoy the evolving
community. This project adopts those principles, not the original assets.

- The new **Tell me one little oddity** option lets a living, willing
  resident make an original surreal remark about their *actual* favorite
  food, hobby, or music. Stable identity/catchphrase determines delivery;
  the player learns one previously unknown real preference when possible.
- **Our History** displays known food, hobby, and music details as
  conversational follow-up choices. These are backed by existing TalkMem
  known-preference bits, not a new save format. The original quick reply
  order and goodbye placement are unchanged.
- Residents who are visibly sad, scared, angry, or having a heated/grief
  conversation are not forced into throwaway jokes. Care first.
- The public town headline now distinguishes a *verified favorite-venue*
  meetup from an ordinary visit. A cross-port guard excludes the headline
  from configurations that compile Talk outcome code but not the Talk
  news templates.
- Original flavor should remain warm, grounded in actual state, with
  Hellvile's oddness as contrast. No copied Mii dialogue or image assets.

Next: more visit-specific behavior tied to actual favorite activities,
and low-cost observational moments; verify all handheld budgets first.

Sources: https://www.nintendo.com/us/whatsnew/ask-the-developer-vol-21-tomodachi-life-living-the-dream-part-3/
https://www.nintendo.com/us/whatsnew/ask-the-developer-vol-21-tomodachi-life-living-the-dream-part-2/

## Implemented: opt-in local vignettes

On the Free Play City Map, places with residents present offer
**Catch a little moment**. The existing crowd count gates the row; pressing
it samples up to two residents who are *still actually there*. Friendly
pairs, rivals and strangers get different small two-line situations.
Solitary characters may simply daydream, with a subdued response when
sad. The selection and relationship check happen only on deliberate
input, never during per-frame rendering.

Observing doesn't raise relationship values, mint news, consume goods,
force romances, or add another hidden quest. It is deliberately quiet,
warm and slightly uncanny: the player can enjoy the world without
treating every scene as a task to complete. An integration test confirms
co-location and no forced bond update.

### Tone refinement: sincere oddness instead of clever one-liners

The Nintendo development team's Part 1 interview describes an important
ingredient: residents shouldn't always know they are being funny. The
player notices that an utterly earnest thought is bizarre. This pass
rewrote the short oddity dialogue to sound more innocent and particular,
and lets observing pairs use the *actual* first resident's hobby/food
rather than one interchangeable stock joke. The existing grief/difficult-
mood gate remains. Scenes still emerge only from real co-location.

### Incident safety: uneasy moments remain in-world

The observation picker rotates by real venue, day and game-hour,
prefers an *existing* friend/rival/family edge when available, and limits
per-button relationship probes. It does not invent a connection or run
an ongoing social AI task each frame. A resident in the ghost,
soul-lost or possessed state is not treated as an ordinary lively comic
partner; if no living citizen is present, the player hears a more
appropriate, quiet and uneasy line instead. A controller-driven
regression preserves this status.
