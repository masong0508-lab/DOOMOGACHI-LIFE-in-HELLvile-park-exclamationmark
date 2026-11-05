/* look_pool.c - PROVISIONAL in-RAM deep-look run pool and resolve (P1.M1 shim; see look_fields.h).
 *
 * Replaces the weak stubs of look_pool_find / put / drop (stubs.c). 384 B, never saved or loaded,
 * nothing in the game writes to it yet, so with no run stored the renderer is bit-for-bit what it
 * was. P0 replaces this file whole. */
#include "doomo/doomo.h"
#include "doomo/look_pool.h"
#include "doomo/look_fields.h"
#include "doomo/portrait.h"

#define SHIM_BYTES 384
/* record: id lo, id hi, kind, len, data[len] */
static uint8_t  pool[SHIM_BYTES];
static uint16_t used;

const DoomoDeep doomo_deep_zero = { { 0 } };
const uint8_t doomo_df_flags[DF_COUNT] = {
    [DF_BLUSH2] = DFF_CHIBI, [DF_GLASSES2] = DFF_CHIBI, [DF_EAR_FOLD] = DFF_CHIBI,
    [DF_AURA2] = DFF_CHIBI | DFF_ANIM,
};

static int rec_find(int id, int kind, int *at)
{
    int p = 0;
    while (p + 4 <= used) {
        int rid = pool[p] | (pool[p + 1] << 8), len = pool[p + 3];
        if (rid == id && pool[p + 2] == kind) { *at = p; return 1; }
        p += 4 + len;
    }
    return 0;
}

const uint8_t *look_pool_find(int id, int kind, int *len)
{
    int at;
    if (len) *len = 0;
    if (id < 0 || id > 1023 || !rec_find(id, kind, &at)) return NULL;
    if (len) *len = pool[at + 3];
    return pool + at + 4;
}

static void rec_cut(int at)
{
    int n = 4 + pool[at + 3];
    memmove(pool + at, pool + at + n, (size_t)(used - at - n));
    used = (uint16_t)(used - n);
}

int look_pool_put(int id, int kind, const uint8_t *data, int len)
{
    int at;
    if (id < 0 || id > 1023 || kind < 0 || kind > 255 || len < 0 || len > 255 || (len && !data)) return 0;
    if (rec_find(id, kind, &at)) rec_cut(at);
    if (!len) return 1;
    if (used + 4 + len > SHIM_BYTES) return 0;
    pool[used] = (uint8_t)id; pool[used + 1] = (uint8_t)(id >> 8);
    pool[used + 2] = (uint8_t)kind; pool[used + 3] = (uint8_t)len;
    memcpy(pool + used + 4, data, (size_t)len);
    used = (uint16_t)(used + 4 + len);
    return 1;
}

void look_pool_drop(int id)
{
    int p = 0;
    while (p + 4 <= used) {
        if ((pool[p] | (pool[p + 1] << 8)) == id) rec_cut(p);
        else p += 4 + pool[p + 3];
    }
}

void look_resolve_mask(const DoomoResident *r, int id, uint32_t lv, DoomoDeep *out)
{
    const uint8_t *run;
    int len, i;
    (void)r;
    *out = doomo_deep_zero;
    run = look_pool_find(id, RUN_DEEP, &len);
    if (!run) return;
    for (i = 0; i + 1 < len; i += 2) {
        int f = run[i];
        if (f >= DF_COUNT) continue;
        if (lv == LV_CHIBI && !(doomo_df_flags[f] & DFF_CHIBI)) continue;
        out->v[f] = (uint8_t)(run[i + 1] & 0x7F);
    }
}

void look_resolve(const DoomoResident *r, int id, DoomoDeep *out)
{
    look_resolve_mask(r, id, LV_ALL, out);
}

uint32_t look_run_hash(int id, int chibi)
{
    const uint8_t *run;
    uint32_t h = 2166136261u;
    int len, i, any = 0;
    run = look_pool_find(id, RUN_DEEP, &len);
    if (!run) return 0;
    for (i = 0; i + 1 < len; i += 2) {
        if (chibi && (run[i] >= DF_COUNT || !(doomo_df_flags[run[i]] & DFF_CHIBI))) continue;
        h = (h ^ run[i]) * 16777619u;
        h = (h ^ run[i + 1]) * 16777619u;
        any = 1;
    }
    return any ? h : 0;
}

uint32_t deep_anim_mask(const DoomoDeep *d)
{
    int f;
    for (f = 0; f < DF_COUNT; f++)
        if (d->v[f] && (doomo_df_flags[f] & DFF_ANIM)) return LAM_FX;
    return 0;
}
