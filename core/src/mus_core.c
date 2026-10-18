/* mus_core.c - the bank, music_update(), cue requests and the hash
 * (owner: MU1; docs/design/music/SPEC.md sec.4, 6.2, 9.1, CF-33).
 *
 * M1 (engine, rings, driver, legacy bridge, classic cues): deck A plays
 * one cue; deck B plays the second song of audio_music_dual(). Requests
 * start NOW (CF-18: audio_music() keeps today's timing): ring A is cut,
 * the deck restarts at the driver's clock (its fraction dropped, so the
 * cue's grid starts exactly where the classic sequencer's did). Every
 * music_update() writes >= 1 bar ahead (or until the ring is full), and on
 * the legacy bridge it then runs the driver for the tick's 8 mt itself.
 *
 * music_hash() is the T2 "hash mode" (CF-33): a private deck, generation
 * driven by position, FNV-1a over every event in merge order. It never
 * touches the playing music. */
#include "mus_int.h"

/* ---- the bank ------------------------------------------------------------------------ */
const MusBank *mus_bank(void) { return (const MusBank *)(const void *)doomo_mus_bank; }

static const MusDirEnt *dir_find(int kind_sting, int id)
{
    const MusBank *b = mus_bank();
    const MusDirEnt *dir;
    int lo, hi;
    if (b->magic != MUS_BANK_MAGIC || id < 0 || id > 255) return NULL;
    dir = (const MusDirEnt *)(const void *)(doomo_mus_bank + b->dir_off);
    lo = kind_sting ? b->n_cues : 0;
    hi = kind_sting ? b->n_cues + b->n_stings : b->n_cues;
    while (lo < hi) {                                     /* sorted by id */
        int mid = (lo + hi) / 2;
        if (dir[mid].id < id) lo = mid + 1;
        else hi = mid;
    }
    return (lo < (kind_sting ? b->n_cues + b->n_stings : b->n_cues) && dir[lo].id == id) ? &dir[lo] : NULL;
}

int mus_bank_cue(int id, MusCueView *out)
{
    const MusDirEnt *e = dir_find(0, id);
    const uint8_t *blob;
    int roles_at;
    if (!e || !out) return 0;
    blob = doomo_mus_bank + e->off;
    out->blob = blob;
    out->cue = (const MusCue *)(const void *)blob;
    out->len = e->len;
    out->id = (uint8_t)id;
    out->kind = (uint8_t)MUS_DIR_KIND(e);
    roles_at = (int)sizeof(MusCue) + (out->kind == MDK_STING ? (int)sizeof(MusStingHdr) : 0);
    out->roles = (const MusRole *)(const void *)(blob + roles_at);
    out->nodes = (const MusNode *)(const void *)(blob + roles_at + out->cue->n_roles * (int)sizeof(MusRole));
    out->edges = (const MusEdge *)(const void *)((const uint8_t *)out->nodes + out->cue->n_nodes * (int)sizeof(MusNode));
    return out->cue->n_nodes > 0;
}

const MusInst *mus_bank_inst(int i)
{
    static const MusInst silent = { TIMBRE_PULSE, TIMBRE_PULSE, 0, 255, 0, 255, 0, 8, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0 };
    const MusBank *b = mus_bank();
    if (b->magic != MUS_BANK_MAGIC || i < 0 || i >= b->n_inst) return &silent;
    return (const MusInst *)(const void *)(doomo_mus_bank + b->inst_off) + i;
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

/* ---- decks and rings ---------------------------------------------------------------------- */
static struct {
    MusGen a, b;
    MusVoiceRt va[MUS_A_VOICES], vb[MUS_B_VOICES];
    uint32_t start;                 /* master position deck A's cue started at */
    uint8_t inited, ended, bridge, pad;
} C;

static int32_t pdiff28(uint32_t a, uint32_t b) { return (int32_t)((a - b) << 4) >> 4; }

static void ring_emit(void *ctx, uint32_t pos_kind, uint32_t arg)
{
    mus_drv_push((int)(intptr_t)ctx, pos_kind, arg);
}

void mus_core_init(void)
{
    DoomoAudioCaps caps;
    memset(&C, 0, sizeof(C));
    C.a.v = C.va;
    C.a.max_roles = MUS_A_VOICES;
    C.b.v = C.vb;
    C.b.max_roles = MUS_B_VOICES;
    C.b.deck = 1;
    mus_inst_tab = mus_bank_inst(0);
    mus_inst_n = mus_bank()->magic == MUS_BANK_MAGIC ? mus_bank()->n_inst : 0;
    mus_drv_reset();
    plat_audio_caps(&caps);
    C.bridge = (uint8_t)(caps.tier == AT_BRIDGE);
    mus_drv_set_bridge(C.bridge);
    mus_drv_set_ctl(caps.flags & CAP_DRV_IRQ ? 4 : MUS_MT_PER_TICK);
    C.inited = 1;
}

int mus_core_bridge(void) { if (!C.inited) mus_core_init(); return C.bridge; }

static void cut_all(void)
{
    mus_drv_truncate(0);
    mus_drv_truncate(1);
    mus_drv_cut(0xFFFFFFFFu);
    mus_gen_stop(&C.a);
    mus_gen_stop(&C.b);
    C.ended = 0;
}

void mus_core_start(int cue, int once)
{
    MusCueView cv;
    uint32_t base;
    if (!C.inited) mus_core_init();
    cut_all();
    base = mus_drv_rebase();
    C.start = base;
    if (cue == MC_NONE || !mus_bank_cue(cue, &cv)) return;
    mus_drv_set_tempo((uint32_t)mus_rate256(cv.cue->bpm) * 32u);
    mus_gen_start(&C.a, &cv, base, mus_rate256(cv.cue->bpm), once ? MGF_ONCE : 0, NULL);
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
    mus_drv_set_tempo((uint32_t)r * 32u);
    mus_gen_start(&C.a, &ca, base, r, 0, map_a);
    if (mus_bank_cue(cue_b, &cb)) mus_gen_start(&C.b, &cb, base, r, 0, map_b);
}

int mus_core_take_end(void) { int e = C.ended; C.ended = 0; return e; }

void mus_core_test_irq(int on)
{
    if (!C.inited) mus_core_init();
    C.bridge = (uint8_t)!on;
    mus_drv_set_bridge(!on);
}

static void fill(MusGen *g, int ring, uint32_t until)
{
    MusSink s;
    uint32_t w;
    if (g->d.state != MGS_PLAY) {
        if (ring == 0) mus_drv_written(0, (until + 0x01000000u) & MUS_POS_MASK);
        return;
    }
    if (pdiff28(g->next, until) >= 0) {   /* nothing due yet: the common tick */
        if (ring == 0) mus_drv_written(0, g->next);
        return;
    }
    s.emit = ring_emit;
    s.ctx = (void *)(intptr_t)ring;
    s.room = mus_drv_room(ring);
    w = mus_gen_until(g, until, &s);
    g->next = w;
    if (ring == 0) mus_drv_written(0, g->d.state == MGS_PLAY ? w : (until + 0x01000000u) & MUS_POS_MASK);
}

void music_update(void)
{
    uint32_t until;
    if (!C.inited) mus_core_init();
    mus_drv_set_master(opt_get(OPT_MUSIC_VOL));
    /* look-ahead: >= 1 bar for an IRQ driver (D5); on the bridge the driver
     * runs right below, so two ticks of music are enough (and a new cue
     * never decodes a whole bar on its first tick) */
    until = (mus_drv_clock() + (C.bridge ? (uint32_t)(C.a.rate256 >> 7) + 2u : MUS_LOOKAHEAD_U)) & MUS_POS_MASK;
    fill(&C.a, 0, until);
    fill(&C.b, 1, until);
    if (C.bridge) mus_drv_run(MUS_MT_PER_TICK);
    if (mus_drv_take_marks() & MMK_END) C.ended = 1;
}

/* Before a blocking load (PS1 CdRead): write ms of music ahead, or until
 * the ring is full (sec.6.2). On the bridge nothing plays during a load. */
void music_prefill(int ms)
{
    uint32_t units;
    if (!C.inited) mus_core_init();
    if (C.a.d.state != MGS_PLAY || ms <= 0) return;
    units = (uint32_t)ms * C.a.cv.cue->bpm * MUS_UPQ / 60000u + 1u;
    fill(&C.a, 0, (mus_drv_clock() + units) & MUS_POS_MASK);
    fill(&C.b, 1, (mus_drv_clock() + units) & MUS_POS_MASK);
}

/* ---- the public API (M1 part) ---------------------------------------------------------------- */
void music_set_cue(int cue, int grid)
{
    (void)grid;                         /* M1: every grid starts NOW (CF-18) */
    if (!C.inited) mus_core_init();
    if (cue != MC_NONE && C.a.d.state == MGS_PLAY && C.a.d.cue == cue && C.b.d.state != MGS_PLAY) return;
    mus_core_start(cue, 0);
}

void music_set_cue_ex(const MusCueReq *r) { if (r) music_set_cue(r->cue, r->grid); }
void music_set_cue_dual(int cue_a, int cue_b) { mus_core_dual(cue_a, cue_b); }
int  music_cue(void) { return C.a.d.state == MGS_PLAY ? C.a.d.cue : MC_NONE; }
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
    for (i = 0; i <= last && i < cv.cue->n_nodes; i++) units += (uint32_t)cv.nodes[i].bars * meter * MUS_UPQ;
    r = mus_rate256(cv.cue->bpm);
    return (uint32_t)(((int64_t)units * 256 + r - 1) / r);
}

void music_pos(MusPos *out)
{
    uint32_t u;
    int meter;
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (C.a.d.state != MGS_PLAY) return;
    meter = C.a.cv.cue->meter ? C.a.cv.cue->meter : 4;
    u = (mus_drv_clock() - C.start) & MUS_POS_MASK;
    out->bar = (uint16_t)(u / (uint32_t)(meter * MUS_UPQ));
    out->beat = (uint8_t)((u / MUS_UPQ) % (uint32_t)meter);
    out->sub48 = (uint8_t)(u % MUS_UPQ);
    out->cue = C.a.d.cue;
    out->node = C.a.d.node;
    out->pass = C.a.d.pass;
    out->beats_per_bar = (uint8_t)meter;
    out->bpm_q4 = C.a.d.bpm_q4;
}

int music_ticks_to(int grid)
{
    uint32_t u, step, left;
    int32_t r;
    if (C.a.d.state != MGS_PLAY) return 0;
    step = grid == MQ_16TH ? 12u : grid == MQ_BEAT ? MUS_UPQ :
           grid == MQ_BAR ? (uint32_t)(C.a.cv.cue->meter ? C.a.cv.cue->meter : 4) * MUS_UPQ : 0u;
    if (!step) return 0;
    u = (mus_drv_clock() - C.start) & MUS_POS_MASK;
    left = step - u % step;
    r = mus_rate256(C.a.cv.cue->bpm);
    return (int)(((int64_t)left * 256 + r - 1) / r);
}

/* ---- hash mode (T2, CF-33) ----------------------------------------------------------------------- */
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
    MusVoiceRt v[MUS_A_VOICES];
    MusCueView cv;
    MusSink s;
    HashCtx x;
    uint32_t until, end, at = 0;
    if (!mus_bank_cue(cue, &cv)) return 0;
    memset(&g, 0, sizeof(g));
    g.v = v;
    g.max_roles = MUS_A_VOICES;
    mus_gen_start(&g, &cv, 0, mus_rate256(cv.cue->bpm), 0, NULL);
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
    return x.h ^ x.n;
}
