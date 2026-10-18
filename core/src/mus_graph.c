/* mus_graph.c - the node walk (owner: MU1; docs/design/music/SPEC.md
 * sec.6.3, 18).
 *
 * M1: MCF_EXACT cues (the classic imports) walk their single edges in
 * order: node i -> edges[nodes[i].edge0].to, which mmlc points at the next
 * node and, from the last one, back to the cue's reentry node. A once play
 * (audio_music_once, MCF_ONCE) ends after the cue's `tag` node - for a
 * classic cue the last node of its first pass. The weighted walk, GEN bags,
 * chapters, the matrix and stingers arrive at M4 behind this function. */
#include "mus_int.h"

int mus_graph_next(const MusCueView *cv, int node, int once, int *end)
{
    const MusCue *c = cv->cue;
    const MusNode *n;
    *end = 0;
    if (node < 0 || node >= c->n_nodes) { *end = 1; return 0; }
    if (once && node == c->tag) { *end = 1; return node; }
    n = &cv->nodes[node];
    if (n->n_edge == 0 || n->edge0 >= c->n_edges) {
        if (!(c->flags & MCF_LOOP)) { *end = 1; return node; }
        return c->reentry < c->n_nodes ? c->reentry : 0;
    }
    return cv->edges[n->edge0].to < c->n_nodes ? cv->edges[n->edge0].to : 0;
}
