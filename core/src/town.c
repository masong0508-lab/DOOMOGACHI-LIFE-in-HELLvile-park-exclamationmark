/* town.c - the data API of the saved world (owner: foundation).
 *
 * Relationship edges are kept SORTED by (a, b) with a < b, so rel_find()
 * is a binary search. When the graph is full, the weakest bond (smallest
 * |value|, special kinds protected) is evicted to make room.
 */
#include "doomo/town.h"
#include "doomo/roster.h"
#include "doomo/rng.h"
#include "doomo/portrait.h"
#include "doomo/ui.h"
#include "doomo/save_ext.h"

DoomoTown doomo_town;

const char *const doomo_weekday_names[7] = {
    "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
    "Sunday"
};

const char *const doomo_rel_kind_names[REL_KIND_COUNT] = {
    "Acquaintance", "Friend", "Best Friend", "Dating", "Married", "Rival",
    "Nemesis", "Family", "Ex"
};

/* ====================================================================== */
/* residents                                                                */
DoomoResident *town_res(int id)
{
    if (id == DOOMO_PLAYER_ID) return &doomo_town.player;
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS) return NULL;
    return &doomo_town.res[id];
}

int town_alive(int id)
{
    DoomoResident *r = town_res(id);
    return r && r->status != RES_EMPTY && r->status != RES_DEAD;
}

int town_alloc(int district)
{
    int i, base = district ? DOOMO_DISTRICT_SIZE : 0;
    for (i = 0; i < DOOMO_DISTRICT_SIZE; i++)
        if (doomo_town.res[base + i].status == RES_EMPTY) return base + i;
    return -1;
}

void town_free(int id)
{
    int i, n;
    if (id < 0 || id >= DOOMO_MAX_RESIDENTS) return;
    memset(&doomo_town.res[id], 0, sizeof(DoomoResident));
    rel_remove_all(id);
    /* forget its graves too, so a new citizen in the slot is not mourned */
    for (i = 0, n = 0; i < doomo_town.grave_count; i++)
        if (doomo_town.graves[i].resident != id) doomo_town.graves[n++] = doomo_town.graves[i];
    doomo_town.grave_count = (uint16_t)n;
    portrait_invalidate(id);
}

int town_count(int status)
{
    int i, n = 0;
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        int s = doomo_town.res[i].status;
        if (status < 0 ? s != RES_EMPTY : s == status) n++;
    }
    return n;
}

int town_count_district(int district, int status)
{
    int i, n = 0, base = district ? DOOMO_DISTRICT_SIZE : 0;
    for (i = base; i < base + DOOMO_DISTRICT_SIZE; i++) {
        int s = doomo_town.res[i].status;
        if (status < 0 ? s != RES_EMPTY : s == status) n++;
    }
    return n;
}

static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

int town_find_by_name(const char *name)
{
    int i, j, pass;
    if (!name || !*name) return -1;
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
            const DoomoResident *r = &doomo_town.res[i];
            if (r->status == RES_EMPTY) continue;
            for (j = 0; j < DOOMO_NAME_LEN; j++) {
                int a = (unsigned char)r->name[j], b = (unsigned char)name[j];
                if (pass) { a = lower(a); b = lower(b); }
                if (a != b) break;
                if (a == 0) return i;
            }
        }
    }
    return -1;
}

void town_premade_record(int id, DoomoResident *out)
{
    const DoomoResident *p = (id >= 0 && id < DOOMO_MAX_RESIDENTS) ? roster_premade(id) : NULL;
    if (p) { *out = *p; return; }
    if (id < DOOMO_DISTRICT_SIZE) resident_generate(out, 0xD00D0000u + (uint32_t)id);
    else resident_generate_hellborn(out, 0xD00D0000u + (uint32_t)id);
}

void town_random_record(uint32_t seed, int id, DoomoResident *out)
{
    uint32_t s = doomo_hash32(seed * 2654435761u + (uint32_t)id * 40503u + 0x51ED270Bu);
    if (id < DOOMO_DISTRICT_SIZE) resident_generate(out, s);
    else resident_generate_hellborn(out, s);
}

/* ====================================================================== */
/* relationship graph                                                       */
static uint32_t ekey(const DoomoEdge *e) { return ((uint32_t)e->a << 16) | e->b; }

static int valid_id(int id) { return id >= 0 && id <= DOOMO_PLAYER_ID; }

/* index of the first edge with key >= k */
static int lower_bound(uint32_t k)
{
    int lo = 0, hi = doomo_town.edge_count;
    while (lo < hi) {
        int mid = (lo + hi) >> 1;
        if (ekey(&doomo_town.edges[mid]) < k) lo = mid + 1; else hi = mid;
    }
    return lo;
}

DoomoEdge *rel_find(int a, int b)
{
    int i;
    uint32_t k;
    if (a == b || !valid_id(a) || !valid_id(b)) return NULL;
    if (a > b) { int t = a; a = b; b = t; }
    k = ((uint32_t)a << 16) | (uint32_t)b;
    i = lower_bound(k);
    if (i < doomo_town.edge_count && ekey(&doomo_town.edges[i]) == k)
        return &doomo_town.edges[i];
    return NULL;
}

static int edge_strength(const DoomoEdge *e)
{
    int v = e->value < 0 ? -e->value : e->value;
    switch (e->kind & REL_KIND_MASK) {
    case REL_MARRIED: case REL_FAMILY: v += 400; break;
    case REL_DATING: v += 300; break;
    case REL_BEST_FRIEND: case REL_NEMESIS: v += 150; break;
    case REL_EX: v += 50; break;
    default: break;
    }
    if (e->kind & (REL_CRUSH_AB | REL_CRUSH_BA)) v += 60;
    if (e->a == DOOMO_PLAYER_ID || e->b == DOOMO_PLAYER_ID) v += 200;
    return v;
}

static void remove_edge(int i)
{
    int n = doomo_town.edge_count;
    if (i < 0 || i >= n) return;
    memmove(&doomo_town.edges[i], &doomo_town.edges[i + 1],
            (size_t)(n - i - 1) * sizeof(DoomoEdge));
    doomo_town.edge_count = (uint16_t)(n - 1);
}

DoomoEdge *rel_get_or_add(int a, int b)
{
    int i, n;
    uint32_t k;
    DoomoEdge *e = rel_find(a, b);
    if (e) return e;
    if (a == b || !valid_id(a) || !valid_id(b)) return NULL;
    if (a > b) { int t = a; a = b; b = t; }
    if (doomo_town.edge_count >= DOOMO_MAX_EDGES) {
        int best = 0, bs = 0x7FFF;
        for (i = 0; i < doomo_town.edge_count; i++) {
            int s = edge_strength(&doomo_town.edges[i]);
            if (s < bs) { bs = s; best = i; }
        }
        remove_edge(best);
    }
    k = ((uint32_t)a << 16) | (uint32_t)b;
    i = lower_bound(k);
    n = doomo_town.edge_count;
    memmove(&doomo_town.edges[i + 1], &doomo_town.edges[i], (size_t)(n - i) * sizeof(DoomoEdge));
    e = &doomo_town.edges[i];
    e->a = (uint16_t)a;
    e->b = (uint16_t)b;
    e->value = 0;
    e->kind = REL_ACQUAINTANCE;
    doomo_town.edge_count = (uint16_t)(n + 1);
    return e;
}

int rel_value(int a, int b)
{
    DoomoEdge *e = rel_find(a, b);
    return e ? e->value : 0;
}

int rel_kind(int a, int b)
{
    DoomoEdge *e = rel_find(a, b);
    return e ? (e->kind & REL_KIND_MASK) : -1;
}

static int is_auto_kind(int k)
{
    return k == REL_ACQUAINTANCE || k == REL_FRIEND || k == REL_BEST_FRIEND ||
           k == REL_RIVAL || k == REL_NEMESIS;
}

/* Values drive the casual kinds; special bonds (dating, married, family,
 * ex) only change through rel_set_kind (the life sim). Becoming friends
 * counts PROG_FRIENDSHIPS (and PROG_CROSS_BORDER_FRIENDS across the wall),
 * so the sim must not count those again. */
void rel_adjust(int a, int b, int delta)
{
    DoomoEdge *e = rel_get_or_add(a, b);
    int v, k, nk;
    if (!e) return;
    v = e->value + delta;
    v = DOOMO_CLAMP(v, -100, 100);
    e->value = (int8_t)v;
    k = e->kind & REL_KIND_MASK;
    if (!is_auto_kind(k)) return;
    if (v >= 70) nk = REL_BEST_FRIEND;
    else if (v >= 25) nk = REL_FRIEND;
    else if (v <= -70) nk = REL_NEMESIS;
    else if (v <= -25) nk = REL_RIVAL;
    else nk = REL_ACQUAINTANCE;
    /* a little hysteresis: friends stay friends until they drop below 15 */
    if (k == REL_FRIEND && nk == REL_ACQUAINTANCE && v >= 15) nk = REL_FRIEND;
    if (k == REL_RIVAL && nk == REL_ACQUAINTANCE && v <= -15) nk = REL_RIVAL;
    if (nk != k) {
        int ea = e->a, eb = e->b;
        e->kind = (uint8_t)((e->kind & ~REL_KIND_MASK) | nk);
        if ((nk == REL_FRIEND || nk == REL_BEST_FRIEND) &&
            k != REL_FRIEND && k != REL_BEST_FRIEND) {
            progress_add(PROG_FRIENDSHIPS, 1);
            if (ea < DOOMO_MAX_RESIDENTS && eb < DOOMO_MAX_RESIDENTS &&
                RES_DISTRICT(ea) != RES_DISTRICT(eb))
                progress_add(PROG_CROSS_BORDER_FRIENDS, 1);
        }
    }
}

void rel_set_kind(int a, int b, int kind)
{
    DoomoEdge *e;
    if (kind < 0 || kind >= REL_KIND_COUNT) return;
    e = rel_get_or_add(a, b);
    if (e) e->kind = (uint8_t)((e->kind & ~REL_KIND_MASK) | kind);
}

int rel_list(int a, uint16_t *out_ids, int max)
{
    static int8_t vals[256];
    int i, n = 0;
    if (max > 256) max = 256;
    if (max <= 0 || !out_ids) return 0;
    for (i = 0; i < doomo_town.edge_count; i++) {
        const DoomoEdge *e = &doomo_town.edges[i];
        int other, j;
        if (e->a == a) other = e->b;
        else if (e->b == a) other = e->a;
        else continue;
        /* insertion into the top-`max` list, sorted by value desc */
        j = n < max ? n : max - 1;
        if (n == max && e->value <= vals[max - 1]) continue;
        while (j > 0 && vals[j - 1] < e->value) {
            vals[j] = vals[j - 1];
            out_ids[j] = out_ids[j - 1];
            j--;
        }
        vals[j] = e->value;
        out_ids[j] = (uint16_t)other;
        if (n < max) n++;
    }
    return n;
}

void rel_remove_all(int a)
{
    int i, n = 0;
    for (i = 0; i < doomo_town.edge_count; i++) {
        const DoomoEdge *e = &doomo_town.edges[i];
        if (e->a == a || e->b == a) continue;
        doomo_town.edges[n++] = *e;
    }
    doomo_town.edge_count = (uint16_t)n;
}

int rel_has_crush(int from, int to)
{
    DoomoEdge *e = rel_find(from, to);
    if (!e) return 0;
    return (e->kind & (from < to ? REL_CRUSH_AB : REL_CRUSH_BA)) != 0;
}

void rel_set_crush(int from, int to, int on)
{
    DoomoEdge *e;
    uint8_t bit;
    if (on) e = rel_get_or_add(from, to);
    else e = rel_find(from, to);
    if (!e) return;
    bit = (uint8_t)(from < to ? REL_CRUSH_AB : REL_CRUSH_BA);
    if (on) e->kind |= bit; else e->kind &= (uint8_t)~bit;
}

void rel_normalize(void)
{
    static const int gaps[] = { 701, 301, 132, 57, 23, 10, 4, 1 };
    int i, n = 0, g;
    DoomoEdge *E = doomo_town.edges;
    int count = doomo_town.edge_count;
    if (count > DOOMO_MAX_EDGES) count = DOOMO_MAX_EDGES;
    /* fix up and drop junk */
    for (i = 0; i < count; i++) {
        DoomoEdge e = E[i];
        int k = e.kind & REL_KIND_MASK;
        if (e.a == e.b || !valid_id(e.a) || !valid_id(e.b)) continue;
        if (e.a > e.b) {
            uint16_t t = e.a; uint8_t c = e.kind;
            e.a = e.b; e.b = t;
            e.kind = (uint8_t)((c & ~(REL_CRUSH_AB | REL_CRUSH_BA)) |
                               ((c & REL_CRUSH_AB) ? REL_CRUSH_BA : 0) |
                               ((c & REL_CRUSH_BA) ? REL_CRUSH_AB : 0));
        }
        if (k >= REL_KIND_COUNT) k = REL_ACQUAINTANCE;
        e.kind = (uint8_t)((e.kind & (REL_CRUSH_AB | REL_CRUSH_BA)) | k);
        if (e.value < -100) e.value = -100;
        E[n++] = e;
    }
    /* shell sort by key */
    for (g = 0; g < DOOMO_COUNTOF(gaps); g++) {
        int gap = gaps[g];
        for (i = gap; i < n; i++) {
            DoomoEdge t = E[i];
            uint32_t k = ekey(&t);
            int j = i;
            while (j >= gap && ekey(&E[j - gap]) > k) {
                E[j] = E[j - gap];
                j -= gap;
            }
            E[j] = t;
        }
    }
    /* dedupe (keep the first) */
    if (n > 1) {
        int w = 1;
        for (i = 1; i < n; i++)
            if (ekey(&E[i]) != ekey(&E[w - 1])) E[w++] = E[i];
        n = w;
    }
    doomo_town.edge_count = (uint16_t)n;
}

/* ====================================================================== */
/* inventory, graves, clock                                                 */
int inv_count(int item)
{
    if (item <= ITEM_NONE || item >= ITEM_COUNT) return 0;
    return doomo_town.items[item];
}

int inv_add(int item, int n)
{
    int c;
    if (item <= ITEM_NONE || item >= ITEM_COUNT) return 0;
    c = doomo_town.items[item] + n;
    c = DOOMO_CLAMP(c, 0, 99);
    doomo_town.items[item] = (uint8_t)c;
    return c;
}

int inv_take(int item, int n)
{
    if (item <= ITEM_NONE || item >= ITEM_COUNT || n < 0) return 0;
    if (doomo_town.items[item] < n) return 0;
    doomo_town.items[item] = (uint8_t)(doomo_town.items[item] - n);
    return 1;
}

int inv_total(void)
{
    int i, t = 0;
    for (i = 1; i < ITEM_COUNT; i++) t += doomo_town.items[i];
    return t;
}

void grave_add(int resident_id)
{
    int i, n = doomo_town.grave_count;
    if (resident_id < 0 || resident_id >= DOOMO_MAX_RESIDENTS) return;
    for (i = 0; i < n; i++)
        if (doomo_town.graves[i].resident == resident_id) return;
    if (n >= DOOMO_MAX_GRAVES) {
        memmove(&doomo_town.graves[0], &doomo_town.graves[1],
                (DOOMO_MAX_GRAVES - 1) * sizeof(DoomoGrave));
        n = DOOMO_MAX_GRAVES - 1;
    }
    doomo_town.graves[n].resident = (uint16_t)resident_id;
    doomo_town.graves[n].day = (uint16_t)(doomo_town.day > 0xFFFF ? 0xFFFF : doomo_town.day);
    doomo_town.grave_count = (uint16_t)(n + 1);
}

int town_hour(void) { return (doomo_town.minute / 60) % 24; }

int town_is_night(void)
{
    int h = town_hour();
    return h >= 21 || h < 6;
}

void town_time_str(char *buf, int cap)
{
    int h = town_hour(), m = doomo_town.minute % 60;
    if (opt_get(OPT_CLOCK_24H)) doomo_snprintf(buf, cap, "%02d:%02d", h, m);
    else doomo_snprintf(buf, cap, "%d:%02d %s", h % 12 ? h % 12 : 12, m, h < 12 ? "AM" : "PM");
}

/* ====================================================================== */
/* new islands                                                              */
static void random_edges(uint32_t seed)
{
    DoomoRng r;
    int i, j, n = 0;
    rng_seed(&r, seed ^ 0xED6E5EEDu);
    for (i = 0; i < DOOMO_MAX_RESIDENTS && n < DOOMO_MAX_EDGES - 4; i++) {
        int links = rng_range(&r, 3);
        for (j = 0; j < links && n < DOOMO_MAX_EDGES - 4; j++) {
            DoomoEdge *e = &doomo_town.edges[n];
            int other, roll = rng_range(&r, 100), kind, v;
            if (rng_chance(&r, 75)) {
                int base = i < DOOMO_DISTRICT_SIZE ? 0 : DOOMO_DISTRICT_SIZE;
                other = base + rng_range(&r, DOOMO_DISTRICT_SIZE);
            } else {
                other = rng_range(&r, DOOMO_MAX_RESIDENTS);
            }
            if (other == i) continue;
            if (roll < 40)      { kind = REL_FRIEND; v = rng_between(&r, 25, 65); }
            else if (roll < 60) { kind = REL_ACQUAINTANCE; v = rng_between(&r, -10, 20); }
            else if (roll < 68) { kind = REL_BEST_FRIEND; v = rng_between(&r, 70, 100); }
            else if (roll < 78) { kind = REL_RIVAL; v = rng_between(&r, -60, -25); }
            else if (roll < 86) { kind = REL_FAMILY; v = rng_between(&r, 20, 90); }
            else if (roll < 92) { kind = REL_DATING; v = rng_between(&r, 50, 95); }
            else if (roll < 95) { kind = REL_MARRIED; v = rng_between(&r, 60, 100); }
            else if (roll < 97) { kind = REL_NEMESIS; v = rng_between(&r, -100, -70); }
            else                { kind = REL_EX; v = rng_between(&r, -40, 20); }
            e->a = (uint16_t)i;
            e->b = (uint16_t)other;
            e->value = (int8_t)v;
            e->kind = (uint8_t)kind;
            if (rng_chance(&r, 6)) e->kind |= REL_CRUSH_AB;
            n++;
        }
    }
    doomo_town.edge_count = (uint16_t)n;
    rel_normalize();
}

void town_new(int mode, uint32_t seed, int population)
{
    uint8_t opt[DOOMO_OPT_BYTES];
    uint8_t unl[DOOMO_UNLOCK_BITS / 8];
    int i;
    DoomoResident *p;

    memcpy(opt, doomo_town.opt, sizeof(opt));
    memcpy(unl, doomo_town.unlocks, sizeof(unl));
    memset(&doomo_town, 0, sizeof(doomo_town));
    memcpy(doomo_town.opt, opt, sizeof(opt));
    memcpy(doomo_town.unlocks, unl, sizeof(unl));

    doomo_town.mode = (uint8_t)(mode == TOWN_STORY ? TOWN_STORY : TOWN_FREEPLAY);
    doomo_town.seed = seed;
    doomo_town.population = (uint8_t)DOOMO_CLAMP(population, TOWN_POP_EMPTY, TOWN_POP_RANDOM);
    doomo_town.minute = 8 * 60;
    doomo_town.money = DOOMO_START_MONEY;
    doomo_town.items[ITEM_CUPCAKE] = 3;
    doomo_town.items[ITEM_ONIGIRI] = 2;
    doomo_town.items[ITEM_MILK] = 2;
    doomo_town.items[ITEM_SOAP] = 2;
    doomo_town.items[ITEM_FLOWERS] = 1;
    doomo_town.items[ITEM_HOLY_WATER] = 1;
    doomo_town.story.hp = 100;

    p = &doomo_town.player;
    resident_default(p);
    memcpy(p->name, "You", 4);
    p->flags = RF_PLAYER | RF_CUSTOM;

    if (doomo_town.population == TOWN_POP_PREMADE) {
        int n = 0;
        for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
            town_premade_record(i, &doomo_town.res[i]);
            resident_sanitize(&doomo_town.res[i]);
        }
        for (i = 0; i < doomo_roster_edge_count && n < DOOMO_MAX_EDGES; i++)
            doomo_town.edges[n++] = doomo_roster_edges[i];
        doomo_town.edge_count = (uint16_t)n;
        rel_normalize();
    } else if (doomo_town.population == TOWN_POP_RANDOM) {
        for (i = 0; i < DOOMO_MAX_RESIDENTS; i++)
            town_random_record(seed, i, &doomo_town.res[i]);
        random_edges(seed);
    }
    save_ext_reset_all();       /* save_ext.h: module data back to defaults */
}
