/* look_pool.c - the deep-look run pool and resolve (CR5; look_fields.h, look_pool.h).
 *
 * Replaces the weak stubs of look_pool_find / put / drop (stubs.c). The pool holds the runs of the
 * citizens whose look has deep fields (a record: id lo, id hi, kind, len, data): LOOK_POOL_CAP bytes
 * (6,144 B; the PS1 keeps 384 B, its resident RAM is full). The creator writes it when it commits a
 * citizen; it is saved as a section of the creator's save chunk (creator.c: look_pool_raw /
 * look_pool_load) and cleared by a new town, a load and town_free. With no run stored the renderer
 * is bit-for-bit what it was. */
#include "doomo/doomo.h"
#include "doomo/look_pool.h"
#include "doomo/look_fields.h"
#include "doomo/portrait.h"

/* record: id lo, id hi, kind, len, data[len] */
static uint8_t  pool[LOOK_POOL_CAP];
static uint16_t used;

const DoomoDeep doomo_deep_zero = { { 0 } };
const uint8_t doomo_df_flags[DF_COUNT] = {
    [DF_BLUSH2] = DFF_CHIBI, [DF_GLASSES2] = DFF_CHIBI, [DF_EAR_FOLD] = DFF_CHIBI,
    [DF_HOOD_HEAD] = DFF_CHIBI,
    [DF_AURA2] = DFF_CHIBI | DFF_ANIM,
    /* CR3: the gene-backed silhouettes and colours that show on a chibi (look_gene.c) */
    [DF_FUR_PATTERN] = DFF_CHIBI, [DF_FUR_COLOR3] = DFF_CHIBI, [DF_OUTER] = DFF_CHIBI,
    [DF_LEGWEAR] = DFF_CHIBI,
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
    int at, had = 0;
    if (id < 0 || id > 1023 || kind < 0 || kind > 255 || len < 0 || len > 255 || (len && !data)) return 0;
    if (rec_find(id, kind, &at)) had = 4 + pool[at + 3];
    if (len && used - had + 4 + len > LOOK_POOL_CAP) return 0;        /* full: the old run stays */
    if (had) rec_cut(at);
    if (!len) return 1;
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

/* The saved form is the records as they are (creator.c writes and reads it in its chunk). */
const uint8_t *look_pool_raw(uint32_t *len)
{
    *len = used;
    return pool;
}

/* Replace the pool by saved records. 1 ok; 0 if the bytes are malformed (a record that runs past the
 * end, no room): the pool is then empty. */
int look_pool_load(const uint8_t *src, uint32_t len)
{
    uint32_t p;
    used = 0;
    if (len > LOOK_POOL_CAP) return 0;
    for (p = 0; p < len; p += 4u + src[p + 3])
        if (len - p < 4 || 4u + src[p + 3] > len - p) return 0;
    if (len) memcpy(pool, src, len);
    used = (uint16_t)len;
    return 1;
}

void look_resolve_mask(const DoomoResident *r, int id, uint32_t lv, DoomoDeep *out)
{
    const uint8_t *run;
    int len, i;
    *out = doomo_deep_zero;
    look_gene_deep(r, lv, out);                         /* CR3: the gene's own new fields (r may be NULL) */
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

/* the run's own part of a cache key: deep_hash of what the run resolves to (the PS1's resident bytes).
 * (G2-CO) No run for the id resolves to the zero set, whose deep_hash is 0: every chibi a frame asks for
 * (the concert, 27-30 a frame) skips the resolve and the hash then. */
uint32_t look_run_hash(int id, int chibi)
{
    DoomoDeep d;
    int at;
    if (id < 0 || id > 1023 || !rec_find(id, RUN_DEEP, &at)) return 0;
    look_resolve_mask(NULL, id, chibi ? LV_CHIBI : LV_ALL, &d);
    return deep_hash(&d, chibi);
}

uint32_t deep_anim_mask(const DoomoDeep *d)
{
    int f;
    for (f = 0; f < DF_COUNT; f++)
        if (d->v[f] && (doomo_df_flags[f] & DFF_ANIM)) return LAM_FX;
    return 0;
}
