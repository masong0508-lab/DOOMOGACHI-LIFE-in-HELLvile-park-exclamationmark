/* mus_lane.c - the arranger: lane tokens -> ring events (owner: MU1;
 * docs/design/music/SPEC.md sec.5 "Tokens", 6.2-6.4, 8.1 TWIN, 8.3, D3-D5,
 * CF-20, CF-34).
 *
 * A deck (MusGen) plays one cue: per role a MusVoiceRt reads that role's
 * lane in the current node's pack. mus_gen_until() merges the roles in
 * time order (the role whose next token starts first; the lower runtime on
 * ties), so every ring stays nondecreasing, and writes events up to a
 * master position. Commands take effect at the next timed token's
 * position, which is exactly when the classic sequencer applied them.
 *
 * The decoder is stateless across nodes apart from the role runtime: each
 * node starts from the role defaults (inst = MusRole.inst, or inst_hi once
 * the dial passes the role's morph rank; vol 48, gate 0, deflen 24, base 0)
 * and its lane's leading commands, so any node can be entered cold
 * (mus_gen_enter: seeks, the scrub, CONTINUE).
 *
 * Bar lines. Before the first token at or after a bar line the deck calls
 * mus_core_bar() (deck A: stems with a 4-bar dwell, the dial, the voice
 * plan, the CF-20 bar log). A role whose stem goes off, or which loses its
 * voice, is released on the bar line. Silent roles are decoded and then
 * filtered, never skipped (CF-34).
 *
 * GEN (sec.8.3). A pack with variants takes, per pass, one variant of the
 * family pool_value(node) chose (mus_gen_variant: a no-repeat bag); its
 * lanes replace the base lanes of the roles in its role mask.
 * TWIN (sec.8.1). From dial S3 a COUNTER role with the TWIN morph bit plays
 * the LEAD lane a diatonic third lower (same rhythm, refit to the scale).
 *
 * The scrub (MGF_SCRUB, deck B's storage during DECK vinyl) decodes a node
 * without emitting: each runtime keeps the note sounding at its cursor
 * (nx_pitch, accent = vel, bar_off..gate_u = its span in node units).
 *
 * Gates follow the classic law: a note's first piece gives
 * max(1, frames(len) * g / 8) and every TIE piece adds frames(len) * g / 8,
 * g = the lane's GATE or the instrument's gate, frames() the classic
 * note_ticks at the master tempo. */
#include "mus_int.h"

static const uint16_t len_tab[16] = MUS_LEN_TAB_INIT;

#define rd16 mus_rd16

static uint32_t mpos(const MusGen *g, uint32_t u)
{
    u += g->d.acc;
    if (g->scale_q8 != 256) u = g->base + ((u & MUS_POS_MASK) * g->scale_q8 >> 8);
    return u & MUS_POS_MASK;
}

static int pitched(int kind) { return kind != RK_KIT && kind != RK_PERC; }

static uint16_t bar_units(const MusGen *g) { return (uint16_t)(g->bar_u_meter * MUS_UPQ); }

/* the role's default instrument at the deck's dial (sec.6.4: one rank per bar) */
static uint8_t role_inst(const MusGen *g, const MusRole *ro)
{
    return (ro->inst_hi != ro->inst && g->d.dial >= 128 + 32 * (ro->morph & 3)) ? ro->inst_hi : ro->inst;
}

/* the variant block of the node's pack, NULL = none */
static const uint8_t *var_block(const MusGen *g, uint16_t *mask)
{
    const uint8_t *pk = g->pack, *dir;
    if (g->var == 0xFF || !(pk[1] & MNP_VARIANTS)) return NULL;
    dir = pk + rd16(pk + 2 + 2 * pk[0]);
    *mask = (uint16_t)rd16(dir + 2);
    return pk + rd16(dir + 4 + 2 * g->var);
}

void mus_gen_enter(MusGen *g, int node, uint32_t pos, int pass, int var)
{
    const MusNode *n = &g->cv.nodes[node];
    const uint8_t *pack = g->cv.blob + n->pack_off, *vb;
    const MusRole *roles = g->cv.roles;
    uint16_t vmask = 0;
    int r, lead = -1, kv = 0;
    g->d.node = (uint8_t)node;
    g->d.acc = pos;
    g->pack = pack;
    g->bar_u_meter = (uint8_t)(g->cv.cue->meter ? g->cv.cue->meter : 4);
    g->node_len = (uint16_t)(n->bars * bar_units(g));
    g->bar_next = 0;
    g->d.bar = 0;
    if (pass < 0) {
        if (g->d.passes[node & 31] < 255) g->d.passes[node & 31]++;
        g->d.pass = g->d.passes[node & 31];
    } else g->d.pass = (uint8_t)pass;
    g->var = 0xFF;                        /* var: 0xFF choose, < 8 forced, 0xFE the base */
    if (var != 0xFF) { if (var < 8 && (pack[1] & MNP_VARIANTS)) g->var = (uint8_t)var; }
    else if ((pack[1] & MNP_VARIANTS) && !(g->cv.cue->flags & MCF_EXACT)) {
        const uint8_t *dir = pack + rd16(pack + 2 + 2 * pack[0]);
        int nv = DOOMO_CLAMP(dir[0], 1, 8), nf = DOOMO_MAX(dir[1], 1), fam, i, m = 0, k;
        fam = mus_pool_value(g->d.seed, node, g->d.picks / 5, nf);
        for (i = 0; i < nv; i++) if ((pack[rd16(dir + 4 + 2 * i)] >> 4) % nf == fam) m++;
        k = mus_gen_variant(g->d.seed, node, g->d.pass ? g->d.pass - 1 : 0, m ? m : nv);
        for (i = 0; i < nv; i++)
            if (!m || (pack[rd16(dir + 4 + 2 * i)] >> 4) % nf == fam) { if (k-- == 0) break; }
        g->var = (uint8_t)(i < nv ? i : 0);
    }
    vb = var_block(g, &vmask);
    if (g->deck == 0 && !(g->mode & MGF_SCRUB)) g->ktrans = (int8_t)g->d.vlevel;
    for (r = 0; r < g->cv.cue->n_roles; r++) if (roles[r].kind == RK_LEAD) { lead = r; break; }
    for (r = 0; r < g->n_roles; r++) {
        MusVoiceRt *v = &g->v[r];
        const MusRole *ro = &roles[v->role];
        int ri = v->role;
        unsigned off;
        v->flags &= (uint8_t)~MVF_TWIN;
        if (ro->kind == RK_COUNTER && (ro->morph & 0x80) && g->d.dial >= 192 && lead >= 0) {
            ri = lead;                                            /* TWIN */
            v->flags |= MVF_TWIN;
        }
        off = ri < pack[0] ? rd16(pack + 2 + 2 * ri) : 0;
        if (vb && (vmask & (1u << ri))) {
            int j, idx = 0;
            for (j = 0; j < ri; j++) if (vmask & (1u << j)) idx++;
            kv = (vb[0] & 1) ? (n->bars * 2) : 0;                /* CHART */
            off = rd16(vb + 1 + kv + 2 * idx);
        }
        v->lane = (uint16_t)off;
        v->next_u = (uint16_t)(off ? 0 : g->node_len);
        v->inst = role_inst(g, ro);
        v->vol = 48;
        v->gate_q = 0;
        v->deflen = 24;
        v->base = 0;
        v->trans = 0;
        v->nx_pitch = 0;
        v->flags = (uint8_t)((v->flags & (MVF_NOTE | MVF_TWIN)) | MVF_DIRTY | (off ? 0 : MVF_DONE));
    }
}

void mus_plan(const MusCueView *cv, uint16_t stems, int deck, uint8_t *vmap)
{
    int r, n = cv->cue->n_roles;
    uint8_t best[MUS_A_VOICES];
    (void)deck;
    if ((cv->cue->flags & MCF_EXACT) && n <= MUS_A_VOICES) {     /* classic: role i = voice i */
        for (r = 0; r < MUS_A_ROLES; r++) vmap[r] = (uint8_t)(r < n ? r : 0xFF);
        return;
    }
    memset(best, 0xFF, sizeof(best));
    for (r = 0; r < n && r < MUS_A_ROLES; r++) {
        int k = cv->roles[r].kind, c;
        c = k == RK_LEAD ? 0 : k == RK_BASS ? 2 : (k == RK_KIT || k == RK_PERC) ? 3 :
            (k == RK_BED || k == RK_FX) ? -1 : 1;
        if (c < 0 || !(stems & (1u << r))) continue;
        if (best[c] == 0xFF || (cv->roles[r].prio & 3) > (cv->roles[best[c]].prio & 3)) best[c] = (uint8_t)r;
    }
    for (r = 0; r < MUS_A_ROLES; r++) vmap[r] = 0xFF;
    for (r = 0; r < MUS_A_VOICES; r++) if (best[r] != 0xFF) vmap[best[r]] = (uint8_t)r;
}

void mus_gen_start(MusGen *g, const MusCueView *cv, uint32_t base, int32_t rate256,
                   int mode, const uint8_t *vmap)
{
    int r;
    memset(&g->d, 0, sizeof(g->d));
    g->cv = *cv;
    g->d.cue = cv->id;
    g->d.state = MGS_PLAY;
    g->d.bpm_q4 = (uint16_t)(cv->cue->bpm * 16);
    g->d.speed_q8 = 256;
    g->d.stems = 0xFFFF;
    g->d.stems_next = 0xFFFF;
    g->d.dial = g->d.dial_target = cv->cue->dial_home;
    g->base = base & MUS_POS_MASK;
    g->scale_q8 = 256;
    g->ktrans = 0;
    g->rate256 = rate256 > 0 ? rate256 : 1;
    g->mode = (uint8_t)mode;
    g->n_roles = (uint8_t)DOOMO_MIN(cv->cue->n_roles, g->max_roles);
    if (vmap) for (r = 0; r < MUS_A_ROLES; r++) g->vmap[r] = (uint8_t)(r < MUS_A_VOICES ? vmap[r] : 0xFF);
    else mus_plan(cv, 0xFFFF, g->deck, g->vmap);
    for (r = 0; r < g->n_roles; r++) {
        memset(&g->v[r], 0, sizeof(g->v[r]));
        g->v[r].role = (uint8_t)r;
    }
    mus_gen_enter(g, cv->cue->entry < cv->cue->n_nodes ? cv->cue->entry : 0, base & MUS_POS_MASK, -1, 0xFF);
    g->next = g->d.acc;
}

void mus_gen_stop(MusGen *g) { g->d.state = MGS_IDLE; }

static void ev(MusSink *s, int kind, uint32_t pos, uint32_t arg)
{
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
    uint32_t pos = mpos(g, v->next_u);
    int voice = g->vmap[r], guard = 512, kind = g->cv.roles[v->role].kind;
    int on = (g->d.stems >> v->role) & 1, scrub = g->mode & MGF_SCRUB;
    if (scrub) voice = 0xFF;
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
            case MTC_PAN:
                if (voice != 0xFF) ev(s, MEV_PAN, pos, MEV_PAN_ARG(voice, (int8_t)a));
                break;
            case MTC_KEYSH: g->ktrans = (int8_t)(g->ktrans + (int8_t)a); break;
            case MTC_CALL:
                if (!(v->flags & MVF_RET)) { v->ret = i; i = (uint16_t)(a * 4); v->flags |= MVF_RET; }
                break;
            case MTC_RET:
                if (v->flags & MVF_RET) { i = v->ret; v->flags &= (uint8_t)~MVF_RET; }
                break;
            case MTC_MARK:
                if (voice != 0xFF) ev(s, MEV_MARK, pos, MEV_MARK_ARG(a, 0, 0, 0));
                break;
            case MTC_VINYL: case MTC_BED:
                if (!scrub && g->deck == 0) mus_core_lane_cmd(g, b - MT_CMD, a, pos);
                break;
            default: break;               /* GLIDE TMB ECHO PROB ALT HEAD QSLOT COMP DNA FILL VMASK: mus_var */
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
            ev(s, MEV_INST, pos, MEV_INST_ARG(voice, v->inst, in->tparam, v->vol));
        }
        if (voice != 0xFF || scrub) v->flags &= (uint8_t)~MVF_DIRTY;
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
            key = v->base + n + v->trans + (pitched(kind) ? g->ktrans : 0);
            if (v->flags & MVF_TWIN) key = mus_twin_pitch(g->cv.cue->key, key);
            key = DOOMO_CLAMP(key, 0, 127);
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
            if (scrub && on) {                    /* the scrub keeps the sounding note */
                uint32_t e = v->next_u + (uint32_t)gate * (uint32_t)g->rate256 / 256u;
                v->nx_pitch = (uint8_t)key;
                v->accent = v->vol;
                v->bar_off = v->next_u;
                v->gate_u = (uint16_t)DOOMO_MIN(e, 0xFFFFu);
            }
            if (voice != 0xFF && on)
                ev(s, MEV_NOTE, pos, MEV_NOTE_ARG(voice, key, v->vol, DOOMO_MIN(gate, MEV_DUR_MAX), 0));
            v->flags |= MVF_NOTE;
        } else if (b >= MT_HIT && b < MT_TONE) {
            if (voice != 0xFF && on)
                ev(s, MEV_DRUM, pos, MEV_DRUM_ARG(voice, b & 7, KIT_CLASSIC, v->vol, 0, 0));
            v->flags &= (uint8_t)~MVF_NOTE;
        } else if ((b >= MT_REST && b < MT_TIE) || b == MT_REST_D) {
            if ((v->flags & MVF_NOTE) && voice != 0xFF)
                ev(s, MEV_OFF, pos, MEV_OFF_ARG(voice, MOFF_RELEASE));
            if (scrub && v->gate_u > v->next_u) v->gate_u = v->next_u;
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

/* a bar line of the node (bar_next): the core's decisions, then release
 * every sounding role that went silent or lost its voice */
static void bar_line(MusGen *g, MusSink *s)
{
    uint8_t old[MUS_A_ROLES];
    uint16_t was = g->d.stems;
    uint32_t pos = mpos(g, g->bar_next);
    int r;
    memcpy(old, g->vmap, sizeof(old));
    if (!(g->mode & MGF_SCRUB)) mus_core_bar(g, pos, g->bar_next == 0);
    g->bar_next = (uint16_t)(g->bar_next + bar_units(g));
    if (g->bar_next > g->node_len) g->bar_next = g->node_len;
    g->d.bar++;
    for (r = 0; r < g->n_roles; r++) {
        MusVoiceRt *v = &g->v[r];
        int rr = v->role;
        if (!(v->flags & MVF_NOTE) || old[r] == 0xFF) continue;
        if (((g->d.stems >> rr) & 1) && old[r] == g->vmap[r]) continue;
        if (!((was >> rr) & 1)) continue;
        ev(s, MEV_OFF, pos, MEV_OFF_ARG(old[r], MOFF_RELEASE));
        v->flags &= (uint8_t)~MVF_NOTE;
    }
}

uint32_t mus_gen_until(MusGen *g, uint32_t until, MusSink *s)
{
    int loads = 0;
    uint32_t lu;
    until &= MUS_POS_MASK;
    lu = until;
    if (g->scale_q8 != 256)
        lu = mus_pdiff(until, g->base) <= 0 ? 0 : (uint32_t)(((until - g->base) & MUS_POS_MASK) * 256u / g->scale_q8);
    while (g->d.state == MGS_PLAY) {
        int r, best = -1;
        unsigned bu = 0xFFFFu;
        uint32_t pos;
        for (r = 0; r < g->n_roles; r++) {
            const MusVoiceRt *v = &g->v[r];
            if (!(v->flags & MVF_DONE) && v->next_u < g->node_len && v->next_u < bu) { bu = v->next_u; best = r; }
        }
        if (g->bar_next < g->node_len && (best < 0 || bu >= g->bar_next) && !(g->mode & MGF_SCRUB)) {
            if (mus_pdiff((g->d.acc + g->bar_next) & MUS_POS_MASK, lu) >= 0 || s->room < g->n_roles + 4)
                return mpos(g, g->bar_next);
            bar_line(g, s);
            continue;
        }
        if (best < 0) {                                           /* the node is over */
            int end, nx;
            uint32_t epos = (g->d.acc + g->node_len) & MUS_POS_MASK;
            if ((g->mode & MGF_SCRUB) || mus_pdiff(epos, lu) >= 0) return mpos(g, g->node_len);
            nx = mus_graph_next(g, g->d.node, g->mode & MGF_ONCE, &end);
            if (end) {
                if (s->room < g->n_roles + 2) return mpos(g, g->node_len);
                for (r = 0; r < g->n_roles; r++)
                    if (g->vmap[r] != 0xFF) ev(s, MEV_OFF, mpos(g, g->node_len), MEV_OFF_ARG(g->vmap[r], MOFF_CUT));
                ev(s, MEV_MARK, mpos(g, g->node_len), MEV_MARK_ARG(MMK_END, 0, 0, 0));
                g->d.state = MGS_ENDED;
                return mpos(g, g->node_len);
            }
            if (++loads > 64) { g->d.state = MGS_ENDED; return mpos(g, g->node_len); }   /* empty nodes */
            g->d.picks++;
            g->d.ghost_u += g->node_len;
            mus_gen_enter(g, nx, epos, -1, 0xFF);
            continue;
        }
        pos = (g->d.acc + bu) & MUS_POS_MASK;
        if (mus_pdiff(pos, lu) >= 0 || s->room < 4) return mpos(g, bu);
        lane_step(g, best, s);
    }
    return until;
}

/* ---- scales: the twin and the babble snap ---------------------------------------------- */
static const uint8_t scale_tab[6][7] = {     /* MMODE_* (mus_data.h) */
    { 0, 2, 4, 5, 7, 9, 11 },             /* major          */
    { 0, 2, 3, 5, 7, 8, 10 },             /* natural minor  */
    { 0, 2, 3, 5, 7, 8, 11 },             /* harmonic minor */
    { 0, 2, 3, 5, 7, 9, 10 },             /* dorian         */
    { 0, 1, 3, 5, 7, 8, 10 },             /* phrygian       */
    { 0, 2, 4, 5, 7, 9, 10 },             /* mixolydian     */
};

const uint8_t *mus_scale(int mode) { return scale_tab[(unsigned)mode < 6u ? mode : 0]; }
static const uint8_t *scale_of(int key) { return mus_scale((key >> 4) & 15); }

int mus_twin_pitch(int key, int pitch)
{
    const uint8_t *sc = scale_of(key);
    int rel = ((pitch - (key & 15)) % 12 + 12) % 12, base = pitch - rel, d = 6;
    while (d > 0 && sc[d] > rel) d--;
    d -= 2;
    if (d < 0) { d += 7; base -= 12; }
    return base + sc[d];
}

int mus_scale_snap(int key, int pitch16)
{
    const uint8_t *sc = scale_of(key);
    int semi = (pitch16 + 8) >> 4, rel = ((semi - (key & 15)) % 12 + 12) % 12, d, best = 0, bd = 99;
    for (d = 0; d < 8; d++) {
        int s = d < 7 ? sc[d] : 12, x = rel - s;
        if (x < 0) x = -x;
        if (x < bd) { bd = x; best = s - rel; }
    }
    return (semi + best) << 4;
}
