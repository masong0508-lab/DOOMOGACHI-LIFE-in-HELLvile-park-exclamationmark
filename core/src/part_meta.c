/* part_meta.c - lookups over the creator's part catalog (CR1; owner: creator).
 *
 * The tables are generated (core/gen/part_meta_data.c, tools/partcat_build.py); this file only
 * searches them. Everything is const and allocation-free. On the PS1 it is linked into the
 * creator overlay with the data (ps1/tools/ovl_plan.py): only the creator and tools read names. */
#include "doomo/doomo.h"
#include "doomo/part_meta.h"

const PartSlotInfo *part_slot_of_list(int list)
{
    int lo = 0, hi = PART_SLOT_COUNT - 1;
    while (lo <= hi) {                                  /* slots are sorted by list id */
        int mid = (lo + hi) >> 1;
        int l = doomo_part_slots[mid].list;
        if (l == list) return &doomo_part_slots[mid];
        if (l < list) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

const PartMeta *part_meta_find(int list, int value)
{
    const PartSlotInfo *s = part_slot_of_list(list);
    int lo, hi;
    if (!s) return NULL;
    lo = s->first; hi = s->first + s->count - 1;
    while (lo <= hi) {                                  /* rows are sorted by value in a slot */
        int mid = (lo + hi) >> 1;
        int v = doomo_part_meta[mid].value;
        if (v == value) return &doomo_part_meta[mid];
        if (v < value) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

const char *part_meta_name(const PartMeta *m)
{
    return m ? doomo_part_names + m->name : "";
}

const char *part_name_of(int list, int value)
{
    return part_meta_name(part_meta_find(list, value));
}

const PartSetInfo *part_set_info(int set)
{
    return set >= 0 && set < PART_SET_COUNT ? &doomo_part_sets[set] : NULL;
}

int part_set_size(int set, int live_only)
{
    int i, n = 0;
    for (i = 0; i < PART_META_COUNT; i++)
        if (doomo_part_meta[i].set == set &&
            (!live_only || PMF_STATE(doomo_part_meta[i].flags) == PSTATE_LIVE)) n++;
    return n;
}

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* case-insensitive substring */
static int has_text(const char *hay, const char *needle)
{
    int i, k;
    if (!needle || !needle[0]) return 1;
    for (i = 0; hay[i]; i++) {
        for (k = 0; needle[k] && hay[i + k] && lower(hay[i + k]) == lower(needle[k]); k++) {}
        if (!needle[k]) return 1;
    }
    return 0;
}

int part_search(int slot, const char *letters, int live_only, uint16_t *out, int max)
{
    int i, n = 0;
    for (i = 0; i < PART_META_COUNT; i++) {
        int row = doomo_part_name_order[i];
        const PartMeta *m = &doomo_part_meta[row];
        if (slot >= 0 && m->slot != slot) continue;
        if (live_only && PMF_STATE(m->flags) != PSTATE_LIVE) continue;
        if (!has_text(part_meta_name(m), letters) && !has_text(doomo_part_sets[m->set].name, letters)) continue;
        if (out && n < max) out[n] = (uint16_t)row;
        n++;
    }
    return n;
}
