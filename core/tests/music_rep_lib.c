/* music_rep_lib.c - the non-repetition metric of T3 (owner: MU6;
 * docs/design/music/SPEC.md sec.8.5, CF-37, CF-38; music_rep.h has the
 * definitions). Ported from the design study's proto/host_main.c and
 * lr60.c, with CF-38's corrections: no velocity anywhere, drum dice and
 * ornaments out of the exact signatures, a fuzzy signature, recency and
 * texture. Host test code: malloc is fine; integer only all the same. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "music_rep.h"

static const int cls_w[RC_N] = { 3, 1, 1, 2, 1 };          /* LEAD COUNTER HARM BASS KIT */
static const int8_t chord_iv[16][4] = {     /* MCH_* (mus_data.h) chord tones above the root */
    { 0, 4, 7, -1 }, { 0, 3, 7, -1 }, { 0, 3, 6, -1 }, { 0, 4, 8, -1 }, { 0, 2, 7, -1 },
    { 0, 5, 7, -1 }, { 0, 4, 7, 10 }, { 0, 4, 7, 11 }, { 0, 3, 7, 10 }, { 0, 3, 6, 10 },
    { 0, 3, 6, 9 }, { 0, 3, 7, 11 }, { 0, 7, -1, -1 }, { 0, 4, 7, 2 }, { 0, 3, 7, 2 },
    { 0, 4, 7, 9 } };

void rep_init(RepRun *r, int beats_per_bar, int bpm, int know)
{
    memset(r, 0, sizeof(*r));
    r->bar_u = (beats_per_bar > 0 ? beats_per_bar : 4) * 48;
    r->bpm = bpm > 0 ? bpm : 120;
    r->know = know;
}

void rep_free(RepRun *r)
{
    free(r->b);
    r->b = NULL;
    r->n = r->cap = 0;
}

void rep_begin_bar(RepRun *r, const RepMeta *m)
{
    r->ncur = 0;
    if (m) r->cm = *m;
    else memset(&r->cm, 0, sizeof(r->cm));
}

void rep_note(RepRun *r, const RepNote *n)
{
    if (r->ncur < (int)(sizeof(r->cur) / sizeof(r->cur[0]))) r->cur[r->ncur++] = *n;
}

static uint32_t fnv(uint32_t h, uint32_t v)
{
    int i;
    for (i = 0; i < 4; i++) { h ^= (v >> (i * 8)) & 0xFFu; h *= 0x01000193u; }
    return h;
}

static int cmp_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : x > y;
}

static int len_class(int u)
{
    int lc = 0;
    while (u > 12 && lc < 7) { u >>= 1; lc++; }
    return lc;
}

static uint32_t sig_of(uint32_t *items, int n, uint32_t salt)
{
    uint32_t h = 0x811C9DC5u;
    int i;
    if (!n) return 0;
    qsort(items, (size_t)n, sizeof(uint32_t), cmp_u32);
    for (i = 0; i < n; i++) h = fnv(h, items[i]);
    h = fnv(h, salt);
    return h ? h : 1;
}

void rep_end_bar(RepRun *r)
{
    uint32_t s_it[512], p_it[512], l_it[512];
    int ns = 0, np = 0, nl = 0, i, k;
    RepBar *b;
    if (r->n == r->cap) {
        int cap = r->cap ? r->cap * 2 : 1024;
        RepBar *nb = (RepBar *)realloc(r->b, sizeof(RepBar) * (size_t)cap);
        if (!nb) return;
        r->b = nb;
        r->cap = cap;
    }
    b = &r->b[r->n++];
    memset(b, 0, sizeof(*b));
    b->m = r->cm;
    for (i = 0; i < r->ncur; i++) {
        const RepNote *n = &r->cur[i];
        int step = n->at / 12;          /* floor: a swung 16th stays on its step */
        if (n->cls >= RC_N || (n->flags & RNF_SURF)) continue;
        b->stems |= (uint16_t)(1u << (n->role & 15));
        if (n->flags & RNF_ORN) continue;                 /* ornament-blind */
        if (n->flags & RNF_DRUM) {
            if (!(n->flags & RNF_CORE)) continue;         /* the dice are not variation */
            s_it[ns++] = (uint32_t)(n->role & 15) << 28 | (uint32_t)(step & 255) << 16 |
                         (uint32_t)n->pitch << 8 | 0xFFu;
            b->ons[RC_KIT] |= (uint16_t)(1u << (step & 15));
            b->pcs[RC_KIT] |= (uint16_t)(1u << (n->pitch & 7));
        } else {
            uint32_t it = (uint32_t)(n->role & 15) << 28 | (uint32_t)(step & 255) << 16 |
                          (uint32_t)n->pitch << 8 | (uint32_t)len_class(n->len) << 4 |
                          (uint32_t)((n->flags & RNF_CHORD) ? (n->q & 15) : 0);
            s_it[ns++] = it;                              /* <= 256 notes per bar */
            p_it[np++] = it;
            if (n->cls == RC_LEAD) l_it[nl++] = it;
            b->ons[n->cls] |= (uint16_t)(1u << (step & 15));
            if (n->flags & RNF_CHORD) {
                for (k = 0; k < 4 && chord_iv[n->q & 15][k] >= 0; k++)
                    b->pcs[n->cls] |= (uint16_t)(1u << ((n->pitch + chord_iv[n->q & 15][k]) % 12));
            } else b->pcs[n->cls] |= (uint16_t)(1u << (n->pitch % 12));
            if (n->cls == RC_LEAD) { b->lead_sum = (uint16_t)(b->lead_sum + n->pitch); b->lead_n++; }
        }
        if (b->notes < 255) b->notes++;
    }
    b->S = sig_of(s_it, ns, (uint32_t)b->stems | (uint32_t)b->m.dial << 16);
    b->P = sig_of(p_it, np, 0x50u);
    b->L = (b->m.flags & RBF_ANCHOR4) ? 0 : sig_of(l_it, nl, 0x4Cu);
    r->ncur = 0;
}

static int popc(uint32_t x) { int n = 0; while (x) { x &= x - 1; n++; } return n; }
static int jac(uint32_t a, uint32_t b) { int u = popc(a | b); return u ? popc(a & b) * 1000 / u : 1000; }

/* The lead is in every comparison: where neither bar has one it scores 0
 * (a rest is not a repeat - the rule of L, and of the design study's own
 * signature). Any other role sounding in neither bar is left out. */
int rep_sim(const RepRun *r, int i, int j)
{
    const RepBar *a = &r->b[i], *b = &r->b[j];
    int c, num = 0, den = 0;
    for (c = 0; c < RC_N; c++) {
        if (!(a->ons[c] | b->ons[c])) {
            if (c == RC_LEAD) den += cls_w[c];
            continue;
        }
        num += cls_w[c] * (jac(a->ons[c], b->ons[c]) * jac(a->pcs[c], b->pcs[c]) / 1000);
        den += cls_w[c];
    }
    return den ? num / den : 0;
}

int rep_seconds(const RepRun *r, int bar)
{
    return (int)((long)bar * (r->bar_u / 48) * 60 / r->bpm);
}

/* ---- LRR: binary search on the run length, windows hashed and sorted ---------- */
typedef struct WinH { uint32_t h; int i; } WinH;
static int cmp_win(const void *a, const void *b)
{
    const WinH *x = (const WinH *)a, *y = (const WinH *)b;
    if (x->h != y->h) return x->h < y->h ? -1 : 1;
    return x->i - y->i;
}

static int has_repeat(const uint32_t *s, int n, int L, WinH *w, int *at, int *lag)
{
    int i, k, m = 0, zero = 0;
    /* zero counts the empty bars inside the window [i, i + L) */
    for (k = 0; k < L && k < n; k++) zero += !s[k];
    for (i = 0; i + L <= n; i++) {
        if (i) { zero -= !s[i - 1]; zero += !s[i + L - 1]; }
        if (zero) continue;
        {
            uint32_t h = 0x811C9DC5u;
            for (k = 0; k < L; k++) h = fnv(h, s[i + k]);
            w[m].h = h;
            w[m].i = i;
            m++;
        }
    }
    qsort(w, (size_t)m, sizeof(WinH), cmp_win);
    for (i = 0; i + 1 < m; i++) {
        int j;
        for (j = i + 1; j < m && w[j].h == w[i].h; j++)
            if (!memcmp(s + w[i].i, s + w[j].i, sizeof(uint32_t) * (size_t)L)) {
                *at = w[i].i;
                *lag = w[j].i - w[i].i;
                return 1;
            }
    }
    return 0;
}

int rep_lrr(const uint32_t *sig, int n, int *at, int *lag)
{
    int lo = 0, hi = n - 1, a = 0, g = 0;
    WinH *w;
    if (n < 2) return 0;
    w = (WinH *)malloc(sizeof(WinH) * (size_t)n);
    if (!w) return n;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2, ma, mg;
        if (has_repeat(sig, n, mid, w, &ma, &mg)) { lo = mid; a = ma; g = mg; }
        else hi = mid - 1;
    }
    free(w);
    if (at) *at = a;
    if (lag) *lag = g;
    return lo;
}

/* ---- the measures --------------------------------------------------------------- */
#define NEAR_LAGS 64
#define NEAR_MATCH 850

/* the texture of bars [b0, b0 + 8). The stem set is the roles sounding
 * throughout the 8 bars and the roles sounding in any of them, so a role
 * that drops out or comes in inside the block is a texture event. */
static uint32_t texture_of(const RepRun *r, int b0)
{
    int i, lead = 0, nl = 0, notes = 0, dial = 0, dens;
    uint16_t stems = 0, always = 0xFFFFu;
    uint32_t h = 0x811C9DC5u;
    for (i = b0; i < b0 + 8 && i < r->n; i++) {
        const RepBar *b = &r->b[i];
        stems |= b->stems;
        always &= b->stems;
        lead += b->lead_sum;
        nl += b->lead_n;
        notes += b->notes;
        if (b->m.dial > dial) dial = b->m.dial;
    }
    notes /= 8;
    dens = notes < 4 ? 0 : notes < 8 ? 1 : notes < 16 ? 2 : notes < 32 ? 3 : 4;
    h = fnv(h, (uint32_t)stems | (uint32_t)always << 16);
    h = fnv(h, (uint32_t)dial);
    h = fnv(h, r->b[b0].m.key);
    h = fnv(h, (uint32_t)r->b[b0].m.bpm / 16u);
    h = fnv(h, nl ? (uint32_t)(lead / nl / 6) : 0xFFu);
    h = fnv(h, (uint32_t)dens);
    return h;
}

static void add_span(RepStats *st, int len, int at, int lag)
{
    int i, k;
    for (i = 0; i < 5; i++)                  /* the same stretch heard twice: keep the longer */
        if (st->worst[i].len && at < st->worst[i].at + st->worst[i].len && st->worst[i].at < at + len) {
            if (len > st->worst[i].len) { st->worst[i].len = len; st->worst[i].at = at; st->worst[i].lag = lag; }
            goto sort;
        }
    for (i = 0; i < 5; i++)
        if (len > st->worst[i].len) {
            for (k = 4; k > i; k--) st->worst[k] = st->worst[k - 1];
            st->worst[i].len = len;
            st->worst[i].at = at;
            st->worst[i].lag = lag;
            break;
        }
sort:
    for (i = 1; i < 5; i++)
        for (k = i; k > 0 && st->worst[k].len > st->worst[k - 1].len; k--) {
            RepSpan t = st->worst[k];
            st->worst[k] = st->worst[k - 1];
            st->worst[k - 1] = t;
        }
}

void rep_measure(const RepRun *r, RepStats *st)
{
    int n = r->n, i, d, k;
    uint32_t *sig = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)(n + 1));
    uint16_t *sim = (uint16_t *)calloc((size_t)NEAR_LAGS * (size_t)(n + 1), sizeof(uint16_t));
    memset(st, 0, sizeof(*st));
    st->bars = n;
    if (!sig || !sim || n < 2) { free(sig); free(sim); return; }
    /* gate 1: literal repeats */
    for (i = 0; i < n; i++) sig[i] = r->b[i].S;
    st->lrr_s = rep_lrr(sig, n, &st->lrr_s_at, &st->lrr_s_lag);
    for (i = 0; i < n; i++) sig[i] = r->b[i].P;
    st->lrr_p = rep_lrr(sig, n, NULL, NULL);
    for (i = 0; i < n; i++) sig[i] = r->b[i].L;
    st->lrr_l = rep_lrr(sig, n, NULL, NULL);
    /* gate 2: near-loops, every lag < 64 */
    for (d = 1; d < NEAR_LAGS && d < n; d++) {
        int run = 0;
        for (i = d; i < n; i++) {
            int s = rep_sim(r, i, i - d);
            sim[(size_t)d * (size_t)n + (size_t)i] = (uint16_t)s;
            if (s >= NEAR_MATCH) {
                run++;
                if (run > st->near_run) { st->near_run = run; st->near_at = i - run + 1; st->near_lag = d; }
            } else {
                if (run >= 4) add_span(st, run, i - run, d);
                run = 0;
            }
        }
        if (run >= 4) add_span(st, run, n - run, d);
    }
    /* gate 3: recency of 8-bar windows */
    {
        int pass = 0, tot = 0;
        for (i = 0; i + 8 <= n; i++) {
            int near = 0;
            tot++;
            for (d = 1; d < 24 && d <= i && !near; d++) {
                int sum = 0;
                for (k = 0; k < 8; k++) sum += sim[(size_t)d * (size_t)n + (size_t)(i + k)];
                if (sum >= NEAR_MATCH * 8) near = 1;
            }
            pass += !near;
        }
        st->recency_pm = tot ? pass * 1000 / tot : 1000;
    }
    /* gate 4: texture */
    {
        int nb = n / 8, ch = 0, spans = 0, per10, w;
        uint32_t *tv = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)(nb + 1));
        if (tv) {
            for (i = 0; i < nb; i++) tv[i] = texture_of(r, i * 8);
            for (i = 0; i + 4 <= nb; i++) {
                spans++;
                for (k = 1; k < 4; k++) if (tv[i + k] != tv[i]) { ch++; break; }
            }
            st->texture_pm = spans ? ch * 1000 / spans : 1000;
            per10 = r->bpm * 10 / (r->bar_u / 48) / 8;      /* blocks in 10 minutes */
            if (per10 > nb) per10 = nb;
            st->texture_min10 = 1 << 30;
            for (w = 0; w + per10 <= nb && per10 > 0; w++) {
                int distinct = 0, j;
                for (k = w; k < w + per10; k++) {
                    for (j = w; j < k && tv[j] != tv[k]; j++) {}
                    distinct += j == k;
                }
                if (distinct < st->texture_min10) st->texture_min10 = distinct;
            }
            if (st->texture_min10 == 1 << 30) st->texture_min10 = 0;
            free(tv);
        }
    }
    /* gate 5: identity and form */
    {
        int music = 0, last = 0;
        for (i = 0; i < n; i++) {
            const RepBar *b = &r->b[i];
            if (b->notes) music++;
            if (b->m.flags & RBF_HEAD) {
                if (music - last > st->head_gap) st->head_gap = music - last;
                last = music;
            }
            if (b->m.flags & RBF_CAD) { st->cad_n++; st->cad_ok += !!(b->m.flags & RBF_CAD_OK); }
            if (b->m.flags & RBF_SEAM) st->seams++;
            if (b->lead_n) { st->lit_n++; st->lit_ok += !!(b->m.flags & RBF_LITERAL); }
        }
        if (music - last > st->head_gap) st->head_gap = music - last;
    }
    free(sig);
    free(sim);
}

#define WHY(...) do { if (why && !*why) snprintf(why, (size_t)why_len, __VA_ARGS__); } while (0)

int rep_hub_gates(const RepRun *r, const RepStats *st, int steady, char *why, int why_len)
{
    int f = 0;
    if (why && why_len) *why = 0;
    if (st->lrr_s > 4 || st->lrr_p > 8 || st->lrr_l > 16) {
        f |= 1 << 1;
        WHY("gate 1: LRR S %d (<= 4) P %d (<= 8) L %d (<= 16); S repeats bar %d (%ds) at lag %d",
            st->lrr_s, st->lrr_p, st->lrr_l, st->lrr_s_at, rep_seconds(r, st->lrr_s_at), st->lrr_s_lag);
    }
    if (st->near_run > 8) {
        f |= 1 << 2;
        WHY("gate 2: a %d-bar near-loop at lag %d from bar %d (%ds)", st->near_run, st->near_lag,
            st->near_at, rep_seconds(r, st->near_at));
    }
    if (st->recency_pm < 950) {
        f |= 1 << 3;
        WHY("gate 3: only %d.%d %% of 8-bar windows are >= 24 bars from their nearest echo",
            st->recency_pm / 10, st->recency_pm % 10);
    }
    if (st->texture_pm < 900 || (steady && st->texture_min10 < 4)) {
        f |= 1 << 4;
        WHY("gate 4: texture changes in %d.%d %% of 32-bar spans, %d vectors per 10 min",
            st->texture_pm / 10, st->texture_pm % 10, st->texture_min10);
    }
    if (((r->know & RK_HEAD) && st->head_gap > 32) ||
        ((r->know & RK_CAD) && st->cad_ok * 100 < st->cad_n * 95) ||
        ((r->know & RK_SEAM) && st->seams) ||
        ((r->know & RK_LITERAL) && st->lit_ok * 2 < st->lit_n)) {
        f |= 1 << 5;
        WHY("gate 5: HEAD_GAP %d, CAD_OK %d/%d, seams %d, literal %d/%d", st->head_gap, st->cad_ok,
            st->cad_n, st->seams, st->lit_ok, st->lit_n);
    }
    return f;
}

int rep_cue_gates(const RepStats *st, char *why, int why_len)
{
    if (why && why_len) *why = 0;
    if (st->lrr_s >= 8 || st->lrr_p >= 16 || st->lrr_p > 8) {
        WHY("10 min: LRR S %d (< 8), P %d (<= 8)", st->lrr_s, st->lrr_p);
        return 1 << 1;
    }
    return 0;
}
