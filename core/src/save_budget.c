/* save_budget.c - the joint dynamic save budget (save_budget.h; SPEC R13,
 * C9). Owner: S0 -> I9.
 *
 * Resident and tiny: a per-slot snapshot of the last save / load (the file
 * size and every registered chunk's size() at that moment), refreshed
 * lazily when save_last_size() changes. Never runs the encoder.
 */
#include "doomo/doomo.h"
#include "doomo/save_budget.h"

/* 2 x 24 B of .bss (only linked once something asks for a budget): chunk sizes are kept by registry position (stable in
 * a build) and fit 16 bits (each chunk <= DOOMO_SAVE_EXT_MAX). */
typedef struct BudgetSnap {
    uint32_t total;                              /* file size on the medium */
    uint32_t seed;                               /* doomo_town.seed at the I/O */
    uint16_t size[SAVE_EXT_SLOTS];
    uint8_t  valid, n;
} BudgetSnap;

static BudgetSnap snap[2];
static uint32_t seen_last_size;
static uint8_t  seen_init;

static uint32_t slot_cap(int slot)
{
    return slot == 0 ? SAVE_SLOT_STORY_CAP : SAVE_SLOT_FP_CAP;
}

int32_t save_budget_compute(uint32_t cap, uint32_t last_total,
                            const uint32_t *now, const uint32_t *at_io, int n)
{
    int64_t used = last_total;
    int i;
    for (i = 0; i < n; i++) {
        uint32_t a = at_io ? at_io[i] : 0, b = now ? now[i] : 0;
        if (b > a) used += ((int64_t)(b - a) * 129 + 127) / 128;
        if (a == 0 && b > 0) used += SAVE_BUDGET_CHUNK_HDR;
    }
    used += SAVE_BUDGET_MARGIN;
    used = (int64_t)cap - used;
    if (used < -0x7FFFFFFFLL) used = -0x7FFFFFFFLL;
    return (int32_t)used;
}

/* The registered chunks' current sizes, in registry order. */
static int chunk_sizes(uint32_t *size)
{
    int i, n = save_ext_count();
    if (n > SAVE_EXT_SLOTS) n = SAVE_EXT_SLOTS;
    for (i = 0; i < n; i++) {
        const DoomoSaveExt *e = save_ext_at(i);
        size[i] = (e && e->size) ? e->size() : 0;
    }
    return n;
}

void save_budget_note_io(int slot, uint32_t total)
{
    uint32_t now[SAVE_EXT_SLOTS];
    BudgetSnap *s;
    int i, n;
    if (slot < 0 || slot > 1) return;
    s = &snap[slot];
    memset(s, 0, sizeof(*s));
    n = chunk_sizes(now);
    for (i = 0; i < n; i++) s->size[i] = (uint16_t)(now[i] > 0xFFFFu ? 0xFFFFu : now[i]);
    s->n = (uint8_t)n;
    s->seed = doomo_town.seed;
    s->total = total;
    s->valid = 1;
}

void save_budget_reset(void)
{
    memset(snap, 0, sizeof(snap));
    seen_last_size = save_last_size();
    seen_init = 1;
}

/* A save or load happened since we last looked: read the slot's real size
 * from its header and snapshot the chunk sizes now. */
static void sync(int slot)
{
    uint32_t cur = save_last_size();
    DoomoSaveInfo info;
    if (!seen_init) { seen_init = 1; seen_last_size = cur; }
    if (cur == seen_last_size) return;
    seen_last_size = cur;
    if (save_peek(slot, &info) && info.size) save_budget_note_io(slot, info.size);
}

int32_t save_budget_left(int slot)
{
    uint32_t now[SAVE_EXT_SLOTS], at[SAVE_EXT_SLOTS];
    const BudgetSnap *s;
    int i, n;
    if (slot < 0 || slot > 1) return 0;
    sync(slot);
    s = &snap[slot];
    /* another town since that I/O (town_new, a load of another save that
     * happened to have the same size): the snapshot says nothing about it */
    if (s->valid && s->seed != doomo_town.seed) snap[slot].valid = 0;
    n = chunk_sizes(now);
    for (i = 0; i < n; i++) at[i] = (s->valid && i < s->n) ? s->size[i] : 0;
    return save_budget_compute(slot_cap(slot),
                               s->valid ? s->total
                                        : (slot == 0 ? SAVE_BUDGET_TOWN_BOUND_STORY
                                                     : SAVE_BUDGET_TOWN_BOUND_FP),
                               now, at, n);
}

int save_budget_meter(int slot)
{
    int32_t cap = (int32_t)slot_cap(slot), left = save_budget_left(slot);
    int32_t used = cap - left;
    if (used <= 0) return 0;
    if (used >= cap) return 100;
    return (int)((int64_t)used * 100 / cap);
}

int save_budget_fits(int slot, uint32_t extra)
{
    int32_t left = save_budget_left(slot);
    return left >= 0 && (int64_t)extra * 129 / 128 + 1 <= (int64_t)left;
}
