/* mus_graph.c - the walk, GEN bags, chapters, pools, lifts and the
 * transition matrix (owner: MU1; docs/design/music/SPEC.md sec.6.3, 6.5,
 * 8.3, 8.4, D10, CF-20).
 *
 * MCF_EXACT cues (the classic imports) walk their single edges in order,
 * exactly as at M1: node i -> edges[nodes[i].edge0].to, and a once play
 * ends after the cue's `tag` node.
 *
 * Every other cue takes the weighted walk of sec.6.3. Candidates are the
 * node's edges whose condition holds and whose target's [i_min, i_max]
 * holds the committed intensity; then
 *   1 no A -> A,  2 soft recency (the last 2 picks w/4, the 3rd w/2),
 *   3 the bag (a node not among the last min(8, 2n) picks: w x 4),
 *   4 anchors (after 2 non-anchors only anchors; never 3 anchors in a row),
 *   5 cadence legality: the seam chord_out -> chord_in must be legal in the
 *     cadence bitmap of the dial band's family (turns.dms),
 *   rules 2-5 relax when they leave nothing (mus_seams counts the illegal
 *   seams taken; the 1-bar turnarounds of turns.dms need the comp runtime,
 *   and rule 6's arcs are DG's, both later).
 * The pick is mus_h(seed, S_WALK, node, picks) % sum(w). The walk reads
 * only the deck (hist, passes, picks, dial, lifts), the committed
 * intensity and the params, so a deck copy replays it exactly (CF-20).
 * Chapters are picks / 5 (an edge's condition reads the chapter its target
 * plays in); a MEC_LIFT edge is live from chapter 2 when
 * mus_h(seed, S_LIFT, chapter) % 100 < 35, lifts the key by its arg (at
 * most twice), and the key comes home at the next chapter.
 *
 * The anti-repeat guard is musgen's (D10): nothing here looks at notes. */
#include "mus_int.h"

static uint32_t mix(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

uint32_t mus_h(uint32_t seed, uint32_t stream, uint32_t a, uint32_t b)
{
    return mix(seed ^ mix(stream * 0x9E3779B9u ^ a * 0x85EBCA77u ^ b * 0xC2B2AE3Du));
}

/* element k of the hashed Fisher-Yates permutation of 0..n-1 (n <= 8) */
int mus_bag(uint32_t h, int n, int k)
{
    uint8_t p[8];
    int i;
    if (n <= 1) return 0;
    if (n > 8) n = 8;
    for (i = 0; i < n; i++) p[i] = (uint8_t)i;
    for (i = n - 1; i > 0; i--) {
        int j = (int)(mix(h + (uint32_t)i) % (uint32_t)(i + 1));
        uint8_t t = p[i];
        p[i] = p[j];
        p[j] = t;
    }
    return p[k % n];
}

int mus_pool_value(uint32_t seed, int e, int chapter, int n)
{
    return n > 1 ? (int)(mus_h(seed, S_POOL, (uint32_t)e, (uint32_t)(chapter + e) / 5u) % (uint32_t)n) : 0;
}

uint8_t mus_intensity_now = 128;

static int cond_ok(const MusGen *g, const MusEdge *e, int node)
{
    int a = e->arg, ch = (g->d.picks + 1) / 5;              /* the chapter the target plays in */
    switch (e->cond) {
    case MEC_ALWAYS:     return 1;
    case MEC_I_GE:       return mus_intensity_now >= a;
    case MEC_I_LT:       return mus_intensity_now < a;
    case MEC_DIAL_GE:    return g->d.dial >= a;
    case MEC_DIAL_LT:    return g->d.dial < a;
    case MEC_PASS_GE:    return g->d.passes[node & 31] >= a;
    case MEC_PARAM_GE:   return music_param(a & 31) >= (a >> 5) * 32;
    case MEC_PARAM_LT:   return music_param(a & 31) < (a >> 5) * 32;
    case MEC_CHAPTER_GE: return ch >= a;
    case MEC_LIFT:       return ch >= 2 && g->d.arc_i < 2 && g->d.vlevel == 0 &&
                                mus_h(g->d.seed, S_LIFT, (uint32_t)ch, 0) % 100u < 35u;
    default:             return 0;
    }
}

/* rule 5: is the seam chord_out(from) -> chord_in(to) legal in the family
 * of the deck's dial band (MU1T's cadence bitmaps, turns.dms)? N.C., "same"
 * and a bank without the table are legal. */
static int chord_class(int q)
{
    switch (q) {
    case MCH_MAJ: case MCH_MAJ7: case MCH_ADD9: case MCH_SIX: case MCH_SUS2: return MCC_MAJ;
    case MCH_MIN: case MCH_MIN7: case MCH_MIN9: case MCH_MMAJ7: return MCC_MIN;
    case MCH_DOM7: case MCH_AUG: return MCC_DOM;
    default: return MCC_OTHER;
    }
}

int mus_seam_ok(const MusGen *g, int from, int to)
{
    const uint8_t *t = mus_bank_misc(MUS_MISC_TURNS);
    int o = g->cv.nodes[from].chord_out, i = g->cv.nodes[to].chord_in, fam, idx;
    if (!t || (o & 15) >= 12 || (i & 15) >= 12) return 1;
    fam = g->d.dial < 128 ? MCAD_FAM_MAJOR : g->d.dial < 192 ? MCAD_FAM_MINOR : MCAD_FAM_METAL;
    if (fam >= t[1]) return 1;
    idx = (chord_class(o >> 4) * 4 + chord_class(i >> 4)) * 12 + ((i & 15) - (o & 15) + 12) % 12;
    return (t[4 + fam * MUS_CAD_BYTES + (idx >> 3)] >> (idx & 7)) & 1;
}

uint16_t mus_seams;                                  /* illegal seams the walk had to take */

#define WALK_MAX 16

static int walk(MusGen *g, int node)
{
    const MusCue *c = g->cv.cue;
    const MusNode *n = &g->cv.nodes[node];
    const MusEdge *ed = g->cv.edges + n->edge0;
    uint16_t w[WALK_MAX], wr[WALK_MAX];
    int k, ne = DOOMO_MIN(n->n_edge, WALK_MAX), any_anchor = 0, recent, relaxed = 0;
    uint32_t tot = 0, totr = 0, h;
    if (n->edge0 + ne > c->n_edges) ne = c->n_edges > n->edge0 ? c->n_edges - n->edge0 : 0;
    recent = DOOMO_MIN(8, 2 * c->n_nodes);
    for (k = 0; k < ne; k++) {
        int t = ed[k].to;
        const MusNode *tn;
        w[k] = wr[k] = 0;
        if (t >= c->n_nodes || !cond_ok(g, &ed[k], node)) continue;
        tn = &g->cv.nodes[t];
        if (mus_intensity_now < tn->i_min || mus_intensity_now > tn->i_max) continue;
        wr[k] = ed[k].w;
        if (t == node) continue;                               /* 1: no A -> A */
        if (!mus_seam_ok(g, node, t)) continue;                /* 5: cadence legality */
        if (tn->flags & MNF_ANCHOR) any_anchor = 1;
        w[k] = ed[k].w;
    }
    for (k = 0; k < ne; k++) {
        int t = ed[k].to, i, heard = 0;
        uint32_t x = (uint32_t)w[k] << 2;                       /* w/4 of a weight 1 is not 0 */
        if (!x) continue;
        if (g->d.hist[0] == t + 1 || g->d.hist[1] == t + 1) x >>= 2;   /* 2: soft recency */
        else if (g->d.hist[2] == t + 1) x >>= 1;
        for (i = 0; i < recent; i++) if (g->d.hist[i] == t + 1) heard = 1;
        if (!heard) x <<= 2;                                    /* 3: the bag */
        if (g->cv.nodes[t].flags & MNF_ANCHOR) {                /* 4: anchors */
            if (g->d.anchors >= 2) x = 0;
        } else if (g->d.nonanchor >= 2 && any_anchor) x = 0;
        w[k] = (uint16_t)x;
        tot += x;
    }
    for (k = 0; k < ne; k++) totr += wr[k];
    if (!tot) {                                                 /* relax rules 2-5 */
        for (k = 0; k < ne; k++) w[k] = wr[k];
        tot = totr;
        relaxed = 1;
    }
    if (!tot) return -1;
    h = mus_h(g->d.seed, S_WALK, (uint32_t)node, g->d.picks) % tot;
    for (k = 0; k < ne; k++) {
        if (h < w[k]) break;
        h -= w[k];
    }
    if (k >= ne) k = ne - 1;
    if (relaxed && !mus_seam_ok(g, node, ed[k].to) && mus_seams < 0xFFFF) mus_seams++;
    if (ed[k].cond == MEC_LIFT) {                               /* a key lift */
        g->d.vlevel = (uint8_t)DOOMO_CLAMP(ed[k].arg, 1, 2);
        g->d.arc_i++;
    }
    return ed[k].to;
}

int mus_graph_next(MusGen *g, int node, int once, int *end)
{
    const MusCue *c = g->cv.cue;
    const MusNode *n;
    int nx, i;
    *end = 0;
    if (node < 0 || node >= c->n_nodes) { *end = 1; return 0; }
    if (once && node == c->tag) { *end = 1; return node; }
    n = &g->cv.nodes[node];
    if (c->flags & MCF_EXACT) {
        if (n->n_edge == 0 || n->edge0 >= c->n_edges) {
            if (!(c->flags & MCF_LOOP)) { *end = 1; return node; }
            return c->reentry < c->n_nodes ? c->reentry : 0;
        }
        return g->cv.edges[n->edge0].to < c->n_nodes ? g->cv.edges[n->edge0].to : 0;
    }
    /* the chapter changes with this pick: a lifted key comes home */
    if (g->d.vlevel && (g->d.picks + 1) / 5 != g->d.picks / 5) g->d.vlevel = 0;
    nx = walk(g, node);
    if (nx < 0) {
        if (!(c->flags & MCF_LOOP)) { *end = 1; return node; }
        nx = c->reentry < c->n_nodes ? c->reentry : 0;
    }
    for (i = 7; i > 0; i--) g->d.hist[i] = g->d.hist[i - 1];
    g->d.hist[0] = (uint8_t)(nx + 1);
    if (g->cv.nodes[nx].flags & MNF_ANCHOR) { g->d.anchors++; g->d.nonanchor = 0; }
    else { g->d.nonanchor++; g->d.anchors = 0; }
    return nx;
}

/* ---- GEN: the variant of a node's pass (sec.6.3, 8.3) --------------------------------- */
/* variant = bag(mus_h(seed, S_GEN, node, pass / n), pass % n), with the
 * epoch-boundary repeat swapped (the last of one epoch never opens the next;
 * the swap moves only positions 0 and 1, so for n >= 3 an epoch's last is
 * its bag's last). Two variants simply alternate (a swap would move the
 * last, and nothing else avoids the repeat). */
int mus_gen_variant(uint32_t seed, int node, int pass, int n)
{
    int e, k, v, prev_last;
    if (n <= 1) return 0;
    if (n == 2) return mus_bag(mus_h(seed, S_GEN, (uint32_t)node, 0), 2, pass & 1);
    e = pass / n;
    k = pass % n;
    if (k > 1 || e == 0) return mus_bag(mus_h(seed, S_GEN, (uint32_t)node, (uint32_t)e), n, k);
    prev_last = mus_bag(mus_h(seed, S_GEN, (uint32_t)node, (uint32_t)(e - 1)), n, n - 1);
    v = mus_bag(mus_h(seed, S_GEN, (uint32_t)node, (uint32_t)e), n, 0);
    if (v != prev_last) return k == 0 ? v : mus_bag(mus_h(seed, S_GEN, (uint32_t)node, (uint32_t)e), n, 1);
    return mus_bag(mus_h(seed, S_GEN, (uint32_t)node, (uint32_t)e), n, k ^ 1);
}

/* ---- the transition matrix (sec.6.5): (from, to) > (from, *) > (*, to) > (*, *) ---- */
const MusTrans *mus_trans_find(int from, int to)
{
    const MusTrans *t = (const MusTrans *)(const void *)mus_bank_misc(MUS_MISC_TRANS), *best = NULL;
    int i, n = t ? mus_bank()->n_trans : 0, bs = -1;
    for (i = 0; i < n; i++) {
        int s;
        if ((t[i].from != from && t[i].from != MUS_ANY) || (t[i].to != to && t[i].to != MUS_ANY)) continue;
        s = (t[i].from == from) * 2 + (t[i].to == to);
        if (s > bs) { bs = s; best = &t[i]; }
    }
    return best;
}
