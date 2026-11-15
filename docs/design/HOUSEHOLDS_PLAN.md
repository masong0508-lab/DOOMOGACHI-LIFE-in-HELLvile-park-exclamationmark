# HOUSEHOLDS FOR EVERYONE: design note (HH-0)

Spec: `held/OWNER_DECISIONS.md`, entry "HOUSEHOLDS FOR EVERYONE, IN THE CREATOR" (2026-11-15) and its
addition, the Create-a-Sim style relationship editor. Stream: HOUSEHOLDS (a second CREATOR machine).
Status block: `held/status/CREATOR.md`, "HOUSEHOLDS". Defaults below are chosen by Claude (HOUSEHOLDS),
the owner may override.

## 1. What exists today

* One household: the player's. `family.h` / `family.c`: a table `fam_[]` of up to the cap (3, then 10,
  then 20; GBA 3 / 6 / 10) members, each a resident id (0..999) or a **pool** id (1001..1020, GBA
  1001..1010: a person or pet that lives only in the household, 20 B identity). Roles are relative
  to the player (partner, child, parent, sibling, grandparent, housemate, pet). Bond / mood nibble,
  loss, pets, free will, disasters all hang on this one table.
* Saved in the `LIFE` chunk (sections FAMILY, POOL, BOND, LOSS). That chunk is full (2,688 of 2,688 B).
* "Add a member" is a wizard in `fp_family.c` (neighbour moves in / new person / new pet / baby).
* The creator (`SCENE_CREATOR`) makes or edits one resident per run (`arg >= 0` edit in place, empty
  slot creates, `-1` / `-2` first free slot of a district, `-3` the protagonist).
* The town has a sparse relationship graph (`town.h`: `REL_*` kinds on up to 1024 / 2048 edges, crush
  flags). `sim_romance_ok()` is the one gate for romance; `RES_STAGE()` is the life stage.

## 2. Data model

A **household** is a head plus up to **20 members** (`HH_MEMBERS_MAX` 20, the head counts, all
platforms). Two kinds:

* **Household 0, the player's.** Unchanged table (`fam_[]`, pool, bond, loss, pets, free will). Its
  cap becomes 20 on every platform (the three tiers no longer limit it; the tier unlocks still give
  their toast and the pet uplift). The GBA pool grows from 10 to 20 ids (+290 B EWRAM:
  `fam_` 80, `bond_` 10, `ident_` 200). Old saves load as they are.
* **Residents' households (1..).** Head and members are **residents** (ids 0..999; 500 live on the
  GBA, 1,000 elsewhere). No pool ids, no pets in the pool (a pet of a resident household is a
  resident with a pet species; "pet" as a relation only). Records exist only for a household of two or more people
  (a lone resident is their own household and costs nothing).

```c
typedef struct HouseRel { uint8_t a, b, rel; } HouseRel;   /* slots in the record; rel = HREL_*: a is b's rel */
typedef struct HouseRec {
    uint16_t head;               /* resident id = member[0]                        */
    uint8_t  n;                  /* 2..20                                          */
    uint8_t  flags;              /* 0 (HF_LOCKED later)                            */
    uint16_t member[20];         /* display order = list order; member[0] is head  */
    uint8_t  nrel, spare;
    HouseRel rel[32];            /* the relationship editor's pairs                */
} HouseRec;                      /* 4 + 40 + 2 + 96 = 142 B, 144 with padding      */
```

* `HH_MAX_RECS` = **24** residents' households on every platform (parity; 24 x 144 = 3,456 B resident).
  Household 0 owns one more small record: its pairwise relation list only (`HouseRel rel[32]`, 98 B);
  its members stay in the family table.
* A resident is in **one household at a time**: `household_of(id)` returns 0 (player's), 1..N, or -1
  (alone). Found by scanning the records (24 x 20 compares: cheap, used by Talk and the sim per day,
  never per frame). A resident can **join** another household (`household_move`); leaving the old one
  first. A head who leaves passes the head to the next member; a household that drops to one person
  is dissolved. A member of household 0 who "moves out" is removed from the family as today.
* **Roles are derived, not stored**: a member's role toward the head is `rel(head, member)`. For
  household 0 the old `FamilyMember.role` is the stored role; a pair with no explicit `HouseRel` falls
  back to it (PARTNER -> PARTNER, CHILD -> the player is the child's PARENT, and so on). So old saves and
  every old caller keep working and the editor only adds rows.
* **Relations** (`HREL_*`, "a is b's ..."; the reverse is a pure table, shown with the member's
  pronoun: mother / father / parent): SPOUSE, PARTNER, FIANCE (symmetric), PARENT <-> CHILD,
  STEPPARENT <-> STEPCHILD, SIBLING, STEPSIBLING (symmetric), GRANDPARENT <-> GRANDCHILD,
  AUNT_UNCLE <-> NIECE_NEPHEW, COUSIN, ROOMMATE (symmetric), PET_OWNER <-> PET. Setting one writes one
  row (replacing any row of the same pair); `household_rel(h, a, b)` returns it or the derived
  one. Derived (tree only, never stored): grandparent through two parent steps, and so on.
* **Feelings** are not stored in the household: they are edges of the town graph (`REL_FRIEND`,
  `REL_BEST_FRIEND`, `REL_RIVAL`, `REL_NEMESIS` for "enemy", `REL_CRUSH_*` flags), set through the
  existing edge call. No new save bytes; they count against the 1,024 (GBA) / 2,048 edges.

### Saving

A new save chunk **`HOUS`** (its own tag, version 1; the `LIFE` chunk stays as it is). Worst case
3,400 B (`SAVE_EXT_MAX_HOUS`, checked in `test_save_budget.c`; the creator refuses to add a household or
a relation that would not fit: "No room in town for more households"). Layout: `count u8` then per
household `head u16, n u8, flags u8, n x member u16, nrel u8, nrel x (a, b, rel)`; household 0's
relation list is record 0 with `head = 0xFFFF`. Nothing is written while there is nothing to say
(an island without residents' households and without relations writes no chunk, byte for byte as
before). Old saves have no `HOUS` chunk and load unchanged. On load every id is checked: a resident
who no longer exists drops out; a record under two people dissolves; a resident named twice stays in
the first record only. Migration is therefore "nothing to migrate": today's household is household 0.

### Memory (resident bytes, reported per platform in every package)

| | household 0 growth | records | total |
|---|---|---|---|
| GBA EWRAM (share 6 KB, shared with the CREATOR lead) | +290 B | 3,456 + 98 B | about 3.9 KB; the creator pages draw from the scene scratch arena |
| PS1 resident (CREATOR share 21,504 B, 224 B left) | 0 (already 20) | 3,554 B | **needs 3.5 KB from the reserve** (about 6.5 KB): asked of the coordinator; if refused, `HH_MAX_RECS` is 12 on the PS1 (1.8 KB) |
| Dreamcast, native | 0 | 3,554 B | plenty |

The GBA ROM and the PS1 disc take the code (about 6 KB of code, PS1 code goes into the creator
overlay group, not the resident EXE).

## 3. Rules (the hard ones; tested)

`household_rel_check(h, a, b, rel)` returns 0 or a reason code (the creator shows the reason as one
plain line). It reads `RES_STAGE()`, `sim_romance_ok()` and the kid-cast list; it **never writes**
`mature_adult_ok` or the kid-cast list.

* SPOUSE, PARTNER, FIANCE and a crush: **both adults** (ADULT / ELDER), never with or between minors,
  `sim_romance_ok` true both ways, not kin, at most one of spouse / partner / fiance each.
* PARENT, STEPPARENT, GRANDPARENT, AUNT_UNCLE: the older side must be an **adult**, and strictly older
  than the child in stage (BABY < TODDLER < CHILD < TEEN < ADULT < ELDER); no cycles (nobody is their
  own ancestor, checked to depth 20); a parent of a parent is a grandparent, not a second parent.
* A minor is never a ROOMMATE of an adult (the household names them child, step-child or ward);
  minors may be roommates of minors.
* SIBLING, STEPSIBLING, COUSIN: no age rule; never romance (set by the other rows' gate).
* PET: the pet has no other relation; the owner is a person.
* Crush feelings: adults only, both ways, `sim_romance_ok`; refused with a plain line for a minor.
* Talk keeps its own rules (adults-only romance both ways, refusal / deflection with minors, no
  teen-teen romance, a hug toward a child is a plain hug the child may decline). The editor makes
  nothing that Talk would refuse.
* A new person made for a household obeys the creator's own age gates for parts (CR12).

## 4. Where a new person lives ("a new member takes one of the island's resident places")

`household_new_person(h, role)`: the first free slot of the district (`town_alloc`); if the town is
full, the resident with the lowest friendship level who is not in a household, not a story or
premade-roster citizen, not a minor and not the avatar **moves away** (news line "X moved away to
make room", `town_free`). A minor is never chosen to leave. If nobody qualifies: "The island is full."
The creator then edits that slot in place (`SCENE_CREATOR arg = slot`), as today.

## 5. The creator flow

New file `core/src/creator_house.c` (HOUSEHOLDS owns it; `creator.c` gets one hook,
`creator_house_open(h)`, agreed with the CREATOR lead). Reached from the creator's START menu
("Household") and from the Family dashboard's former "Add a member" row.

* **Household strip** along the top: up to 5 chibi portraits (GBA 240 x 160), 7 (PS1 / DC
  320 x 240), scrolling to 20, the head first, the cursor member's name, life stage and role under
  it, "n / 20" at the right. The strip is drawn with the creator's retained chibi path (no whole
  portrait in a frame: S3-2 rule), so it holds 30 fps with a direction held.
* **Below:** the cursor member's card (look preview, name, stage, relation to the head) and one
  action list. Actions: **Add member** (A new person: creates the slot, runs the creator on it; A
  citizen moves in: the roster picker with the stage / district filter; a baby is born (needs a
  partner); a new pet), **Edit** (the creator on that member), **Reorder** (hold A, L / R moves
  them along the strip), **Relations**, **Make head**, **Move out**, **Switch household** (next /
  previous household of the island, so any resident can head one: "Start a household" on a resident
  makes a record with them as head).
* **Relationship editor**, three pages (L / R): **Pair** (pick A with the cursor and A button, pick
  B the same way, the list shows only the relations the rules allow, each line "Ana is Bo's mother";
  the line under it shows the reverse that was filled in: "Bo is Ana's child"; a refused relation
  is greyed with its reason), **Tree** (the family tree preview: generation rows from the
  relation graph, a heart-line between a couple, at most 20 chibis in 4 rows, the cursor member
  highlighted), **Feelings** (a member and any citizen: friend, best friend, rival, enemy, crush for
  adults only).
* One plain instruction line and a control card on each page (comprehensibility rule). Texts fit
  (text-fit gate of the UI stream; every string measured per platform).
* The old wizard in `fp_family.c` (kind picker, name, pronoun, stage, role) is removed; its
  "Add a member" row opens the household view. Care for everyone, member menu, memorial stay.

## 6. Use in the game (HH-USE)

* **Sim**: household members share a home (`house_api.c` assigns them one apartment / house id), eat
  and rest together (the activity pick leans toward the household's), and family events (birthday,
  new baby, moving in / out) write news lines. Household 0 keeps its F-systems.
* **Talk** (cross-stream, small, marked): `talk_gen_rel_band(a, b)` returns the family / housemate band
  when `household_rel_any(a, b)` says so; the rules of Talk are unchanged.
* **Houses and levels**: the island's house screens list who lives there from `household_*`.
* **News**: moved in, moved out, baby, wedding (adults), head changed.

## 7. Packages (rounds of the wave runner, one push each)

| Package | What | Done when |
|---|---|---|
| **HH-0** | this note, the OWNER_DECISIONS defaults, the status block | landed |
| **HH-DATA** | `household.h/.c`, cap 20 everywhere, the `HOUS` chunk, join / leave, old saves load, `household_new_person`, tests (about 40), GBA / PS1 byte report | core tests, SAN, check-gen, GBA + PS1 builds, save-budget test green; memory table above measured |
| **HH-CREATOR** | `creator_house.c`: strip, add / edit / move in / out / reorder / make head, old wizard removed | runner walk-through shots per platform, 30 fps with a direction held, texts fit |
| **HH-REL** | the relationship editor (pair, tree, feelings), reverse fill, the rule checks | tests for every rule above, shots, 30 fps |
| **HH-PETS** | pet families (section 9): the pet pool, creator page, care, save section | pet tests, shots, 30 fps, bytes per platform |
| **HH-USE** | sim, Talk band, houses, news | sim tests, Talk census unchanged, news lines |

HH-CREATOR and HH-REL both need HH-DATA. HH-USE needs HH-DATA only. They touch different files, so
HH-CREATOR + HH-REL + HH-USE may run side by side after HH-DATA lands.

Every package: 30 fps on every screen it touches, every text fits, before / after screenshots,
docs (`docs/CREATOR.md`, `docs/PLAYAS.md`), `ROADMAP.md`, old saves load.

## 8. Defaults chosen (owner may override)

1. The cap is 20 for the player's household from the start (the 3 / 10 / 20 tiers stop limiting it).
2. Residents' households hold residents only; pool ids stay the player's.
3. Up to 24 residents' households (a lone resident needs none); at most 32 relation rows each.
4. New people take a free resident place, or a low-friendship adult moves away; never a minor, a
   story citizen or the avatar.
5. A minor is never a parent, partner, spouse, fiance or crush; a minor is never an adult's
   roommate.
6. Feelings are town-graph edges, not household data.

## 9. Pet families (owner entry BOREAGOTCHI FAMILIES, 2026-11-15) - package HH-PETS, after HH-REL

Every Boreagotchi (the player and each resident) can have up to **8 pet family members** (parent,
sibling, offspring, partner; pet-only relations `PREL_*`). A pet family member is a small record,
not a resident:

```c
typedef struct PetFam {      /* 24 B */
    uint16_t owner;          /* resident id, or DOOMO_PLAYER_ID; 0xFFFF = free slot   */
    uint32_t seed;           /* the pet's look seed (pet*.c draws it as today)        */
    char     name[12];
    uint8_t  rel, kind;      /* PREL_*, FAMPET_* (family.h)                            */
    uint8_t  fed, mood, bond;/* the few care stats (0..15 each, derived loss rules unchanged) */
    uint8_t  spare;
} PetFam;
```

* One shared pool of **`PETFAM_MAX` = 48** records for the whole town (all platforms), so 8 per
  Boreagotchi is the cap per owner and 48 is the cap at once: six full families, or many small ones.
  Pool size is a compile-time constant. Resident bytes: 48 x 24 = **1,152 B** on every platform
  (GBA EWRAM from CREATOR's share; PS1 resident: the coordinator's reserve, asked with these numbers
  before HH-PETS is built, the HH-DATA 3,584 B approval does not cover it).
* Saved as a second section of the `HOUS` chunk (nothing written while the pool is empty), +1,160 B,
  so `SAVE_EXT_MAX_HOUS` is about 4,600 B.
* The creator household view shows the owner's pets in a second strip row under the people (same
  scrolling strip), with add / edit (look seed picker with the existing pet look, name, relation) /
  move out. At the 8 cap or the 48 pool cap the creator says so in one plain line ("Pip has 8 pets
  already", "The island has no room for more pet families").
* Care and look use `pet*.c` / `family.h` pet code (cross-stream, small, marked); the player's
  household pets stay the existing pool pets of household 0 and count toward its 20; the 8-pet
  family is separate from that (a Boreagotchi's own family).
* Kid and content rules apply as for pets today: nothing suggestive, a pet's loss is gentle and off-screen,
  no harm shown to a child.
