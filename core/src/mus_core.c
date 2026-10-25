/* mus_core.c - the bank, music_update(), params and intensity, requests
 * and the transition matrix, stingers, the comic bridge, beat events,
 * windows, vinyl starts, quotes, beds, session memory and the hash
 * (owner: MU1; docs/design/music/SPEC.md sec.4, 6, 7.3, 9.1, 9.2, CF-18,
 * CF-19, CF-20, CF-25, CF-33).
 *
 * TIMING. Everything is stamped in master units on deck A's ghost timeline
 * (sec.5 "One master clock"). Requests resolve to a grid point at least a
 * tick ahead; ring A is never rewritten except by a CUT at that point
 * (D4); everything else - stinger notes, mute and duck windows, re-keys,
 * tempo bends, catch-up notes - goes on ring B, which the driver keeps
 * sorted. music_now() counts music_update() calls: an event "lands on tick
 * T" when it is dispatched in the update that brings music_now() to T, i.e.
 * when the driver's clock reaches mt 8T (T8).
 *
 * audio_music() keeps today's timing (CF-18): it starts its cue NOW
 * through mus_core_start(). music_set_cue() goes through the matrix:
 * (from, to) > (from, *) > (*, to) > (*, *), else a CUT at the grid point.
 *
 * DECKS. Deck A plays the cue (12 role runtimes, the bar log of CF-20).
 * Deck B is shared, one owner at a time: the dual song, a stinger, or the
 * DECK scrub decoder (mus_vinyl.c). On the 4-voice bridge a stinger plays
 * on the voices of its roles' classes while a deck-0 mute window silences
 * deck A there; deck A comes back with catch-up notes on the beat after
 * the stinger's gap.
 *
 * HASH MODE (T2, CF-33): a private deck at a fixed seed and intensity,
 * generation driven by position, FNV-1a over every event in merge order. It
 * never touches the playing music. */
#include "mus_int.h"
#include "audio_int.h"

/* ---- the bank ------------------------------------------------------------------------ */
static const uint8_t *bankp;
const MusBank *mus_bank(void) { return (const MusBank *)(const void *)(bankp ? bankp : doomo_mus_bank); }
void mus_bank_use(const uint8_t *bank) { bankp = bank; }
#define BANKB ((const uint8_t *)(const void *)mus_bank())

static const MusDirEnt *dir_find(int kind_sting, int id)
{
    const MusBank *b = mus_bank();
    const MusDirEnt *dir;
    int lo, hi, end;
    if (b->magic != MUS_BANK_MAGIC || id < 0 || id > 255) return NULL;
    dir = (const MusDirEnt *)(const void *)(BANKB + b->dir_off);
    lo = kind_sting ? b->n_cues : 0;
    end = hi = kind_sting ? b->n_cues + b->n_stings : b->n_cues;
    while (lo < hi) {                                     /* sorted by id */
        int mid = (lo + hi) / 2;
        if (dir[mid].id < id) lo = mid + 1;
        else hi = mid;
    }
    return (lo < end && dir[lo].id == id) ? &dir[lo] : NULL;
}

static int view(const MusDirEnt *e, int id, MusCueView *out)
{
    const uint8_t *blob;
    int at;
    if (!e || !out) return 0;
    blob = BANKB + e->off;
    out->blob = blob;
    out->cue = (const MusCue *)(const void *)blob;
    out->len = e->len;
    out->id = (uint8_t)id;
    out->kind = (uint8_t)MUS_DIR_KIND(e);
    at = (int)sizeof(MusCue);
    out->sting = NULL;
    if (out->kind == MDK_STING) { out->sting = (const MusStingHdr *)(const void *)(blob + at); at += (int)sizeof(MusStingHdr); }
    out->roles = (const MusRole *)(const void *)(blob + at);
    out->nodes = (const MusNode *)(const void *)(blob + at + out->cue->n_roles * (int)sizeof(MusRole));
    out->edges = (const MusEdge *)(const void *)((const uint8_t *)out->nodes + out->cue->n_nodes * (int)sizeof(MusNode));
    out->binds = (const MusBind *)(const void *)((const uint8_t *)out->edges + out->cue->n_edges * (int)sizeof(MusEdge));
    return out->cue->n_nodes > 0;
}

int mus_bank_cue(int id, MusCueView *out) { return view(dir_find(0, id), id, out); }
int mus_bank_sting(int id, MusCueView *out) { return view(dir_find(1, id), id, out); }

const uint8_t *mus_bank_misc(int i)
{
    const MusBank *b = mus_bank();
    uint32_t off;
    if (b->magic != MUS_BANK_MAGIC || i < 0 || i >= MUS_MISC_COUNT || !b->misc_off || b->misc_off + 32u > b->total)
        return NULL;
    off = (uint32_t)mus_rd16(BANKB + b->misc_off + 4 * i) | ((uint32_t)mus_rd16(BANKB + b->misc_off + 4 * i + 2) << 16);
    return off && off < b->total ? BANKB + off : NULL;
}

/* the built-in bed instruments (MUS_INST_BED0 + n): chirp, soft noise, tick, hum */
static const MusInst bed_inst[4] = {
    { TIMBRE_PULSE, TIMBRE_PULSE, 0, 40, 0, 40, 9, 8, 6, 40, 0, 0, 0, 4, 0, 0, 0, 0 },
    { TIMBRE_NOISE15, TIMBRE_NOISE15, 4, 4, 5, 10, 6, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { TIMBRE_NOISE7, TIMBRE_NOISE7, 0, 60, 0, 60, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { TIMBRE_TRI, TIMBRE_TRI, 3, 6, 8, 12, 9, 8, 3, 8, 6, 0, 0, 0, 0, 0, 0, 0 },
};

const MusInst *mus_bank_inst(int i)
{
    static const MusInst silent = { TIMBRE_PULSE, TIMBRE_PULSE, 0, 255, 0, 255, 0, 8, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0 };
    const MusBank *b = mus_bank();
    if (i >= MUS_INST_BED0 && i < MUS_INST_BED0 + 4) return &bed_inst[i - MUS_INST_BED0];
    if (b->magic != MUS_BANK_MAGIC || i < 0 || i >= b->n_inst) return &silent;
    return (const MusInst *)(const void *)(BANKB + b->inst_off) + i;
}

const MusInst *mus_inst_tab;
uint16_t mus_inst_n;

int32_t mus_rate256(int bpm)
{
    bpm = DOOMO_CLAMP(bpm, 30, 300);
    return (int32_t)((bpm * 12288 + 900) / 1800);      /* units per tick = bpm * 48 / 1800 */
}

int mus_note_frames(int units, int32_t rate256)
{
    int t = (int)(((int32_t)units * 256 + rate256 / 2) / rate256);
    return t < 1 ? 1 : t;
}

/* ---- state ------------------------------------------------------------------------------ */
typedef struct MusSession { uint8_t cue, node; uint16_t picks; uint8_t hist[8], passes[32]; } MusSession;   /* 44 */

static struct {
    MusGen a, b;
    MusVoiceRt va[MUS_A_ROLES], vb[MUS_B_VOICES];
    MusBarLog log[MUS_BAR_LOG];
    MusBeatEv bq[MUS_BEATQ];
    MusBeatFn cb[MUS_BEAT_CBS];
    MusSession ses[2];
    uint32_t start;                 /* deck A's grid origin (a bar line)             */
    uint32_t now;                   /* music_update() calls                          */
    uint32_t beat_next;             /* the next audible beat line                    */
    uint32_t req_cut, req_at;       /* a pending request: the CUT and the new start  */
    uint32_t cu_at;                 /* a pending catch-up                            */
    uint32_t gate_from, gate_to;    /* the scripted mute window                      */
    uint32_t stop_at;               /* a STOP hold from                              */
    uint32_t seek_at, seek_P;       /* CONTINUE: deck A re-seeks to P at seek_at     */
    uint32_t st_hit, st_back;       /* the stinger: its hit, deck A's return         */
    uint16_t bars, st_bar, vy_bar, q_bar, q_mbar, beds;
    uint8_t  ptgt[MP_COUNT], pcur[MP_COUNT];
    uint32_t pmove;                 /* params still moving                           */
    uint8_t  cb_mask[MUS_BEAT_CBS];
    uint8_t  bq_h, bq_n, log_i, b_mode;
    uint8_t  req, req_cue, req_entry, req_type;   /* req: 0 none, 1 cut pending, 2 start pending */
    uint8_t  cu_mask, stop_hold, seek, st_live, st_prio, st_mask;
    uint8_t  q_motif, q_prio, q_last, profile;
    uint8_t  inited, ended, bridge, crackle, lat, none, rest_on, sfx_in, ctl_dirty, pad[3];
} C;

#define DECK_A_ON() (C.a.d.state == MGS_PLAY)

static uint16_t bar_units(void) { return (uint16_t)((DECK_A_ON() && C.a.cv.cue->meter ? C.a.cv.cue->meter : 4) * MUS_UPQ); }
static uint32_t clk(void) { return mus_drv_clock(); }
static int before(uint32_t a, uint32_t b) { return mus_pdiff(a, b) < 0; }

MusGen *mus_core_deck(int d) { return d ? &C.b : &C.a; }
int  mus_core_b_mode(void) { return C.b_mode; }
void mus_core_b_claim(int mode)
{
    if (C.b_mode != mode) mus_gen_stop(&C.b);
    C.b_mode = (uint8_t)mode;
}

uint32_t mus_core_lookahead(void)
{
    return C.bridge ? (uint32_t)(C.a.rate256 >> 7) + 2u : MUS_LOOKAHEAD_U;
}

/* the master position `ticks` 30 Hz ticks from now at the current tempo */
uint32_t mus_core_pos_in(int ticks)
{
    uint64_t f = (uint64_t)mus_drv_clock_frac() + (uint64_t)mus_drv_tempo() * 8u * (uint32_t)(ticks > 0 ? ticks : 0);
    return (clk() + (uint32_t)(f >> 16)) & MUS_POS_MASK;
}

int mus_core_ticks_until(uint32_t pos)
{
    int64_t need = (int64_t)mus_pdiff(pos, clk()) * 65536 - mus_drv_clock_frac();
    uint32_t per = mus_drv_tempo() * 8u;
    if (need <= 0 || !per) return 0;
    return (int)((need + per - 1) / per);
}

/* the next point of `grid` at or after `from` on deck A's grid */
static uint32_t grid_at(int grid, uint32_t from)
{
    uint32_t step, u, r;
    switch (grid) {
    case MQ_16TH:  step = 12; break;
    case MQ_BEAT:  step = MUS_UPQ; break;
    case MQ_BAR:   step = bar_units(); break;
    case MQ_PHRASE: step = 4u * bar_units(); break;
    case MQ_NODE_END: {
        int i;
        uint32_t best = 0;
        int found = 0;
        for (i = 0; i < MUS_BAR_LOG; i++) {
            const MusBarLog *l = &C.log[i];
            if (l->bar == 0 && l->pos && !before(l->pos, from) && (!found || before(l->pos, best))) { best = l->pos; found = 1; }
        }
        if (found) return best;
        if (DECK_A_ON()) {
            uint32_t e = (C.a.d.acc + C.a.node_len) & MUS_POS_MASK;
            if (!before(e, from)) return e;
        }
        step = bar_units();
        break; }
    default:       return from;
    }
    if (!DECK_A_ON()) return from;
    u = (from - C.start) & MUS_POS_MASK;
    r = u % step;
    return r ? (from + step - r) & MUS_POS_MASK : from;
}

/* the earliest position a request can act on: one tick ahead (latency incl.) */
static uint32_t lead(void) { return (mus_core_pos_in(1) + C.lat) & MUS_POS_MASK; }

static uint16_t bar_of(uint32_t pos) { return (uint16_t)(((pos - C.start) & MUS_POS_MASK) / bar_units()); }

/* ---- params and intensity (sec.6.4) ------------------------------------------------------ */
static const uint8_t pdef[MP_COUNT] = { [MP_INTENSITY] = 128, [MP_HP] = 255 };

static void params_reset(void)
{
    int i;
    const MusParamDef *pd = (const MusParamDef *)(const void *)mus_bank_misc(MUS_MISC_PARAMS);
    for (i = 0; i < MP_COUNT; i++) C.ptgt[i] = C.pcur[i] = pd ? pd[i].def : pdef[i];
    C.pmove = 0;
}

static void params_step(void)
{
    const MusParamDef *pd = (const MusParamDef *)(const void *)mus_bank_misc(MUS_MISC_PARAMS);
    uint32_t m = C.pmove;
    int i;
    for (i = 0; m; i++, m >>= 1) {
        int d, s;
        if (!(m & 1)) continue;
        d = C.ptgt[i] - C.pcur[i];
        s = pd ? (d > 0 ? pd[i].rise : pd[i].fall) : 8;
        if (!s) s = 8;
        C.pcur[i] = (uint8_t)(d > 0 ? (d > s ? C.pcur[i] + s : C.ptgt[i]) : (-d > s ? C.pcur[i] - s : C.ptgt[i]));
        if (C.pcur[i] == C.ptgt[i]) C.pmove &= ~(1u << i);
    }
}

static int intensity_of(const MusCueView *cv)
{
    int i, sum = 0;
    if (!cv || !cv->cue->n_binds) return C.pcur[MP_INTENSITY];
    for (i = 0; i < cv->cue->n_binds && i < 4; i++) {
        const MusBind *b = &cv->binds[i];
        int x = C.pcur[b->param & 31];
        switch (b->curve & 3) {
        case 1: x = 255 - x; break;
        case 2: x = x * x / 255; break;
        case 3: x = x >= 128 ? 255 : 0; break;
        default: break;
        }
        sum += x * b->w_q4;
    }
    return DOOMO_CLAMP((sum >> 4) + cv->binds[0].bias, 0, 255);
}

void music_set_param(int p, int v)
{
    if (p < 0 || p >= MP_COUNT) return;
    C.ptgt[p] = (uint8_t)DOOMO_CLAMP(v, 0, 255);
    if (C.ptgt[p] != C.pcur[p]) C.pmove |= 1u << p;
}

void music_set_param_now(int p, int v)
{
    if (p < 0 || p >= MP_COUNT) return;
    C.ptgt[p] = C.pcur[p] = (uint8_t)DOOMO_CLAMP(v, 0, 255);
    C.pmove &= ~(1u << p);
}

int music_param(int p) { return (p >= 0 && p < MP_COUNT) ? C.pcur[p] : 0; }
int music_intensity(void) { return mus_intensity_now; }

/* ---- deck A at a bar line (mus_lane.c) ----------------------------------------------------- */
static void quote_at(uint32_t pos);

void mus_core_bar(MusGen *g, uint32_t pos, int node_start)
{
    const MusCue *c = g->cv.cue;
    if (g->deck != 0) return;
    if (!(c->flags & MCF_EXACT)) {
        uint16_t want = g->d.stems;
        int r, I = mus_intensity_now, tgt;
        for (r = 0; r < c->n_roles && r < 16; r++) {
            int on = (want >> r) & 1, ion = g->cv.roles[r].i_on;
            if (on && I < ion - 24) want &= (uint16_t)~(1u << r);
            else if (!on && I >= ion) want |= (uint16_t)(1u << r);
        }
        g->d.stems_next = want;
        if (want != g->d.stems && (g->d.dwell >= 4 || (g->d.picks == 0 && g->d.bar == 0))) {
            g->d.stems = want;
            g->d.dwell = 0;
            mus_plan(&g->cv, want, 0, g->vmap);
        }
        if (g->d.dwell < 255) g->d.dwell++;
        tgt = c->dial_home + ((c->dial_hi - c->dial_home) * music_param(MP_HELL)) / 255;
        g->d.dial_target = (uint8_t)DOOMO_CLAMP(tgt, c->dial_lo, c->dial_hi);
        if (g->d.dial < g->d.dial_target) g->d.dial = (uint8_t)DOOMO_MIN(g->d.dial + 64, g->d.dial_target);
        else if (g->d.dial > g->d.dial_target) g->d.dial = (uint8_t)DOOMO_MAX(g->d.dial - 64, g->d.dial_target);
    }
    if (g != &C.a) return;
    {
        MusBarLog *l = &C.log[C.log_i];
        C.log_i = (uint8_t)((C.log_i + 1) % MUS_BAR_LOG);
        l->pos = pos;
        l->stems = g->d.stems;
        l->node = g->d.node;
        l->pass = g->d.pass;
        l->bar = g->d.bar;
        l->dial = g->d.dial;
        l->var = g->var;
        l->ktrans = g->ktrans;
    }
    if (node_start) C.rest_on = (uint8_t)(c->rest_policy && c->rest && g->d.node == c->rest);
    C.bars++;
    if (C.q_motif) quote_at(pos);
}

const MusBarLog *mus_core_log_at(uint32_t pos)
{
    const MusBarLog *best = NULL;
    int i;
    for (i = 0; i < MUS_BAR_LOG; i++) {
        const MusBarLog *l = &C.log[i];
        if (!l->pos && !l->stems) continue;
        if (!before(pos, l->pos) && (!best || before(best->pos, l->pos))) best = l;
    }
    return best;
}

/* ---- the scrub decoder (deck B's storage) ---------------------------------------------------- */
static void drop_emit(void *ctx, uint32_t pos_kind, uint32_t arg) { (void)ctx; (void)pos_kind; (void)arg; }
#define LOG_VAR(l) ((l)->var == 0xFF ? 0xFE : (l)->var)
uint8_t mus_core_scrub(uint32_t P, MusScrubNote out[MUS_A_VOICES])
{
    const MusBarLog *l = mus_core_log_at(P);
    MusGen *s = &C.b;
    MusSink sink = { drop_emit, NULL, 1 << 30 };
    uint32_t ns;
    uint8_t got = 0, vm[MUS_A_ROLES];
    int k, r;
    if (!l || !DECK_A_ON()) return 0;
    ns = (l->pos - (uint32_t)l->bar * bar_units()) & MUS_POS_MASK;
    if (s->d.state != MGS_PLAY || s->cv.blob != C.a.cv.blob || s->d.acc != ns || s->d.node != l->node ||
        before(P, s->next) || C.b_mode != BM_SCRUB) {
        mus_core_b_claim(BM_SCRUB);
        memset(&s->d, 0, sizeof(s->d));
        s->cv = C.a.cv;
        s->d.state = MGS_PLAY;
        s->d.seed = C.a.d.seed;
        s->d.dial = l->dial;
        s->d.stems = l->stems;
        s->scale_q8 = 256;
        s->rate256 = C.a.rate256;
        s->mode = MGF_SCRUB;
        s->deck = 1;
        mus_plan(&C.a.cv, l->stems, 0, vm);        /* the plan of P's bar, not today's */
        for (r = 0, k = 0; r < C.a.n_roles && k < MUS_B_VOICES; r++) {
            int kind = C.a.cv.roles[r].kind;
            if (vm[r] >= MUS_A_VOICES || kind == RK_KIT || kind == RK_PERC) continue;
            memset(&s->v[k], 0, sizeof(s->v[k]));
            s->v[k].role = (uint8_t)r;
            s->vmap[k] = vm[r];
            k++;
        }
        s->n_roles = (uint8_t)k;
        mus_gen_enter(s, l->node, ns, l->pass, LOG_VAR(l));
        s->ktrans = l->ktrans;
        s->next = ns;
    }
    mus_gen_until(s, (P + 1) & MUS_POS_MASK, &sink);
    s->next = P;
    for (k = 0; k < s->n_roles; k++) {
        const MusVoiceRt *v = &s->v[k];
        uint32_t off = (P - ns) & MUS_POS_MASK;
        int vo = s->vmap[k];
        if (!v->nx_pitch || off < v->bar_off || off >= v->gate_u || vo >= MUS_A_VOICES) continue;
        out[vo].start = (ns + v->bar_off) & MUS_POS_MASK;
        out[vo].end = (ns + v->gate_u) & MUS_POS_MASK;
        out[vo].pitch = v->nx_pitch;
        out[vo].vel = v->accent;
        out[vo].inst = v->inst;
        got |= (uint8_t)(1u << vo);
    }
    return got;
}

void mus_core_catchup(uint32_t pos, uint8_t mask)
{
    C.cu_at = pos;
    C.cu_mask |= mask;
}

static void catchup_run(void)
{
    MusScrubNote nt[MUS_A_VOICES];
    uint8_t got;
    int i;
    if (!C.cu_mask || !DECK_A_ON() || C.b_mode == BM_DUAL || (C.b_mode == BM_STING && C.b.d.state == MGS_PLAY)) return;
    if (before(C.a.next, C.cu_at) || mus_drv_room(1) < 2 * MUS_A_VOICES) return;   /* not written yet */
    got = mus_core_scrub(C.cu_at, nt);
    for (i = 0; i < MUS_A_VOICES; i++) {
        uint32_t at = before(C.cu_at, clk()) ? clk() : C.cu_at;
        int dur;
        if (!((C.cu_mask & got) >> i & 1) || nt[i].start == C.cu_at || !before(at, nt[i].end)) continue;
        dur = mus_note_frames((int)mus_pdiff(nt[i].end, at), C.a.rate256);
        mus_drv_push(1, MEV_MAKE_POS(MEV_INST, at), MEV_INST_ARG(i, nt[i].inst, MUS_INST(nt[i].inst)->tparam, nt[i].vel));
        mus_drv_push(1, MEV_MAKE_POS(MEV_NOTE, at), MEV_NOTE_ARG(i, nt[i].pitch, nt[i].vel, DOOMO_MIN(dur, MEV_DUR_MAX), 0));
    }
    C.cu_mask = 0;
    if (C.b_mode == BM_SCRUB && !mus_vy.on) mus_core_b_claim(BM_IDLE);
}

/* ---- init, starts ------------------------------------------------------------------------------- */
static void ring_emit(void *ctx, uint32_t pos_kind, uint32_t arg)
{
    mus_drv_push((int)(intptr_t)ctx, pos_kind, arg);
}

void mus_core_init(void)
{
    DoomoAudioCaps caps;
    memset(&C, 0, sizeof(C));
    memset(&mus_vy, 0, sizeof(mus_vy));
    C.a.v = C.va;
    C.a.max_roles = MUS_A_ROLES;
    C.b.v = C.vb;
    C.b.max_roles = MUS_B_VOICES;
    C.b.deck = 1;
    mus_inst_tab = mus_bank_inst(0);
    mus_inst_n = mus_bank()->magic == MUS_BANK_MAGIC ? mus_bank()->n_inst : 0;
    mus_intensity_now = 128;
    mus_drv_reset();
    plat_audio_caps(&caps);
    C.bridge = (uint8_t)(caps.tier == AT_BRIDGE);
    C.lat = caps.latency_mt;
    mus_drv_set_bridge(C.bridge);
    mus_drv_set_ctl(caps.flags & CAP_DRV_IRQ ? 4 : MUS_MT_PER_TICK);
    params_reset();
    C.st_bar = C.vy_bar = C.q_bar = C.q_mbar = 0xFFFF;
    C.none = 1;
    C.inited = 1;
}

int mus_core_bridge(void) { if (!C.inited) mus_core_init(); return C.bridge; }

static void session_save(void)
{
    MusSession *s;
    if (!DECK_A_ON() || (C.a.cv.cue->flags & MCF_EXACT)) return;
    s = C.ses[0].cue == C.a.d.cue ? &C.ses[0] : &C.ses[1];
    if (s == &C.ses[1]) C.ses[1] = C.ses[0], s = &C.ses[0];
    s->cue = C.a.d.cue;
    s->node = C.a.d.node;
    s->picks = C.a.d.picks;
    memcpy(s->hist, C.a.d.hist, 8);
    memcpy(s->passes, C.a.d.passes, 32);
}

static void push_b(int kind, uint32_t pos, uint32_t arg)
{
    if (kind == MEV_MUTE || kind == MEV_GAIN || kind == MEV_FX) C.ctl_dirty = 1;
    mus_drv_push(1, MEV_MAKE_POS(kind, pos), arg);
}

static void vinyl_clear(void)
{
    if (mus_vy.on || mus_drv_vinyl_on()) mus_drv_push(1, MEV_MAKE_POS(MEV_FX, clk()), MFX_VINYL_END);
    memset(&mus_vy, 0, sizeof(mus_vy));
    mus_drv_vy.seg = NULL;
}

/* the ring-B controls a cut drops (duck and stinger gains, mute windows,
 * the platter) are set back explicitly, so nothing stays ducked or muted */
static void controls_reset(void)
{
    int i;
    uint32_t at = clk();
    if (!C.ctl_dirty) return;
    C.ctl_dirty = 0;
    push_b(MEV_MUTE, at, MEV_MUTE_ARG(MUS_ALL_A, 0, 0));
    push_b(MEV_GAIN, at, MEV_GAIN_ARG(24, 255, 0));
    for (i = 0; i < MUS_A_VOICES; i++) push_b(MEV_GAIN, at, MEV_GAIN_ARG(i, 255, 0));
}

static void cut_all(void)
{
    session_save();
    mus_drv_truncate(0);
    mus_drv_truncate(1);
    mus_drv_cut(0xFFFFFFFFu);
    mus_gen_stop(&C.a);
    mus_gen_stop(&C.b);
    C.b_mode = BM_IDLE;
    C.ended = 0;
    C.req = 0;
    C.cu_mask = 0;
    C.st_live = 0;
    C.seek = 0;
    C.stop_hold = 0;
    C.gate_to = C.gate_from;
    vinyl_clear();
    controls_reset();
    memset(C.log, 0, sizeof(C.log));
}

/* deck A starts cue cv at master position base (entry 0xFE = session memory) */
static void deck_a_start(const MusCueView *cv, uint32_t base, int once, int entry)
{
    int i;
    C.start = base;
    C.beat_next = base;
    C.none = 0;
    mus_drv_set_tempo((uint32_t)mus_rate256(cv->cue->bpm) * 32u);
    mus_gen_start(&C.a, cv, base, mus_rate256(cv->cue->bpm), once ? MGF_ONCE : 0, NULL);
    C.a.d.seed = mus_h(doomo_town.seed, S_SEED, (uint32_t)cv->id << 8 | doomo_town.story.chapter, doomo_town.day);
    for (i = 0; i < 2 && entry == 0xFE && !(cv->cue->flags & MCF_EXACT); i++)
        if (C.ses[i].cue == cv->id) {
            int node = C.ses[i].node < cv->cue->n_nodes ? C.ses[i].node : 0;
            C.a.d.picks = C.ses[i].picks;
            memcpy(C.a.d.hist, C.ses[i].hist, 8);
            memcpy(C.a.d.passes, C.ses[i].passes, 32);
            if (cv->cue->reentry < cv->cue->n_nodes) node = cv->cue->reentry;
            mus_gen_enter(&C.a, node, base, -1, 0xFF);
            break;
        }
    if (entry < cv->cue->n_nodes && entry != cv->cue->entry && i == 0) mus_gen_enter(&C.a, entry, base, -1, 0xFF);
    C.a.next = base;
}

void mus_core_start(int cue, int once)
{
    MusCueView cv;
    uint32_t base;
    if (!C.inited) mus_core_init();
    cut_all();
    base = mus_drv_rebase();
    C.start = base;
    C.none = 1;
    if (cue == MC_NONE || !mus_bank_cue(cue, &cv)) return;
    deck_a_start(&cv, base, once, cv.cue->entry);
}

/* audio_music_dual: a's lead + bass on voices 0 and 2 (and a's tempo), b's
 * lead on voice 1 (the harmony channel) and b's kit on voice 3 (SPEC sec.9.3). */
void mus_core_dual(int cue_a, int cue_b)
{
    static const uint8_t map_a[MUS_A_VOICES] = { 0, 0xFF, 2, 0xFF };
    static const uint8_t map_b[MUS_A_VOICES] = { 1, 0xFF, 0xFF, 3 };
    MusCueView ca, cb;
    uint32_t base;
    int32_t r;
    if (!C.inited) mus_core_init();
    cut_all();
    base = mus_drv_rebase();
    C.start = base;
    if (cue_a == MC_NONE || !mus_bank_cue(cue_a, &ca)) return;
    r = mus_rate256(ca.cue->bpm);
    deck_a_start(&ca, base, 0, ca.cue->entry);
    mus_gen_start(&C.a, &ca, base, r, 0, map_a);
    C.a.next = base;
    if (mus_bank_cue(cue_b, &cb)) {
        C.b_mode = BM_DUAL;
        mus_gen_start(&C.b, &cb, base, r, 0, map_b);
    }
}

int mus_core_take_end(void) { int e = C.ended; C.ended = 0; return e; }

void mus_core_test_irq(int on)
{
    if (!C.inited) mus_core_init();
    C.bridge = (uint8_t)!on;
    mus_drv_set_bridge(!on);
}

/* ---- requests and the matrix (sec.6.5, CF-18) -------------------------------------------------- */

static void mute_a(uint32_t from, uint32_t len, uint8_t mask)
{
    push_b(MEV_MUTE, from, MEV_MUTE_ARG(mask, 0, DOOMO_MIN(len, 0x7FFFu)));
}

/* a built-in fill (MusFill rows come with MU4d): a snare run, toms on the last
 * beat, on the kit voice under a deck-0 mute window; the crash rides the CUT */
static void fill_into(uint32_t at, uint32_t len)
{
    uint32_t u, from = (at - len) & MUS_POS_MASK;
    mute_a(from, len, 1u << 3);
    for (u = 0; u < len && mus_drv_room(1) > 4; u += 12)
        push_b(MEV_DRUM, (from + u) & MUS_POS_MASK,
               MEV_DRUM_ARG(3, len - u <= MUS_UPQ ? KH_TOM : KH_SNARE, KIT_CLASSIC, 40 + (int)(u * 20 / len), 0, 0));
    push_b(MEV_DRUM, at, MEV_DRUM_ARG(3, KH_CRASH, KIT_CLASSIC, 63, 0, 0));
}

void music_set_cue_ex(const MusCueReq *r)
{
    const MusTrans *t;
    MusCueView cv;
    uint32_t G, from;
    int type, arg, grid, wait;
    if (!r) return;
    if (!C.inited) mus_core_init();
    if (r->cue != MC_NONE && !mus_bank_cue(r->cue, &cv)) return;
    if (C.req && C.req_cue == r->cue) return;                      /* armed */
    if (!C.req && DECK_A_ON() && C.a.d.cue == r->cue && C.b_mode != BM_DUAL) return;   /* playing */
    if (!DECK_A_ON() || r->cue == MC_NONE) {                       /* nothing to leave: start now */
        if (r->cue == MC_NONE) { mus_core_start(MC_NONE, 0); return; }
        if (!DECK_A_ON() && !C.req) {
            mus_core_start(MC_NONE, 0);
            deck_a_start(&cv, mus_drv_rebase(), 0, r->entry ? r->entry : cv.cue->entry);
            return;
        }
    }
    t = mus_trans_find(DECK_A_ON() ? C.a.d.cue : MC_NONE, r->cue);
    grid = r->grid;
    type = r->type;
    arg = r->arg;
    if (t && !r->type) {
        type = t->type;
        arg = t->arg;
        if (grid != MQ_NOW) grid = t->grid;
        else if (type != MTT_CUT && type != MTT_VINYL && !(type == MTT_FILL && arg <= 1)) type = MTT_CUT;
    }
    from = lead();
    if (type == MTT_FILL && grid == MQ_NOW) grid = MQ_BEAT;
    G = grid_at(grid, from);
    wait = r->max_wait ? r->max_wait : (t ? t->max_wait : 0);
    if (wait && mus_pdiff(G, from) > wait * (int)MUS_UPQ) G = grid_at(MQ_BEAT, from);
    C.req_cue = r->cue;
    C.req_entry = (uint8_t)(r->entry ? r->entry : (t ? t->entry : 0));
    C.req_type = (uint8_t)type;
    C.req_cut = C.req_at = G;
    switch (type) {
    case MTT_FILL: {
        uint32_t len = (uint32_t)DOOMO_CLAMP(arg, 1, 2) * MUS_UPQ;
        if (mus_pdiff(G, from) < (int32_t)len) G = C.req_cut = C.req_at = grid_at(MQ_BAR, (from + len) & MUS_POS_MASK);
        fill_into(G, len);
        break; }
    case MTT_CROSS: {
        uint32_t len = (uint32_t)DOOMO_CLAMP(arg, 1, 8) * MUS_UPQ, f;
        G = C.req_cut = C.req_at = grid_at(MQ_BAR, (from + len) & MUS_POS_MASK);
        f = (G - len) & MUS_POS_MASK;
        for (wait = 0; wait < MUS_A_VOICES; wait++) {
            push_b(MEV_GAIN, f, MEV_GAIN_ARG(wait, 0, DOOMO_MIN((len * 65536u) / DOOMO_MAX(mus_drv_tempo(), 1u), 4095u)));
            push_b(MEV_GAIN, G, MEV_GAIN_ARG(wait, 255, 0));
        }
        break; }
    case MTT_SILENCE:
        C.req_at = (G + (uint32_t)DOOMO_CLAMP(arg, 1, 16) * MUS_UPQ) & MUS_POS_MASK;
        break;
    case MTT_VINYL:
        if (mus_core_vinyl_pos(arg ? arg : VINYL_NEEDLE, G, VSRC_AUTO, AFTER_NEXT_CUE) >= 0)
            C.req_cut = C.req_at = mus_vy.end;
        break;
    case MTT_SYNC:
        G = C.req_cut = C.req_at = grid_at(MQ_BEAT, from);
        break;
    default: break;                       /* CUT PICKUP BRIDGE PIVOT: a CUT at G */
    }
    C.req = 1;
}

void music_set_cue(int cue, int grid)
{
    MusCueReq r;
    memset(&r, 0, sizeof(r));
    r.cue = (uint8_t)cue;
    r.grid = (uint8_t)grid;
    music_set_cue_ex(&r);
}

void music_resume(int cue)
{
    MusCueReq r;
    memset(&r, 0, sizeof(r));
    r.cue = (uint8_t)cue;
    r.grid = MQ_BEAT;
    r.entry = 0xFE;
    music_set_cue_ex(&r);
}

/* the pending request, once its point is inside the look-ahead */
static void request_run(uint32_t until)
{
    MusCueView cv;
    int i;
    if (C.req == 1 && !before(until, C.req_cut)) {
        mus_drv_truncate_at(0, C.req_cut);
        if (DECK_A_ON())
            for (i = 0; i < MUS_A_VOICES; i++) mus_drv_push(0, MEV_MAKE_POS(MEV_OFF, C.req_cut), MEV_OFF_ARG(i, MOFF_CUT));
        session_save();
        mus_gen_stop(&C.a);
        memset(C.log, 0, sizeof(C.log));
        C.cu_mask = 0;
        C.req = 2;
    }
    if (C.req == 2 && !before(until, C.req_at)) {
        C.req = 0;
        if (C.req_cue == MC_NONE || !mus_bank_cue(C.req_cue, &cv)) { C.none = 1; return; }
        if (C.req_type == MTT_SYNC) {
            /* join at deck A's bar and beat: the new cue's grid keeps the old phase */
            uint32_t ph = ((C.req_at - C.start) & MUS_POS_MASK) % ((uint32_t)(cv.cue->meter ? cv.cue->meter : 4) * MUS_UPQ);
            MusSink drop = { drop_emit, NULL, 1 << 30 };
            deck_a_start(&cv, (C.req_at - ph) & MUS_POS_MASK, 0, C.req_entry ? C.req_entry : cv.cue->entry);
            C.a.next = mus_gen_until(&C.a, C.req_at, &drop);
            return;
        }
        deck_a_start(&cv, C.req_at, 0, C.req_entry ? C.req_entry : cv.cue->entry);
    }
}

/* CONTINUE (REWIND): deck A re-enters the music at the playhead P */
static void seek_run(uint32_t until)
{
    const MusBarLog *l;
    MusSink drop = { drop_emit, NULL, 1 << 30 };
    uint32_t ns, off, bu;
    if (!C.seek || before(until, C.seek_at) || !DECK_A_ON()) return;
    C.seek = 0;
    l = mus_core_log_at(C.seek_P);
    if (!l) return;
    bu = bar_units();
    ns = (l->pos - (uint32_t)l->bar * bu) & MUS_POS_MASK;
    off = (C.seek_P - ns) & MUS_POS_MASK;
    mus_drv_truncate_at(0, C.seek_at);
    C.a.d.dial = l->dial;
    C.a.d.stems = l->stems;
    mus_gen_enter(&C.a, l->node, (C.seek_at - off) & MUS_POS_MASK, l->pass, LOG_VAR(l));
    C.a.next = mus_gen_until(&C.a, C.seek_at, &drop);
    C.start = (C.seek_at - off) & MUS_POS_MASK;
    C.beat_next = grid_at(MQ_BEAT, C.seek_at);
    mus_core_catchup(C.seek_at, (uint8_t)MUS_ALL_A);
}

/* ---- stingers (sec.6.5) ------------------------------------------------------------------------- */
static int sting_start(int id, uint32_t H, int flags)
{
    MusCueView cv;
    const MusStingHdr *sh;
    MusSink drop = { drop_emit, NULL, 1 << 30 };
    uint32_t S, from = lead(), len = 0, back, scale;
    uint8_t vm[MUS_A_ROLES], mask = 0;
    int i, prio;
    if (!C.inited) mus_core_init();
    if (!mus_bank_sting(id, &cv) || !(sh = cv.sting)) return -1;
    if (C.b_mode == BM_DUAL || mus_vy.on) return -1;
    prio = sh->prio;
    if (C.st_live && (prio < C.st_prio || !(flags & STF_REPLACE))) return -1;
    if (DECK_A_ON() && bar_of(H) == C.st_bar) return -1;          /* house rule: <= 1 per bar */
    scale = DECK_A_ON() ? (uint32_t)C.a.cv.cue->bpm * 256u / DOOMO_MAX(cv.cue->bpm, 1) : 256u;
    S = (H - (sh->hit_units * scale >> 8)) & MUS_POS_MASK;
    mus_core_b_claim(BM_STING);
    mus_gen_start(&C.b, &cv, 0, mus_rate256(cv.cue->bpm), MGF_ONCE, NULL);
    C.b.scale_q8 = (uint16_t)DOOMO_CLAMP(scale, 64, 1024);
    C.b.base = S;
    C.b.d.acc = C.b.scale_q8 == 256 ? S : 0;
    if ((sh->mode & STM_KEY) && DECK_A_ON())
        C.b.ktrans = (int8_t)((((C.a.cv.cue->key & 15) - (cv.cue->key & 15)) + 18) % 12 - 6);
    C.b.next = S;
    if (before(S, from)) C.b.next = mus_gen_until(&C.b, from, &drop);   /* late: skip the pre-roll */
    for (i = 0; i <= cv.cue->tag && i < cv.cue->n_nodes; i++)
        len += (uint32_t)cv.nodes[i].bars * (cv.cue->meter ? cv.cue->meter : 4) * MUS_UPQ;
    len = len * C.b.scale_q8 >> 8;
    mus_plan(&cv, 0xFFFF, 1, vm);
    for (i = 0; i < cv.cue->n_roles && i < MUS_A_ROLES; i++) if (vm[i] < MUS_A_VOICES) mask |= (uint8_t)(1u << vm[i]);
    back = (S + len + (uint32_t)sh->gap_beats * MUS_UPQ) & MUS_POS_MASK;
    if (DECK_A_ON()) {
        back = grid_at(MQ_BEAT, back);
        mute_a(before(S, from) ? from : S, (back - (before(S, from) ? from : S)) & MUS_POS_MASK, mask);
        if (!(flags & STF_NO_DUCK) && sh->duck_q8 && sh->duck_q8 < 255)
            for (i = 0; i < MUS_A_VOICES; i++)
                if (!(mask >> i & 1)) {
                    push_b(MEV_GAIN, S, MEV_GAIN_ARG(i, sh->duck_q8, 16));
                    push_b(MEV_GAIN, back, MEV_GAIN_ARG(i, 255, 32));
                }
        mus_core_catchup(back, mask);
        C.st_bar = bar_of(H);
    }
    push_b(MEV_MARK, H, MEV_MARK_ARG(MMK_STING_HIT, 0, 0, 0));
    C.st_hit = H;
    C.st_back = back;
    C.st_live = 1;
    C.st_prio = (uint8_t)prio;
    C.st_mask = mask;
    return mus_core_ticks_until(H);
}

int music_sting(int sting, int grid, int flags)
{
    MusCueView cv;
    uint32_t H;
    if (!C.inited) mus_core_init();
    if (!mus_bank_sting(sting, &cv)) return -1;
    H = (lead() + (uint32_t)cv.sting->hit_units) & MUS_POS_MASK;
    H = grid_at(grid, H);
    return sting_start(sting, H, flags);
}

int music_sting_at(int sting, uint32_t land_tick, int flags)
{
    if (!C.inited) mus_core_init();
    if ((int32_t)(land_tick - C.now) < 1) return -1;
    return sting_start(sting, mus_core_pos_in((int)(land_tick - C.now)), flags);
}

/* ---- vinyl starts (sec.7.1, 7.3) ---------------------------------------------------------------------- */
int mus_core_vinyl_pos(int pattern, uint32_t S, int source, int after)
{
    MusVinylView vw;
    uint32_t E;
    (void)source;                                   /* TAPE is M7: DECK everywhere */
    if (!C.inited) mus_core_init();
    if (pattern == VINYL_CRACKLE) {
        C.crackle ^= 1;
        plat_audio_fx(MFX_CRACKLE, C.crackle, 0);
        return mus_core_ticks_until(S);
    }
    if (pattern == VINYL_NONE) {                    /* release a bend-only pattern */
        if (mus_drv_vinyl_on() && !mus_vy.on) push_b(MEV_FX, S, MFX_VINYL_END);
        return 0;
    }
    if (!DECK_A_ON() || (C.a.cv.cue->flags & MCF_NO_VINYL) || !mus_vinyl_get(pattern, &vw)) return -1;
    if (mus_vy.on || C.b_mode == BM_DUAL || (C.st_live && before(S, C.st_back))) return -1;
    if (!(C.a.cv.cue->flags & MCF_DJ) && C.vy_bar != 0xFFFF && (uint16_t)(bar_of(S) - C.vy_bar) < 8) return -1;
    C.vy_bar = bar_of(S);
    if (after == AFTER_DEFAULT) after = vw.after;
    if (after == AFTER_DEFAULT) after = AFTER_RESUME;
    mus_drv_vy = vw;
    mus_drv_vy_seed = mus_h(C.a.d.seed, S_VINYL, (uint32_t)pattern, C.bars);
    push_b(MEV_FX, S, MFX_VINYL_START);
    if (vw.flags & MVF_LOOP) return mus_core_ticks_until(S);   /* WOBBLE: bends only */
    E = (S + vw.len) & MUS_POS_MASK;
    push_b(MEV_FX, E, MFX_VINYL_END);
    mute_a(S, vw.len, (uint8_t)MUS_ALL_A);
    memset(&mus_vy, 0, sizeof(mus_vy));
    mus_vy.start = S;
    mus_vy.end = E;
    mus_vy.seed = mus_drv_vy_seed;
    mus_vy.P = S;
    mus_vy.pattern = (uint8_t)pattern;
    mus_vy.after = (uint8_t)after;
    mus_vy.on = 1;
    switch (after) {
    case AFTER_CONTINUE:
        C.seek = 1;
        C.seek_at = E;
        if (vw.flags & MVF_SNAP)                    /* REWIND: 1-2 bars back, in time */
            C.seek_P = (E - (1u + (mus_vy.seed & 1u)) * bar_units()) & MUS_POS_MASK;
        else C.seek_P = (S + (uint32_t)((mus_vinyl_disp(&vw, vw.len, mus_vy.seed) + 128) >> 8)) & MUS_POS_MASK;
        break;
    case AFTER_STOP:
        mute_a(E, 0x7FFF, (uint8_t)MUS_ALL_A);
        C.stop_hold = 1;
        C.stop_at = S;
        break;
    case AFTER_NEXT_CUE: break;                     /* the matrix's request starts at E */
    default: mus_core_catchup(E, (uint8_t)MUS_ALL_A); break;
    }
    return mus_core_ticks_until(S);
}

void mus_core_vinyl_done(void)
{
    mus_vy.on = 0;
    if (C.b_mode == BM_SCRUB && !C.cu_mask) mus_core_b_claim(BM_IDLE);
}

int music_vinyl(int pattern, int grid, int source, int after)
{
    if (!C.inited) mus_core_init();
    return mus_core_vinyl_pos(pattern, grid_at(grid, lead()), source, after);
}

int music_vinyl_at(int pattern, uint32_t land_tick, int source, int after)
{
    if (!C.inited) mus_core_init();
    if ((int32_t)(land_tick - C.now) < 1) return -1;
    return mus_core_vinyl_pos(pattern, mus_core_pos_in((int)(land_tick - C.now)), source, after);
}

/* the DJ booth: a live platter over deck A (0x7FFF releases, RESUME) */
void music_vinyl_live(int16_t speed_q8, uint8_t fader)
{
    if (!C.inited) mus_core_init();
    if (speed_q8 == 0x7FFF) {
        if (mus_vy.live) {
            uint32_t at = lead();
            push_b(MEV_FX, at, MFX_VINYL_END);
            push_b(MEV_MUTE, at, MEV_MUTE_ARG(MUS_ALL_A, 0, 0));
            mus_core_catchup(at, (uint8_t)MUS_ALL_A);
            memset(&mus_vy, 0, sizeof(mus_vy));
        }
        return;
    }
    mus_drv_vinyl_live(speed_q8, fader);
    if (!mus_vy.live && DECK_A_ON() && !mus_vy.on) {
        uint32_t at = lead();
        mus_drv_vy.seg = NULL;
        push_b(MEV_FX, at, MFX_VINYL_START);
        mute_a(at, 0x7FFF, (uint8_t)MUS_ALL_A);
        mus_vy.live = 1;
        mus_vy.start = mus_vy.P = at;
    }
}

void music_resume_on_beat(void)
{
    uint32_t B;
    if (!C.inited) mus_core_init();
    if (!DECK_A_ON() || (!C.stop_hold && !before(clk(), C.gate_to))) { C.stop_hold = 0; return; }
    B = grid_at(MQ_BEAT, lead());
    push_b(MEV_MUTE, B, MEV_MUTE_ARG(MUS_ALL_A, 0, 0));
    mus_core_catchup(B, (uint8_t)MUS_ALL_A);
    C.stop_hold = 0;
    C.gate_to = C.gate_from = clk();
}

/* ---- windows (sec.9.2) ------------------------------------------------------------------------------- */
void music_mute_window(int from, int to)
{
    uint32_t A, B;
    if (!C.inited) mus_core_init();
    if (to <= from || !DECK_A_ON()) return;
    A = mus_core_pos_in(DOOMO_MAX(from, 1));
    B = mus_core_pos_in(to);
    mute_a(A, (B - A) & MUS_POS_MASK, (uint8_t)MUS_ALL_A);
    mus_core_catchup(B, (uint8_t)MUS_ALL_A);
    C.gate_from = A;
    C.gate_to = B;
}

void music_duck_window(int from, int to, int gain_q8)
{
    if (!C.inited) mus_core_init();
    if (to <= from) return;
    push_b(MEV_GAIN, mus_core_pos_in(DOOMO_MAX(from, 1)), MEV_GAIN_ARG(24, DOOMO_CLAMP(gain_q8, 0, 255), 16));
    push_b(MEV_GAIN, mus_core_pos_in(to), MEV_GAIN_ARG(24, 255, 32));
}

void music_pulse_duck(int gain_q8, int ticks) { music_duck_window(0, ticks, gain_q8); }

int music_gated(void)
{
    uint32_t c = clk();
    if (!C.inited) return 0;
    if (audio_fx_music_gated() || C.none || !DECK_A_ON()) return 1;
    if (C.stop_hold && !before(c, C.stop_at)) return 1;
    return !before(c, C.gate_from) && before(c, C.gate_to);
}

int music_resting(void) { return DECK_A_ON() && C.rest_on; }

/* ---- quotes (sec.8.4) --------------------------------------------------------------------------------- */
void music_quote(int motif, int prio)
{
    if (!C.inited) mus_core_init();
    if (motif < 0 || motif > 254 || (C.q_motif && prio < C.q_prio)) return;
    C.q_motif = (uint8_t)(motif + 1);                 /* 0 = none (MOTIF_HOOK is id 0) */
    C.q_prio = (uint8_t)prio;
}

/* at a deck-A bar line: the motif on the QUOTE role's voice (cooldowns:
 * 1 quote per 4 bars, the same motif per 16). A motif (MU4d's motifs.dms)
 * is scale degrees from its tonic: it sounds in the cue's key, in the cue's
 * mode unless it is MMF_FIXED (WALL, the Riff's b5 keep their own), its
 * head literally (sec.14), from the tonic of octave 5. */
static void quote_at(uint32_t pos)
{
    static const uint16_t len_tab[16] = MUS_LEN_TAB_INIT;
    const uint8_t *tab = mus_bank_misc(MUS_MISC_MOTIFS), *m, *sc;
    const MusCue *c = C.a.cv.cue;
    uint32_t u = 0;
    int r, voice = -1, i, n, id = C.q_motif - 1, tonic = 60 + (c->key & 15);
    if ((c->flags & (MCF_NO_QUOTES | MCF_EXACT)) || C.st_live || mus_vy.on) return;
    if ((uint16_t)(C.bars - C.q_bar) < 4 || (C.q_motif == C.q_last && (uint16_t)(C.bars - C.q_mbar) < 16)) return;
    if (!tab || id >= mus_bank()->n_motif || !mus_rd16(tab + 2 * id)) { C.q_motif = 0; return; }
    for (r = 0; r < C.a.n_roles; r++)
        if ((C.a.cv.roles[r].flags & RF_QUOTE) && C.a.vmap[r] < MUS_A_VOICES) { voice = C.a.vmap[r]; break; }
    if (voice < 0) return;
    m = tab + mus_rd16(tab + 2 * id);
    n = DOOMO_MIN(m[0], 12);
    sc = mus_scale((m[2] & MMF_FIXED) ? (m[2] & 15) : (c->key >> 4));
    if (mus_drv_room(1) < n + 4) return;
    for (i = 0; i < n; i++) u += len_tab[m[5 + 2 * i] & 15];
    mute_a(pos, u, (uint8_t)(1u << voice));
    push_b(MEV_INST, pos, MEV_INST_ARG(voice, C.a.v[r].inst, 0, 48));
    for (i = 0, u = 0; i < n; i++) {
        int deg = (int8_t)m[4 + 2 * i], b = m[5 + 2 * i], l = len_tab[b & 15], o = 0, acc = (b >> 4) & 3;
        uint32_t at = (pos + u) & MUS_POS_MASK;
        while (deg < 0) { deg += 7; o -= 12; }
        o += 12 * (deg / 7) + sc[deg % 7] + (acc == 1 ? 1 : acc == 2 ? -1 : 0);
        if (!(b & MMN_REST))
            push_b(MEV_NOTE, at, MEV_NOTE_ARG(voice, DOOMO_CLAMP(tonic + o + C.a.ktrans, 0, 127), 48,
                                              mus_note_frames(l, C.a.rate256) * 7 / 8 + 1, 0));
        u += (uint32_t)l;
    }
    mus_core_catchup((pos + u) & MUS_POS_MASK, (uint8_t)(1u << voice));
    C.q_bar = C.bars;
    C.q_last = C.q_motif;
    C.q_mbar = C.bars;
    C.q_motif = 0;
}

/* ---- beds (sec.6.5): seeded emitters while deck A rests or is silent ------------------------------------ */
typedef struct BedDef { uint8_t voice, inst, rate, lo, span, vol, len, hours; } BedDef;
enum { BH_ANY, BH_DAY, BH_NIGHT };
static const BedDef beds[MUS_BED_COUNT] = {
    /* BIRDS     */ { 0, MUS_INST_BED0 + 0, 18, 84, 12, 30, 6, BH_DAY },
    /* CRICKETS  */ { 1, MUS_INST_BED0 + 0, 40, 100, 2, 14, 3, BH_NIGHT },
    /* WIND      */ { 3, MUS_INST_BED0 + 1, 6, 40, 12, 16, 96, BH_ANY },
    /* RAIN      */ { 3, MUS_INST_BED0 + 2, 90, 90, 12, 14, 3, BH_ANY },
    /* RIVER     */ { 3, MUS_INST_BED0 + 1, 20, 60, 6, 12, 24, BH_ANY },
    /* LAVA      */ { 2, MUS_INST_BED0 + 3, 8, 28, 5, 30, 48, BH_ANY },
    /* VAPE      */ { 3, MUS_INST_BED0 + 1, 10, 96, 6, 10, 48, BH_ANY },
    /* CROWD     */ { 3, MUS_INST_BED0 + 1, 30, 48, 12, 14, 12, BH_ANY },
    /* CLOCK     */ { 3, MUS_INST_BED0 + 2, 0, 100, 0, 20, 2, BH_ANY },
    /* MACHINERY */ { 2, MUS_INST_BED0 + 3, 24, 31, 7, 24, 6, BH_ANY },
    /* EMBERS    */ { 3, MUS_INST_BED0 + 2, 30, 110, 10, 12, 2, BH_ANY },
    /* FIREWORKS */ { 3, MUS_INST_BED0 + 1, 5, 70, 20, 40, 18, BH_NIGHT },
};

void music_beds(uint16_t mask) { if (!C.inited) mus_core_init(); C.beds = mask; }

static void beds_run(int beat)
{
    uint32_t at;
    uint8_t busy = 0;
    int i, h, night;
    if (!C.beds || (DECK_A_ON() && !C.rest_on && !C.stop_hold) || mus_drv_room(1) < 8) return;
    h = town_hour();
    night = h < 6 || h >= 20;
    at = mus_core_pos_in(1);
    for (i = 0; i < MUS_BED_COUNT; i++) {
        const BedDef *b = &beds[i];
        uint32_t x;
        if (!(C.beds >> i & 1) || (busy >> b->voice & 1)) continue;
        if ((b->hours == BH_DAY && night) || (b->hours == BH_NIGHT && !night)) continue;
        x = mus_h(doomo_town.seed, S_BED, (uint32_t)i, C.now);
        if (b->rate ? (x & 255) >= b->rate : !beat) continue;
        busy |= (uint8_t)(1u << b->voice);
        push_b(MEV_INST, at, MEV_INST_ARG(b->voice, b->inst, 0, b->vol));
        push_b(MEV_NOTE, at, MEV_NOTE_ARG(b->voice, b->lo + (b->span ? (int)((x >> 8) % b->span) : 0), b->vol,
                                          b->len, 0));
    }
}

/* ---- the comic bridge: positions, landing, beat events (sec.9.2) ----------------------------------------- */
uint32_t music_now(void) { return C.now; }

void music_pos(MusPos *out)
{
    uint32_t u, c;
    int meter;
    const MusBarLog *l;
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!DECK_A_ON()) return;
    c = (clk() - C.lat) & MUS_POS_MASK;
    meter = C.a.cv.cue->meter ? C.a.cv.cue->meter : 4;
    u = (c - C.start) & MUS_POS_MASK;
    out->mt = mus_drv_mt();
    out->bar = (uint16_t)(u / (uint32_t)(meter * MUS_UPQ));
    out->beat = (uint8_t)((u / MUS_UPQ) % (uint32_t)meter);
    out->sub48 = (uint8_t)(u % MUS_UPQ);
    out->cue = C.a.d.cue;
    l = mus_core_log_at(c);
    out->node = l ? l->node : C.a.d.node;
    out->pass = l ? l->pass : C.a.d.pass;
    out->beats_per_bar = (uint8_t)meter;
    out->bpm_q4 = C.a.d.bpm_q4;
    out->key = C.a.cv.cue->key;
    out->chord_root = (uint8_t)(C.a.cv.cue->key & 15);
}

int music_ticks_to(int grid)
{
    if (!DECK_A_ON() || grid <= MQ_NOW || grid >= MQ_MARKER) return 0;
    return mus_core_ticks_until(grid_at(grid, (clk() + 1) & MUS_POS_MASK));
}

/* the driver's clock after mt driver ticks: upm1 until the bend at posT
 * dispatches, upm2 after (the driver's own law) -> the mt reaching G */
static uint32_t land_sim(uint32_t upm1, uint32_t posT, uint32_t upm2, uint32_t G, uint32_t max)
{
    uint32_t c = clk(), f = mus_drv_clock_frac(), u = upm1, m;
    int bent = 0;
    for (m = 1; m <= max; m++) {
        f += u & 0xFFFFu;
        c = (c + (u >> 16) + (f >> 16)) & MUS_POS_MASK;
        f &= 0xFFFFu;
        if (!before(c, G)) return m;
        if (!bent && !before(c, posT)) { u = upm2; bent = 1; }
    }
    return max + 1;
}

int music_land(int grid, int frames, int max_bend_q8)
{
    uint32_t G, posT, upm = mus_drv_tempo(), lo, hi, mid, best, mt;
    int land;
    if (!C.inited) mus_core_init();
    if (!DECK_A_ON() || frames <= 0) return frames;
    G = grid_at(grid ? grid : MQ_BEAT, (clk() + 1) & MUS_POS_MASK);
    land = mus_core_ticks_until(G);
    max_bend_q8 = DOOMO_CLAMP(max_bend_q8, 0, 31);              /* <= 12 % */
    if (frames < 3 || land == frames || frames > 600) return land - frames;
    posT = mus_core_pos_in(2);
    if (!before(posT, G)) return land - frames;
    lo = upm - upm * (uint32_t)max_bend_q8 / 256u;
    hi = upm + upm * (uint32_t)max_bend_q8 / 256u;
    mt = (uint32_t)frames * 8u;
    /* the slowest tempo that still reaches G within `frames` ticks */
    if (land_sim(upm, posT, hi, G, mt + 8) > mt) best = hi;
    else {
        while (lo < hi) {
            mid = lo + (hi - lo) / 2;
            if (land_sim(upm, posT, mid, G, mt + 8) <= mt) hi = mid; else lo = mid + 1;
        }
        best = lo;
    }
    land = (int)((land_sim(upm, posT, best, G, (uint32_t)frames * 16u + 16u) + 7) / 8);
    if (best != upm) {
        push_b(MEV_TEMPO, posT, MEV_TEMPO_ARG(best, 0));
        push_b(MEV_TEMPO, G, MEV_TEMPO_ARG(upm, 0));
    }
    return land - frames;
}

int music_on_beat(MusBeatFn fn, uint8_t mask)
{
    int i;
    for (i = 0; i < MUS_BEAT_CBS; i++)
        if (!C.cb[i]) { C.cb[i] = fn; C.cb_mask[i] = mask; return i; }
    return -1;
}

void music_off_beat(int slot) { if (slot >= 0 && slot < MUS_BEAT_CBS) C.cb[slot] = NULL; }

int music_beat_events(MusBeatEv *out, int max)
{
    int n = 0;
    while (C.bq_n && n < max) {
        out[n++] = C.bq[C.bq_h];
        C.bq_h = (uint8_t)((C.bq_h + 1) % MUS_BEATQ);
        C.bq_n--;
    }
    return n;
}

static void beat_ev(int kind, uint32_t pos)
{
    MusBeatEv e;
    const MusBarLog *l = mus_core_log_at(pos);
    int i;
    memset(&e, 0, sizeof(e));
    e.kind = (uint8_t)kind;
    e.node = l ? l->node : 0;
    e.bar = bar_of(pos);
    e.beat = (uint8_t)((((pos - C.start) & MUS_POS_MASK) / MUS_UPQ) % (bar_units() / MUS_UPQ));
    e.due = (int8_t)DOOMO_CLAMP(mus_pdiff(clk(), pos) * 256 / (int32_t)DOOMO_MAX(C.a.rate256, 1), 0, 127);
    e.cue = C.a.d.cue;
    e.key = C.a.cv.cue ? C.a.cv.cue->key : 0;
    e.chord_root = (uint8_t)(e.key & 15);
    i = (C.bq_h + C.bq_n) % MUS_BEATQ;
    if (C.bq_n == MUS_BEATQ) C.bq_h = (uint8_t)((C.bq_h + 1) % MUS_BEATQ);
    else C.bq_n++;
    C.bq[i] = e;
    for (i = 0; i < MUS_BEAT_CBS; i++) if (C.cb[i] && (C.cb_mask[i] & kind)) C.cb[i](&e);
}

static void beats_run(uint8_t marks)
{
    uint32_t c = (clk() - C.lat) & MUS_POS_MASK;
    int n = 0, beat = 0;
    if (marks & MMK_STING_HIT) beat_ev(MB_STING_HIT, c);
    if (marks & MMK_MARKER) beat_ev(MB_MARKER, c);
    if (DECK_A_ON()) {
        while (!before(c, C.beat_next) && n++ < 2) {
            const MusBarLog *l = mus_core_log_at(C.beat_next);
            int k = MB_BEAT;
            if (l && l->pos == C.beat_next) {
                k |= MB_BAR;
                if (l->bar == 0) k |= MB_NODE;
                if ((l->bar & 3) == 0) k |= MB_PHRASE;
            }
            if (C.beat_next == C.start) k |= MB_CUE;    /* fresh or resumed */
            beat_ev(k, C.beat_next);
            C.beat_next = (C.beat_next + MUS_UPQ) & MUS_POS_MASK;
            beat = 1;
        }
        if (n > 2) C.beat_next = grid_at(MQ_BEAT, c);
    }
    if (C.st_live && !before(c, C.st_back)) {
        C.st_live = 0;
        if (C.b_mode == BM_STING) C.b_mode = BM_IDLE;
    }
    beds_run(beat);
}

/* lane commands of deck A (mus_lane.c): VINYL (DJ moments, the Studio lane), BED */
void mus_core_lane_cmd(MusGen *g, int cmd, int a, uint32_t pos)
{
    if (g != &C.a) return;
    if (cmd == MTC_VINYL) mus_core_vinyl_pos(a, pos, VSRC_AUTO, AFTER_DEFAULT);
    else if (cmd == MTC_BED && a < MUS_BED_COUNT) C.beds ^= (uint16_t)(1u << a);
}

/* ---- the tick ---------------------------------------------------------------------------------------------- */
static void fill(MusGen *g, int ring, uint32_t until)
{
    MusSink s;
    uint32_t w;
    if (g->d.state != MGS_PLAY) {
        if (ring == 0) mus_drv_written(0, (until + 0x01000000u) & MUS_POS_MASK);
        return;
    }
    if (mus_pdiff(g->next, until) >= 0) {   /* nothing due yet: the common tick */
        if (ring == 0) mus_drv_written(0, g->next);
        return;
    }
    s.emit = ring_emit;
    s.ctx = (void *)(intptr_t)ring;
    s.room = mus_drv_room(ring) - (ring ? 8 : 0);           /* ring B keeps room for controls */
    w = mus_gen_until(g, until, &s);
    g->next = w;
    if (ring == 0) mus_drv_written(0, g->d.state == MGS_PLAY ? w : (until + 0x01000000u) & MUS_POS_MASK);
}

void music_update(void)
{
    uint32_t until, ua;
    uint8_t marks;
    if (!C.inited) mus_core_init();
    mus_drv_set_master(opt_get(OPT_MUSIC_VOL));
    if (C.pmove) params_step();
    mus_intensity_now = (uint8_t)intensity_of(DECK_A_ON() ? &C.a.cv : NULL);
    /* look-ahead: >= 1 bar for an IRQ driver (D5); on the bridge the driver
     * runs right below, so two ticks of music are enough (and a new cue
     * never decodes a whole bar on its first tick) */
    until = (clk() + mus_core_lookahead()) & MUS_POS_MASK;
    if (C.req) request_run(until);
    seek_run(until);
    ua = until;
    if (C.req == 1 && before(C.req_cut, ua)) ua = C.req_cut;
    if (C.seek && before(C.seek_at, ua)) ua = C.seek_at;
    fill(&C.a, 0, ua);
    if (C.b_mode == BM_DUAL || C.b_mode == BM_STING) fill(&C.b, 1, until);
    if (C.cu_mask) catchup_run();
    if (mus_vy.on) mus_vinyl_update();
    if (C.bridge) mus_drv_run(MUS_MT_PER_TICK);
    C.now++;
    marks = mus_drv_take_marks();
    if (marks & MMK_END) C.ended = 1;
    beats_run(marks);
}

/* Before a blocking load (PS1 CdRead): write ms of music ahead, or until
 * the ring is full (sec.6.2). On the bridge nothing plays during a load. */
void music_prefill(int ms)
{
    uint32_t units;
    if (!C.inited) mus_core_init();
    if (!DECK_A_ON() || ms <= 0 || C.req || C.seek) return;
    units = (uint32_t)ms * C.a.cv.cue->bpm * MUS_UPQ / 60000u + 1u;
    fill(&C.a, 0, (clk() + units) & MUS_POS_MASK);
    if (C.b_mode == BM_DUAL) fill(&C.b, 1, (clk() + units) & MUS_POS_MASK);
}

void music_stall(int kind)
{
    if (!C.inited) mus_core_init();
    plat_music_stall(kind);
}

void music_profile(int scene_class)
{
    DoomoAudioCaps caps;
    if (!C.inited) mus_core_init();
    C.profile = (uint8_t)scene_class;
    plat_audio_caps(&caps);
    if (caps.flags & CAP_DRV_IRQ) mus_drv_set_ctl(scene_class == AP_UI_EDGE ? 8 : 4);
    plat_audio_fx(MFX_STEMS, scene_class, 0);
}

uint16_t music_snap_pitch(uint16_t pitch16)
{
    if (!DECK_A_ON() || (C.a.cv.cue->flags & MCF_EXACT)) return pitch16;
    return (uint16_t)mus_scale_snap(C.a.cv.cue->key, pitch16);
}

/* D14: the synth SFX play from the bank once mmlc imports them (MC 232..);
 * until then (and until S1's M3 swap) the classic synth plays them */
void music_sfx_synth(int sfx)
{
    if (C.sfx_in) return;                   /* after the swap: no recursion */
    C.sfx_in = 1;
    audio_sfx_synth(sfx);
    C.sfx_in = 0;
}

/* ---- the public API (cues) ---------------------------------------------------------------------------------- */
void music_set_cue_dual(int cue_a, int cue_b) { mus_core_dual(cue_a, cue_b); }
int  music_cue(void) { return DECK_A_ON() ? C.a.d.cue : MC_NONE; }
int  music_cue_exists(int cue) { MusCueView cv; return mus_bank_cue(cue, &cv); }

const char *music_cue_name(int cue)
{
    MusCueView cv;
    if (!mus_bank_cue(cue, &cv) || !cv.cue->common_len || cv.kind != MDK_LEGACY) return "";
    return (const char *)cv.blob + cv.cue->common_off;   /* classic: the display name */
}

uint32_t music_cue_length(int cue)
{
    MusCueView cv;
    uint32_t units = 0;
    int i, last, meter;
    int32_t r;
    if (!mus_bank_cue(cue, &cv)) return 0;
    meter = cv.cue->meter ? cv.cue->meter : 4;
    last = (cv.cue->flags & MCF_EXACT) ? cv.cue->tag : 0;        /* classic: pass 1 */
    if (!(cv.cue->flags & MCF_EXACT) && DECK_A_ON() && C.a.d.cue == cue) last = -1;
    if (last < 0) units = C.a.node_len;                             /* adaptive: this node */
    for (i = 0; i <= last && i < cv.cue->n_nodes; i++) units += (uint32_t)cv.nodes[i].bars * meter * MUS_UPQ;
    r = mus_rate256(cv.cue->bpm);
    return (uint32_t)(((int64_t)units * 256 + r - 1) / r);
}

/* ---- hash mode (T2, CF-33) ----------------------------------------------------------------------------------- */
typedef struct HashCtx { uint32_t h, n; } HashCtx;

static void hash_emit(void *ctx, uint32_t pos_kind, uint32_t arg)
{
    HashCtx *x = (HashCtx *)ctx;
    uint32_t w[2], i;
    w[0] = pos_kind;
    w[1] = arg;
    for (i = 0; i < 8; i++) {
        x->h ^= (w[i >> 2] >> ((i & 3) * 8)) & 0xFFu;
        x->h *= 0x01000193u;
    }
    x->n++;
}

uint32_t music_hash(int cue, int minutes)
{
    MusGen g;
    MusVoiceRt v[MUS_A_ROLES];
    MusCueView cv;
    MusSink s;
    HashCtx x;
    uint32_t until, end, at = 0;
    uint8_t keep = mus_intensity_now;
    if (!mus_bank_cue(cue, &cv)) return 0;
    memset(&g, 0, sizeof(g));
    g.v = v;
    g.max_roles = MUS_A_ROLES;
    mus_intensity_now = 128;
    mus_gen_start(&g, &cv, 0, mus_rate256(cv.cue->bpm), 0, NULL);
    g.d.seed = mus_h(0x5EED, S_SEED, (uint32_t)cue << 8, 0);
    x.h = 0x811C9DC5u;
    x.n = 0;
    s.emit = hash_emit;
    s.ctx = &x;
    end = (uint32_t)minutes * cv.cue->bpm * MUS_UPQ;          /* a minute = bpm quarters */
    while (at < end && g.d.state == MGS_PLAY) {               /* 1 bar of look-ahead per step */
        until = DOOMO_MIN(at + 192u, end);
        s.room = 1 << 30;
        mus_gen_until(&g, until, &s);
        at = until;
    }
    mus_intensity_now = keep;
    return x.h ^ x.n;
}
