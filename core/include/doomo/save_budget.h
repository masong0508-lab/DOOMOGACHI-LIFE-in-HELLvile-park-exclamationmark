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

#endif
