# Customisation P0.M0a: what landed, what is left (2026-11-05)

SPEC.md section 7 defines M0a as the pure refactor: contract, gene codec, the 96 -> 76 B record repack, roster v2, and the
warm-up bitset, with every golden byte-identical. This patch is the **additive first slice**; it changes no behaviour, no save
byte and no golden.

## Landed (patch `doomo-p0-m0a-gene-nov05c`)

| File | What |
|---|---|
| `core/include/doomo/gene.h`, `core/src/gene.c` | the 83 gene fields (381 bits + 3 spare) at the SPEC 1.3 positions; byte-wise `gene_get` / `gene_set` (CF-2), clamping; `gene_pack_v1` / `gene_unpack_v1`, the bridge from today's `DoomoResident` (hair_highlight sentinel 0xFF <-> 32, CF-3; `romance` preference half only) |
| `core/include/doomo/look_pool.h` | `DoomoLookPool` layout and constants (8,200 B), static asserts only |
| `core/include/doomo/settings.h` | `DoomoSettings` layout (416 B) and `IC_*`, static asserts only |
| `core/tests/test_gene.c` | 7 tests (9 after the age patch): layout totals (225 / 97 / 59 / 3 spare), unaligned round trips, field isolation + clamp, all 1000 premade and 4000 generated citizens survive v1 -> gene -> v1 unchanged (so every v1 value fits its v2 width, incl. the trimmed `like_hobby`, `like_music`, `catchphrase`), a too-wide value is reported, the life stage stays out of `romance` |

`make -C core test` (before the age patch): 799 passed, 0 failed (was 792), 154 scripts, 19 journeys, `check-gen` clean; `gene.c` also compiles clean with
`-Werror -fsanitize=address,undefined`. Not built with the ARM or MIPS toolchains, not run on GBA or PS1 (none installed here); `gene.c` is
plain integer code with no `.bss`, so it should cost ROM/code only, but the memory gates were not measured.

## Not done (rest of M0a, M0b, M1)

- The repack itself: `DoomoResident` 96 -> 76 B and every `.look.*` / identity reader (about 49 files, a freeze window, `tools/repack_sites.py`).
- Roster v2 (58 B packed records, `roster_premade_record`, bio/job text packs) and the warm-up bitset.
- `tools/look_build.py` + `core/data/look/fields.txt` (the gene table is hand-written in `gene.c` for now) and the 304 deep fields (`DF_COUNT`).
- The weak stubs for `look_pool_*`, `settings_*`, `input_context`, `res_unpack` / `res_pack` / `res_look` (they have no callers yet).
- M0b (age gate, venue table, reviewed sim-golden re-baseline) and M1 (saves v2, pool, derive).

## Decision: the age mapping (settled 2026-11-05, patch `doomo-p0-age-map-nov05d`)

The gene's `age` field **is** the life stage: same numbers as `LIFE_*` (ADULT 0, BABY 1, TODDLER 2, CHILD 3, TEEN 4, ELDER 5), 3 bits, 6 and 7
invalid and read as ADULT (as `resident_sanitize` does). `AGE_*` in `gene.h` are aliases of `LIFE_*`.

- This replaces the spec's `AGE_AUTO, BABY, KID, TEEN, ADULT, ELDER` (SPEC 1.2), which predates the explicit stages. No AUTO: the roster, the
  sim and the family table now set a stage on every citizen. No single KID: TODDLER and CHILD stay separate, so nothing is lost.
- An old record (stage 0) is an adult exactly as today, so migration needs no special case, and the gene and the record cannot disagree.
- The spec's "`BODY_TINY` means kid" migration rule (CF-16) is obsolete for the same reason: the stage is explicit and saved.
- The age gate (M0b) should read `RES_STAGE` / `AGE_*`, and `sim_romance_ok` stays the one romance gate.
- Tested: every stage round-trips (`gene_age_is_the_life_stage_for_every_stage`), and a 20-member tier-2 household with every stage survives
  60 days of `family_age_daily()` through the gene with nobody going backwards (`gene_carries_a_whole_family_through_every_life_stage`).
- Still the same as before: nothing stores the gene yet, so this changes no save and no screen.

## Open questions

1. **Spec baseline is old** (HEAD `8cc7910`, 2026-10-18): since then the tree gained family, play-as, life stages and endings. The
   repack's call-site counts (about 217 `.look.<field>` uses, 49 files) need re-measuring on the current tree.
