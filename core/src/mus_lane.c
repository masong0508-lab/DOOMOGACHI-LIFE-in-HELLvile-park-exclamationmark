/* mus_lane.c - the arranger: lane tokens -> ring events (owner: MU1;
 * docs/design/music/SPEC.md sec.5 "Tokens", 6.2, D3-D5).
 *
 * A deck (MusGen) plays one cue: per role a MusVoiceRt reads that role's
 * lane in the current node's pack. mus_gen_until() merges the roles in
 * time order (the role whose next token starts first; the lower role on
 * ties), so every ring stays nondecreasing, and writes events up to a
 * master position. Commands take effect at the next timed token's
 * position, which is exactly when the classic sequencer applied them.
 *
 * The decoder is stateless across nodes apart from the role runtime: each
 * node starts from the role defaults (inst = MusRole.inst, vol 48, gate 0,
 * deflen 24, base 0) and its lane's leading commands, so any node can be
 * entered cold (seeks, M4's walk).
 *
 * Gates follow the classic law: a note's first piece gives
 * max(1, frames(len) * g / 8) and every TIE piece adds frames(len) * g / 8,
 * g = the lane's GATE or the instrument's gate, frames() the classic
 * note_ticks at the master tempo. */
#include "mus_int.h"

static const uint16_t len_tab[16] = MUS_LEN_TAB_INIT;

static unsigned rd16(const uint8_t *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }

int mus_graph_next(const MusCueView *cv, int node, int once, int *end);

static void node_load(MusGen *g, int node)
{
    const MusNode *n = &g->cv.nodes[node];
    const uint8_t *pack = g->cv.blob + n->pack_off;
    int r;
    g->d.node = (uint8_t)node;
    g->pack = pack;
    g->node_len = (uint16_t)(n->bars * (g->cv.cue->meter ? g->cv.cue->meter : 4) * MUS_UPQ);
    if (g->d.passes[node & 31] < 255) g->d.passes[node & 31]++;
    g->d.pass = g->d.passes[node & 31];
    for (r = 0; r < g->n_roles; r++) {
        MusVoiceRt *v = &g->v[r];
        unsigned off = r < pack[0] ? rd16(pack + 2 + 2 * r) : 0;
        v->lane = (uint16_t)off;
        v->next_u = (uint16_t)(off ? 0 : g->node_len);
        v->inst = g->cv.roles[r].inst;
        v->vol = 48;
        v->gate_q = 0;
        v->deflen = 24;
        v->base = 0;
        v->trans = 0;
        v->flags = (uint8_t)((v->flags & MVF_NOTE) | MVF_DIRTY | (off ? 0 : MVF_DONE));
    }
}

void mus_gen_start(MusGen *g, const MusCueView *cv, uint32_t base, int32_t rate256,
                   int mode, const uint8_t *vmap)
{
    int r;
    memset(&g->d, 0, sizeof(g->d));
    g->cv = *cv;
    g->d.cue = cv->id;
    g->d.state = MGS_PLAY;
    g->d.acc = base & MUS_POS_MASK;
    g->d.bpm_q4 = (uint16_t)(cv->cue->bpm * 16);
    g->d.speed_q8 = 256;
    g->rate256 = rate256 > 0 ? rate256 : 1;
    g->mode = (uint8_t)mode;
    g->n_roles = (uint8_t)DOOMO_MIN(cv->cue->n_roles, g->max_roles);
    for (r = 0; r < MUS_A_VOICES; r++)
        g->vmap[r] = vmap ? vmap[r] : (uint8_t)(r < g->n_roles ? r : 0xFF);
    for (r = 0; r < g->n_roles; r++) {
        memset(&g->v[r], 0, sizeof(g->v[r]));
        g->v[r].role = (uint8_t)r;
    }
    node_load(g, cv->cue->entry < cv->cue->n_nodes ? cv->cue->entry : 0);
    g->next = g->d.acc;
}

void mus_gen_stop(MusGen *g) { g->d.state = MGS_IDLE; }

static void ev(MusGen *g, MusSink *s, int kind, uint32_t pos, uint32_t arg)
{
    (void)g;
    s->emit(s->ctx, MEV_MAKE_POS(kind, pos), arg);
    s->room--;
}

/* the length of the timed token at pk[*i - 1] = b, plus its EXT tokens */
static int tok_len(const MusVoiceRt *v, const uint8_t *pk, uint16_t *i, int b)
{
    int len;
    if (b < MT_NOTE_L || (b >= MT_HIT && b < MT_TONE) || b >= MT_HOLD) len = v->deflen ? v->deflen : 256;
    else len = len_tab[b & 15];
    for (;;) {
        int x = pk[*i];
        if (x >= MT_EXT && x < MT_EXTS) len += len_tab[x & 15];
        else if (x >= MT_EXTS && x < MT_HOLD) len += (x & 7) + 1;
        else break;
        (*i)++;
    }
    return len;
}

static void lane_step(MusGen *g, int r, MusSink *s)
{
    MusVoiceRt *v = &g->v[r];
    const uint8_t *pk = g->pack;
    uint16_t i = v->lane;
    uint32_t pos = (g->d.acc + v->next_u) & MUS_POS_MASK;
    int voice = g->vmap[r], guard = 512;
    while (guard-- > 0) {
        int b = pk[i++], len, a;
        if (b >= MT_CMD && b < MT_CMD + MTC_COUNT) {
            a = pk[i++];
            switch (b - MT_CMD) {
            case MTC_DEFLEN: v->deflen = (uint8_t)a; break;
            case MTC_VOL: v->vol = (uint8_t)(a & 63); v->flags |= MVF_DIRTY; break;
            case MTC_INST: v->inst = (uint8_t)a; v->flags |= MVF_DIRTY; break;
            case MTC_GATE: v->gate_q = (uint8_t)a; break;
            case MTC_TRANS: v->trans = (int8_t)a; break;
            case MTC_BASE: v->base = (uint8_t)a; break;
            case MTC_CALL:
                if (!(v->flags & MVF_RET)) { v->ret = i; i = (uint16_t)(a * 4); v->flags |= MVF_RET; }
                break;
            case MTC_RET:
                if (v->flags & MVF_RET) { i = v->ret; v->flags &= (uint8_t)~MVF_RET; }
                break;
            case MTC_MARK:
                if (voice != 0xFF) ev(g, s, MEV_MARK, pos, MEV_MARK_ARG(a, 0, 0, 0));
                break;
            default: break;               /* M4: PAN GLIDE TMB ECHO PROB ALT HEAD ... */
            }
            continue;
        }
        if (b == MT_END) {
            v->next_u = g->node_len;
            v->flags |= MVF_DONE;
            v->lane = i;
            return;
        }
        len = tok_len(v, pk, &i, b);
        if ((v->flags & MVF_DIRTY) && voice != 0xFF) {
            const MusInst *in = MUS_INST(v->inst);
            ev(g, s, MEV_INST, pos, MEV_INST_ARG(voice, v->inst, in->tparam, v->vol));
        }
        v->flags &= (uint8_t)~MVF_DIRTY;
        if (b < MT_REST) {                                        /* NOTE, NOTE_L */
            int n = b < MT_NOTE_L ? b : pk[i++], key, g8, gate;
            const MusInst *in = MUS_INST(v->inst);
            if (b >= MT_NOTE_L) {                 /* the EXT tokens follow the pitch byte */
                for (;;) {
                    int x = pk[i];
                    if (x >= MT_EXT && x < MT_EXTS) len += len_tab[x & 15];
                    else if (x >= MT_EXTS && x < MT_HOLD) len += (x & 7) + 1;
                    else break;
                    i++;
                }
            }
            key = DOOMO_CLAMP(v->base + n + v->trans, 0, 127);
            g8 = v->gate_q ? v->gate_q : in->gate;
            gate = DOOMO_MAX(1, mus_note_frames(len, g->rate256) * g8 / 8);
            for (;;) {                                            /* the tied pieces */
                int x = pk[i], piece;
                if (!((x >= MT_TIE && x < MT_HIT) || x == MT_TIE_D)) break;
                i++;
                piece = tok_len(v, pk, &i, x == MT_TIE_D ? MT_HOLD : x);
                gate = DOOMO_MIN(60000, gate + mus_note_frames(piece, g->rate256) * g8 / 8);
                len += piece;
            }
            if (voice != 0xFF)
                ev(g, s, MEV_NOTE, pos, MEV_NOTE_ARG(voice, key, v->vol, DOOMO_MIN(gate, MEV_DUR_MAX), 0));
            v->flags |= MVF_NOTE;
        } else if (b >= MT_HIT && b < MT_TONE) {
            if (voice != 0xFF)
                ev(g, s, MEV_DRUM, pos, MEV_DRUM_ARG(voice, b & 7, KIT_CLASSIC, v->vol, 0, 0));
            v->flags &= (uint8_t)~MVF_NOTE;
        } else if ((b >= MT_REST && b < MT_TIE) || b == MT_REST_D) {
            if ((v->flags & MVF_NOTE) && voice != 0xFF)
                ev(g, s, MEV_OFF, pos, MEV_OFF_ARG(voice, MOFF_RELEASE));
            v->flags &= (uint8_t)~MVF_NOTE;
        }
        /* TIE without a note before it, HOLD: time only */
        v->next_u = (uint16_t)(v->next_u + len);
        v->lane = i;
        return;
    }
    v->next_u = g->node_len;              /* a broken lane: skip the node */
    v->flags |= MVF_DONE;
}

static int32_t pdiff(uint32_t a, uint32_t b) { return (int32_t)((a - b) << 4) >> 4; }

uint32_t mus_gen_until(MusGen *g, uint32_t until, MusSink *s)
{
    int loads = 0;
    until &= MUS_POS_MASK;
    while (g->d.state == MGS_PLAY) {
        int r, best = -1;
        unsigned bu = 0xFFFFu;
        uint32_t pos;
        for (r = 0; r < g->n_roles; r++) {
            const MusVoiceRt *v = &g->v[r];
            if (!(v->flags & MVF_DONE) && v->next_u < g->node_len && v->next_u < bu) { bu = v->next_u; best = r; }
        }
        if (best < 0) {                                           /* the node is over */
            int end, nx = mus_graph_next(&g->cv, g->d.node, g->mode & MGF_ONCE, &end);
            uint32_t epos = (g->d.acc + g->node_len) & MUS_POS_MASK;
            if (pdiff(epos, until) >= 0) return epos;
            if (end) {
                if (s->room < g->n_roles + 2) return epos;
                for (r = 0; r < g->n_roles; r++)
                    if (g->vmap[r] != 0xFF) ev(g, s, MEV_OFF, epos, MEV_OFF_ARG(g->vmap[r], MOFF_CUT));
                ev(g, s, MEV_MARK, epos, MEV_MARK_ARG(MMK_END, 0, 0, 0));
                g->d.state = MGS_ENDED;
                return epos;
            }
            if (++loads > 64) { g->d.state = MGS_ENDED; return epos; }   /* empty nodes */
            g->d.acc = epos;
            g->d.picks++;
            node_load(g, nx);
            continue;
        }
        pos = (g->d.acc + bu) & MUS_POS_MASK;
        if (pdiff(pos, until) >= 0 || s->room < 4) return pos;
        lane_step(g, best, s);
    }
    return until;
}
