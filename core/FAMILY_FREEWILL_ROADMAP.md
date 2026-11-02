# Family free will, switching and story roadmap

Owner: freeplay / life. Written after F2 (family dashboard) landed.
Goal: the household is a living cast. You can care for everyone, switch to
play as any member, and while you are not controlling them they live on their
own: they look after each other, keep and grow the relationships you built,
can help (or hurt) the story, and can be lost to the story or to random
disasters.

## Ground rules (from SCRIPT_STYLE R5/R6 and family.h, not negotiable)
- A minor is never shown dying. A child, toddler, baby or teen can be hurt,
  sick, scared or "move away" (FAMLOSS_GONE); only adults, elders and pets
  can die, and never as a joke.
- Every loss goes through `family_lose()` so it leaves a memorial, shows a
  toast, and respects the Loss option (`family_loss_set`).
- Nothing new is written to a save while the household is empty. Any new saved
  byte is a new LIFE section (add-only) and must pass test_save_budget.
- Everything must run on the GBA (household cap 10, no big tables, no heap).

## Done
- **F2 v7**: Family screen, add-member wizard, memorials, island menu entries.
- **F2 v8 (this step, the easy part)**: Care for everyone is now a real menu
  (Quick care that gives each member what they need most, Choose the care...
  play / talk / clean / comfort, and Check on everyone, a short report of who is
  low on what). The list marks members who need care and tags the one you are
  controlling. "Play as them" is in the member menu; it explains itself until
  the play-as package lands, then switches for real.

## Phase 1: Switching (needs PA1)
Depends on: PA1 (playas.c: `playas_switch`, `playas_available`, `playas_can_be`).
Today these are weak defaults, so nothing can be switched yet.
- Family screen: "Play as them" already calls `playas_switch(id, TAGIN)`.
- Add a recent-characters strip (`playas_recent`) so you can hop between the
  family and your hero in one press.
- Story stays the hero's (`playas_story_locked`): while you play a family
  member, story writes are refused. Free play has no such lock.
- Size: S once PA1 exists. Tests: journey that switches and switches back.

## Phase 2: Free will for members you are not controlling (F7)
A small daily pass, run from sim.c `day_rollover` next to `family_loss_daily()`,
skipping `playas_avatar()`.
- Residents already have the sim (needs, moods, relationships); they simply keep
  living. New: members of the household tend each other (an adult feeds or
  comforts the lowest-need minor or pet; a partner comforts a low partner).
  That calls `family_care()` so neglect stops, with a news line now and then.
- Pool members (people and pets who are not citizens) have no sim record, so
  they need a tiny state: **bond** (0..15) per member, and a **mood** nibble.
  Needs one new LIFE section (`LIFE_SEC_BOND`, 1 byte per member, 20 B worst
  case, 10 B on GBA) rather than touching `FamilyMember`.
- Autonomy level option (Off / Gentle / Lively) in Options, saved with the
  other options. Off = today's behaviour.
- Size: M. Risk: save budget (small); sim determinism (use the town RNG only).

## Phase 3: Relationships that carry on (F8)
- Residents: their friendships, crushes and couples already advance in the sim;
  make sure household members count (no skipping because they "live with you").
- Pool members: bond with the player and with each member grows when they are
  cared for or spend the day together, decays slowly when ignored. Bond gates
  events (a child confides, a sibling squabbles and makes up) and endings facts
  (`life_ending_facts`: kids, best friends).
- A "Family" tab line per member: "Close to Pip", "Wants more time with you".
- Romance rules unchanged: `playas_romance_ok`, LS0 stage gate, minors never.
- Size: M. Depends on Phase 2's bond byte.

## Phase 4: Contributing to the story (F9, with LS3 / LS4)
- Household members can help story beats once the story is cleared: an adult
  member with a skill (job, hobby, persona) can be "away helping" for a day and
  bring back an item, money or a news line; a bond-5 child can leave a small
  gift. Uses the existing life-event engine (LS3), reserved callback block
  `CB_LIFE_FIRST..CB_LIFE_LAST` (16 callbacks), not new story code.
- Never required for the main story; it is a bonus, so skipping it costs nothing.
- Size: M-L, mostly writing (events text, English first, Spanish batch later).

## Phase 5: Story deaths and random disasters (F10, with F6 and LS3)
- Disasters (storm, fire, illness wave, demon night, power cut) are daily rolls
  at a low chance, weighted by the Loss option and the hero's progress. Each is
  a life event with warning, a choice for the player when in control, and an
  automatic outcome when not.
- Outcomes by stage: adults and pets can be lost (`family_lose` with
  FAMLOSS_ILLNESS / FAMLOSS_DEMON); minors are hurt, scared or sent away to
  safety (FAMLOSS_GONE), never shown dying. The player always gets one
  chance to prevent it: a warning day, an item, or being home.
- Story hooks: certain chapters can put a household member in danger as a
  story beat. Those use hand-written scripts (no random death), flagged so that
  Loss Off turns them into a rescue.
- Memorials (already in F2) show them afterwards; the endings read the count.
- Size: L. Needs a content review pass (tone), tests for every rule above.

## Order
1. PA1 lands (not ours) -> Phase 1 (tiny).
2. Phase 2 and 3 together (one new save section, one daily pass).
3. Phase 5 disasters without story hooks (random events are self-contained).
4. Phase 4 and the story hooks of Phase 5 with the LS3/LS4 writing.

## Decisions (owner, 1 Nov 2026)
- **Hub shortcut**: SELECT on the island hub now opens Family (reserved journey
  `life_care_all`). The Doomogachi moves to the START menu (journey `life_pet`
  is now START, Down, Down, A). Inside Family, SELECT/L/R still flip the tab.
- **Autonomy**: a "Family free will" level, default **Normal**: Off, Min,
  Normal, High, Subconscious (the most, and silent). Set from Family >
  Care for everyone > Family free will. Saved in the family section's spare
  header byte (0 = Normal), so the default costs nothing and old saves load as
  Normal. Daily pass: `family_freewill_daily()` in sim.c `day_rollover`, before
  `family_loss_daily()`.
- **Lost partner**: leaves a **grieving** state for 14 town days (derived from
  the memorial, nothing saved): your shots do 50% damage rising back to 100%,
  and hits on you do 125% falling back to 100%. The partner ending route is not
  ended (F6 can still tune the tone).
- **Children**: amended R5. A minor can be lost to illness or Hell (never old
  age or neglect), only while Loss is on, always off-screen and gentle (toast
  "has gone to rest", a memorial). Loss Off keeps them safe (a rescue).
  `docs/design/story/SCRIPT_STYLE.md` still says children never die: it needs the
  same amendment before any story beat uses this.

## Done in this step (Phase 2/3 core)
- `LIFE_SEC_BOND` (id 9): one byte per household member in table order, low
  nibble bond 0..15 (default 3), high nibble mood 0..15 (default 8). Length = member
  count (20 B worst, 10 B on GBA, 4 B header); omitted while all are default. It
  uses exactly the 24 B `LIFE_SZ_SPARE`; test_save_budget passes.
- A care act (yours or a housemate's, `family_care`) raises bond and mood by 1.
  Each day a pool member nobody cared for loses a bond point 25% of the time
  (only while free will is not Off). The Family list shows "Close to you"
  (bond 10+) or "Wants more time with you" (bond 0-1).

## Done: Phase 5 disasters (no story hooks)
- `family_disaster_daily()` (sim.c, after free will, before the loss pass): with
  Loss on, 4% a day to warn about a disaster aimed at a non-avatar member
  (illness wave, demon night, storm, fire, power cut). Warning toast + news now,
  outcome next town day. Care in between (you or a housemate) saves them.
  Illness/demon can take adults, elders and pets; a minor only one time in three
  (else they pull through); storm/fire/power only drop mood. Nothing is saved
  (a load cancels a pending one). Loss off: no disasters.

## Done: Phase 3 bond-gated events
- `family_events_daily()` (after free will, before disasters; Off = none,
  Subconscious = silent): a member you are close to (bond 10+) may confide in
  you (news line, mood +3); two non-pet housemates may squabble (moods -3) and
  make up the next day (moods +5). Words only, kid-safe, nothing saved.

## Done: Phase 4 household helpers (bonus, no LS3 yet)
- `family_help_daily()` (before the events pass): a member with bond 5+ may
  lend a hand. Adult, elder or teen: brings home H$20-60. Child: leaves a
  cupcake, flowers or plushie in your bag. Baby or toddler: a smile (news only).
  3-14% a day by free will level, silent at Subconscious, never when Off, never
  pets or the avatar. In a STORY island it waits until the ending was seen
  (`STORY_CH_DONE`); free play has no wait. Not yet wired to the LS3 life-event engine / `CB_LIFE_*` callbacks.
- No household = no RNG draw in any of these passes (test_sim's news hash).
- `SCRIPT_STYLE.md` now carries the owner's child-loss amendment.

## Still open
- `life_ending_facts` (kids, best friends; LS5's, still a weak default); bond between members (only member-to-player so far).
- Phase 4 via LS3 (away helping, item returns), disaster story hooks (hand-written
  scripts, Loss Off = rescue), and an owner tone check of the disaster, loss and
  event lines (a first read found them kid-safe and non-graphic).
