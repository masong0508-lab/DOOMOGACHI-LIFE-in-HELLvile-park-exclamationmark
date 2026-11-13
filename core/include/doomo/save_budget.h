/* save_budget.h - the joint dynamic save budget (SPEC R13, C9).
 *
 * CONTRACT: written by S0, maintained by I9 (add-only). Implementation:
 * core/src/save_budget.c (S0 -> I9).
 *
 * Either customisation design plus a full 'HOME' chunk can exceed the free
 * play slot in the static worst case, so everything that grows a save chunk
 * at runtime (the house editor's placement, photo and blueprint save,
 * home_place_gift, the customisation heap commit) asks first:
 *
 *   left = cap - (last_total(slot)
 *                 + sum over chunks of ceil(max(0, size_now - size_at_last_io) * 129 / 128)
 *                 + 10 * chunks_new)
 *              - SAVE_BUDGET_MARGIN
 *
 * last_total is the file size of the last save written or read for `slot`
 * (header field 8), size_at_last_io each registered chunk's size() at that
 * moment. save.c is not edited (it belongs to the customisation spec):
 * save_budget.c notices a save / load because save_last_size() changed,
 * reads the slot's real size with save_peek() and snapshots the chunk
 * sizes then (the first budget query after the I/O). Callers that know
 * better (the save UI) call save_budget_note_io() directly. The 129/128
 * is the LZ's worst case (one control byte per <= 128 literals), 10 B is a
 * chunk header. The encoder is never run (a counting
 * pass over a 60-90 KB body per placement is far too slow on the GBA).
 *
 * When no save of the slot has been written or read yet, last_total is the
 * slot's measured whole-file bound below (SAVE_BUDGET_TOWN_BOUND_*) and
 * every chunk's current size counts as new on top of it (conservative).
 */
#ifndef DOOMO_SAVE_BUDGET_H
#define DOOMO_SAVE_BUDGET_H

#include "types.h"

#define SAVE_SLOT_STORY_CAP   40960u      /* = DOOMO_SAVE_SLOT0_MAX */
#define SAVE_SLOT_FP_CAP      90112u      /* = DOOMO_SAVE_SLOT1_MAX */
#define SAVE_BUDGET_MARGIN    2048u
#define SAVE_BUDGET_CHUNK_HDR 10u
/* Measured town bodies (compressed, with the 48 B header and the chunks
 * that existed at G0; SPEC C-B / sec.9): the fallback when nothing was saved
 * or loaded yet this session. */
#define SAVE_BUDGET_TOWN_BOUND_STORY 13504u
#define SAVE_BUDGET_TOWN_BOUND_FP    59148u

/* Bytes still free in `slot` (0 story, 1 free play) after the margin;
 * negative when already over. */
int32_t save_budget_left(int slot);
/* The shared "save space" meter: percent of the slot in use, 0..100. */
int     save_budget_meter(int slot);
/* 1 if `extra` more raw bytes in some chunk still fit. */
int     save_budget_fits(int slot, uint32_t extra);
/* Record a save write / read of `slot` with file size `total` and snapshot
 * every registered chunk's size() now. */
void    save_budget_note_io(int slot, uint32_t total);
/* Forget every snapshot. (A snapshot also lapses by itself once
 * doomo_town.seed differs from the town it was taken for.) */
void    save_budget_reset(void);
/* The formula itself, for tests: n chunks with their current and
 * last-I/O sizes (0 at_io = the chunk is new). */
int32_t save_budget_compute(uint32_t cap, uint32_t last_total,
                            const uint32_t *now, const uint32_t *at_io, int n);

/* --- K0: the declared worst cases of the five chunks K0 registers in
 * save_ext.h (LIFESIM_PLAN R1, section 2.5, section 6 amendments). Their
 * owners' strong descriptors set max_size to at most these
 * (core/tests/test_save_budget.c checks it); raising one is a change to
 * this contract, with the joint worst case re-checked. LIFE and PLAY
 * include the section 6 amendments (F5 pets, F6 loss, household tiers,
 * per-villager money / items / health) and PROF the player profiles (MP0);
 * their split is in life_contract.h. --- */
#define SAVE_EXT_MAX_LIFE    2688u    /* 'LIFE' (LS1; F1, LS3, F5, F6 add sections;
                                         life_contract.h LIFE_SZ_TOTAL)      */
#define SAVE_EXT_MAX_PLAY     800u    /* 'PLAY' (PA1; per-villager pockets:
                                         life_contract.h PLAY_SZ_TOTAL)      */
#define SAVE_EXT_MAX_ENDS      32u    /* 'ENDS' (E1)                         */
#define SAVE_EXT_MAX_OPT2      40u    /* 'OPT2' (U0)                         */
#define SAVE_EXT_MAX_PROF    2336u    /* 'PROF' (MP0; 3 guest profiles:
                                         life_contract.h PROF_SZ_TOTAL)      */
#define SAVE_BUDGET_K0_RAW \
    (SAVE_EXT_MAX_LIFE + SAVE_EXT_MAX_PLAY + SAVE_EXT_MAX_ENDS + SAVE_EXT_MAX_OPT2 + \
     SAVE_EXT_MAX_PROF)
#define SAVE_BUDGET_K0_CHUNKS 5u
/* What a chunk of `raw` bytes can cost in the file (the LZ's worst case),
 * and the five together, a header each: 5,995 B. */
#define SAVE_BUDGET_LZ_WORST(raw) (((raw) * 129u + 127u) / 128u)
#define SAVE_BUDGET_K0_WORST \
    (SAVE_BUDGET_LZ_WORST(SAVE_EXT_MAX_LIFE) + SAVE_BUDGET_LZ_WORST(SAVE_EXT_MAX_PLAY) + \
     SAVE_BUDGET_LZ_WORST(SAVE_EXT_MAX_ENDS) + SAVE_BUDGET_LZ_WORST(SAVE_EXT_MAX_OPT2) + \
     SAVE_BUDGET_LZ_WORST(SAVE_EXT_MAX_PROF) + SAVE_BUDGET_K0_CHUNKS * SAVE_BUDGET_CHUNK_HDR)
/* The story slot once U1g carves the GBA's 4 KB config sector out of it
 * (SAVE_SLOT_STORY_CAP becomes this then); the joint worst case is held
 * against both. */
#define SAVE_SLOT_STORY_CAP_CFG 36864u
/* The declared worst case of a K0 chunk by tag (SAVE_EXT_TAG_LIFE, ...);
 * 0 for any other tag. */
uint32_t save_budget_declared_max(uint32_t tag);
/* --- end K0 --- */

/* --- T0 (docs/TALK.md 6.8): 'TALK', the talk memory: 4 B + 24 x 12 B =
 * 292 B worst (talk.h TALK_MEM_WORST), declared 320. PS1, DC and native
 * only (the GBA never registers it), but the joint worst case holds it in
 * both slots anyway (test_contracts_talk.c). save_budget_declared_max() knows
 * its tag too. --- */
#define SAVE_EXT_MAX_TALK     320u
#define SAVE_BUDGET_TALK_WORST (SAVE_BUDGET_LZ_WORST(SAVE_EXT_MAX_TALK) + SAVE_BUDGET_CHUNK_HDR)
/* --- end T0 --- */

/* --- LV0 (levels plan 8.5): 'BRCH', the newer levels' records and the
 * Arrears arc: 4 B header + 48 B arc + 24 x 4 B = 148 B worst (levels_int.h
 * LV_BRCH_MAX; 80 B on the GBA, which lists seven of the 24), declared 160.
 * save_budget_declared_max() knows its tag; the joint worst case with it is
 * held in both slots by core/tests/test_levels_rec.c. --- */
#define SAVE_EXT_MAX_BRCH     160u
#define SAVE_BUDGET_BRCH_WORST (SAVE_BUDGET_LZ_WORST(SAVE_EXT_MAX_BRCH) + SAVE_BUDGET_CHUNK_HDR)
/* --- end LV0 --- */

#endif
