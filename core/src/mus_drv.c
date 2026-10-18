/* mus_drv.c - the music driver (owner: MU1; docs/design/music/SPEC.md
 * sec.6.1-6.2, 9.4, CF-6, CF-19).
 *
 * mus_drv_tick() runs once per mt (1/240 s): from the platform's IRQ
 * (CAP_DRV_IRQ, GBA M2 / PS1 M3) or, on the weak legacy bridge, as
 * mus_drv_run(8) inside music_update(). Per mt it
 *   - advances the master clock by upm_q16 (units per mt, Q16), or stops at
 *     the LOGIC fence,
 *   - dispatches every due event of rings A and B (the earlier head first,
 *     ring A on ties: a stable merge), honouring the mute windows,
 *   - and every `ctl_mt`-th mt runs one control frame: per voice the
 *     envelope, vibrato, arpeggio, sweep and gains, then plat_voice() for
 *     the voices whose output may have changed.
 *
 * THE CLASSIC LAW. A control frame applies the instrument exactly the way
 * the 30 Hz sequencer's chan_shape() did per tick (level 0..240 in 1/16
 * steps, decay / release per frame, vibrato phase per frame, arpeggio step
 * = frame & 3, sweep per frame). With ctl_mt = 8 (the bridge) the frames
 * ARE the old ticks and the output is tick-for-tick the old sound (T1).
 * MU2/MU3 run 4 mt frames for 60 Hz arps and vibrato; MIF_ENV30
 * instruments then step their envelope every other frame (M2).
 *
 * COST (GBA, THUMB from ROM). No division anywhere on the per-mt or per-
 * frame paths (ring indices wrap by compare, volumes go through a
 * multiply-shift and tables), the instrument table is cached, idle voices
 * cost one test per frame, and with nothing playing a tick only moves the
 * clock.
 *
 * Integer only. The main thread writes ring tails; mus_drv_tick() only
 * reads them and moves the heads (single producer, single consumer). */
#include "mus_int.h"

/* the classic kit (the old sequencer's drums[]): the envelope here, the
 * frequency sweep in the renderer (audio_seq.c's bridge, MU2/MU3 later) */
const MusKitHit mus_kit_classic[KH_COUNT] = {
    { 0, 0, 0, 0, 0, 0 },
    { 1800, 120, 15, 40, 3, 0 },     /* kick: a noisy thump falling fast */
    { 5200, 3000, 13, 30, 2, 0 },    /* snare                            */
    { 16000, 16000, 8, 80, 0, 0 },   /* closed hat                       */
    { 14000, 12000, 8, 14, 0, 0 },   /* open hat                         */
    { 11000, 7000, 12, 7, 8, 0 },    /* crash                            */
    { 1100, 520, 13, 36, 4, 0 },     /* tom                              */
    { 3600, 2600, 12, 34, 1, 0 },    /* clap                             */
};

static const int8_t arp_tab[MARP_COUNT][4] = {
    { 0, 0, 0, 0 }, { 0, 4, 7, 4 }, { 0, 3, 7, 3 }, { 0, 12, 0, 12 },
    { 0, 7, 12, 7 }, { 0, 5, 7, 5 }, { 0, 3, 6, 3 }, { 0, 4, 7, 11 },
    { 0, 2, 0, 2 }
};

/* the classic 0..15 volume as 0..63 (v * 63 / 15 rounded): exactly
 * invertible by mus_vol15_of63[] (the bridge) */
const uint8_t mus_vol63_of15[16] = { 0, 4, 8, 13, 17, 21, 25, 29, 34, 38, 42, 46, 50, 55, 59, 63 };

enum { ST_ATTACK, ST_DECAY, ST_SUSTAIN, ST_RELEASE, ST_OFF };
enum { DV_DRUM = 1, DV_DIRTY = 2, DV_KEYON = 4 };      /* DrvVoice.flags */

typedef struct MuteWin { uint32_t from, to; uint16_t vmask; uint8_t live, pad; } MuteWin;

static struct {
    DoomoMusEv a[MUS_RING_A];
    DoomoMusEv b[MUS_RING_B];
    volatile uint8_t a_head, a_tail, b_head, b_tail;
    uint32_t clk_u;              /* master units                                  */
    uint16_t clk_f;              /* Q16 fraction                                  */
    uint8_t  ctl_mt, ctl_t;
    uint32_t upm;                /* units per mt, Q16                             */
    uint32_t fence, written;
    uint32_t keyons, underruns;
    uint8_t  live;               /* bit v: voice v sounds or owes a final record  */
    uint8_t  nwin;               /* live mute windows                             */
    DrvVoice v[MUS_DRV_VOICES];
    uint8_t  gstep[MUS_DRV_VOICES];
    MuteWin  win[MUS_WINDOWS];
    uint8_t  master, master_sent, fence_on, marks;
    uint8_t  bus[8];
    uint8_t  bridge;             /* render straight into mus_bridge_tones[]       */
    uint8_t  wave[MUS_DRV_VOICES];  /* the bridge: each voice's WAVE_* (its inst)   */
    uint8_t  tempo_pending;      /* TEMPO events in the rings (mus_drv_run)       */
} D;

/* signed distance a - b on the 28-bit master clock */
static int32_t pdiff(uint32_t a, uint32_t b)
{
    return (int32_t)((a - b) << 4) >> 4;
}

#define inst_row(i) MUS_INST(i)

void mus_drv_reset(void)
{
    int i;
    memset(&D, 0, sizeof(D));
    D.ctl_mt = MUS_MT_PER_TICK;
    D.upm = (uint32_t)mus_rate256(120) * 32u;
    D.master = 10;
    D.master_sent = 10;
    for (i = 0; i < 8; i++) D.bus[i] = 255;
    for (i = 0; i < MUS_DRV_VOICES; i++) {
        D.v[i].stage = ST_OFF;
        D.v[i].gain = 255;
        D.v[i].gain_to = 255;
        D.v[i].flags = DV_DIRTY;          /* every voice owes the platform a silent record */
    }
    D.live = (uint8_t)((1u << MUS_DRV_VOICES) - 1u);
}

int mus_drv_ring_used(int r)
{
    int h = r ? D.b_head : D.a_head, t = r ? D.b_tail : D.a_tail;
    return t >= h ? t - h : t + (r ? MUS_RING_B : MUS_RING_A) - h;
}

int mus_drv_room(int r) { return (r ? MUS_RING_B : MUS_RING_A) - 1 - mus_drv_ring_used(r); }

void mus_drv_set_bridge(int on) { D.bridge = (uint8_t)(on != 0); }

void mus_drv_push(int r, uint32_t pos_kind, uint32_t arg)
{
    if ((pos_kind >> 28) == MEV_TEMPO) D.tempo_pending++;
    if (r) {
        uint8_t t = D.b_tail, n = (uint8_t)(t + 1 == MUS_RING_B ? 0 : t + 1);
        if (n == D.b_head) return;
        D.b[t].pos = pos_kind; D.b[t].arg = arg;
        D.b_tail = n;
    } else {
        uint8_t t = D.a_tail, n = (uint8_t)(t + 1 == MUS_RING_A ? 0 : t + 1);
        if (n == D.a_head) return;
        D.a[t].pos = pos_kind; D.a[t].arg = arg;
        D.a_tail = n;
    }
}

/* CUT: drop every event not dispatched yet (IRQ-safe on the consoles once
 * MU2/MU3 mask around it; on the bridge it is the same thread). */
void mus_drv_truncate(int r)
{
    if (r) D.b_tail = D.b_head; else D.a_tail = D.a_head;
    if (D.a_head == D.a_tail && D.b_head == D.b_tail) D.tempo_pending = 0;
}

void mus_drv_cut(uint32_t mask)
{
    int i;
    for (i = 0; i < MUS_DRV_VOICES; i++)
        if (mask & (1u << i)) {
            DrvVoice *v = &D.v[i];
            if (v->stage == ST_OFF && !(v->flags & DV_DIRTY) && !(D.live & (1u << i))) continue;
            v->stage = ST_OFF;
            v->env = 0;
            v->flags = (uint8_t)((v->flags & ~DV_KEYON) | DV_DIRTY);
            D.live |= (uint8_t)(1u << i);
        }
}

void mus_drv_set_tempo(uint32_t upm_q16) { D.upm = upm_q16; }
uint32_t mus_drv_rebase(void) { D.clk_f = 0; return D.clk_u; }
uint32_t mus_drv_clock(void) { return D.clk_u; }
void mus_drv_written(int r, uint32_t pos) { if (!r) D.written = pos; }
uint8_t mus_drv_take_marks(void) { uint8_t m = D.marks; D.marks = 0; return m; }
uint32_t mus_drv_keyons(void) { return D.keyons; }
uint32_t mus_drv_underruns(void) { return D.underruns; }
void mus_drv_set_ctl(int mt) { D.ctl_mt = (uint8_t)(mt == 4 ? 4 : 8); }
void mus_drv_fence(int on, uint32_t pos) { D.fence_on = (uint8_t)(on != 0); D.fence = pos & MUS_POS_MASK; }
const DrvVoice *mus_drv_voice(int v) { return (v >= 0 && v < MUS_DRV_VOICES) ? &D.v[v] : NULL; }

void mus_drv_set_master(int vol10)
{
    D.master = (uint8_t)DOOMO_CLAMP(vol10, 0, 10);
}

/* ---- the legacy bridge's renderer (D.bridge): classic tones, no records ---------------- */
static uint8_t legacy_wave(const MusInst *in)
{
    switch (in->timbre) {
    case TIMBRE_PULSE: return (uint8_t)(in->tparam <= 4 ? WAVE_PULSE12 : in->tparam <= 8 ? WAVE_PULSE25 : WAVE_PULSE50);
    case TIMBRE_TRI: return WAVE_TRIANGLE;
    case TIMBRE_SAW: return WAVE_SAW;
    case TIMBRE_NOISE15: case TIMBRE_NOISE7: return WAVE_NOISE;
    default: return WAVE_PULSE50;
    }
}

/* C4..C5 in Hz * 64: 1/16 semitones above C0 -> Hz, the classic pitch_hz()
 * (audio_seq.c), here so the bridge frame needs no call */
static const uint16_t note_hz64[13] = {
    16744, 17740, 18795, 19912, 21096, 22351, 23680, 25088, 26580, 28160,
    29834, 31609, 33488
};

static uint16_t hz_of(int32_t p16)
{
    int32_t semi, oct, n;
    uint32_t a, f;
    if (p16 < 0) p16 = 0;
    if (p16 > 16 * 12 * 9 - 1) p16 = 16 * 12 * 9 - 1;
    semi = p16 >> 4;
    oct = (semi * 5462) >> 16;                     /* semi / 12, no divide */
    n = semi - oct * 12;
    a = note_hz64[n];
    f = a + ((note_hz64[n + 1] - a) * (uint32_t)(p16 & 15) >> 4);
    f = (f << oct) >> 10;
    return (uint16_t)(f < 20 ? 20 : f > 16000 ? 16000 : f);
}

/* the classic (level * vol * master + 1200) / 2400 with vel = vol * 4, as a
 * multiply-shift (exact for every reachable value, T13 checks) */
static int vol15_of(int level, int vel)
{
    uint32_t x = ((uint32_t)level * (uint32_t)vel * D.master + 4800u) >> 2;
    int v = (int)((x * 55925u) >> 27);
    return v > 15 ? 15 : v;
}

/* the same with an explicit master (T13 checks it against the division) */
int mus_drv_vol15(int level, int vel, int master)
{
    uint32_t x = ((uint32_t)level * (uint32_t)vel * (uint32_t)master + 4800u) >> 2;
    int v = (int)((x * 55925u) >> 27);
    return v > 15 ? 15 : v;
}

static void bridge_out(int i, const DrvVoice *v, uint16_t f, int level, int w)
{
    DoomoToneState *o = &mus_bridge_tones[i];
    int vol = level > 0 ? vol15_of(level, v->vel) : 0;
    if (vol && (v->gain != 255 || D.bus[0] != 255)) {
        vol = (vol * v->gain * 257) >> 16;
        vol = (vol * D.bus[0] * 257) >> 16;
    }
    o->freq = f;
    o->vol = (uint8_t)vol;
    o->wave = (uint8_t)w;
}

/* fx_sin(), inlined: the vibrato's phase is a byte (256 steps a cycle) */
static int32_t vib_sin(int a)
{
    a &= DOOMO_ANG_MASK;
    if (a < 1024) return doomo_sin_q[a];
    if (a < 2048) return doomo_sin_q[2048 - a];
    if (a < 3072) return -doomo_sin_q[a - 2048];
    return -doomo_sin_q[4096 - a];
}

/* ---- dispatch ------------------------------------------------------------------------- */
static int muted(int voice, uint32_t pos)
{
    int i;
    if (!D.nwin) return 0;
    for (i = 0; i < MUS_WINDOWS; i++) {
        const MuteWin *w = &D.win[i];
        if (w->live && (w->vmask & (1u << voice)) && pdiff(pos, w->from) >= 0 && pdiff(pos, w->to) < 0)
            return 1;
    }
    return 0;
}

static void dispatch(const DoomoMusEv *e)
{
    uint32_t a = e->arg, pos = MEV_POS(e);
    int k = MEV_KIND(e), vi = MEV_VOICE(a);
    DrvVoice *v = vi < MUS_DRV_VOICES ? &D.v[vi] : NULL;
    switch (k) {
    case MEV_NOTE:
        if (!v || muted(vi, pos)) break;
        {
            const MusInst *in = inst_row(v->inst);
            int dur = MEV_NOTE_DUR(a);
            v->flags = (uint8_t)(DV_DIRTY | DV_KEYON);
            v->pitch16 = (int16_t)(MEV_NOTE_PITCH(a) * 16);
            v->vel = (uint8_t)MEV_NOTE_VEL(a);
            v->age = 0;
            v->arp_i = 0;
            v->vib_t = 0;
            v->stage = in->attack ? ST_ATTACK : ST_DECAY;
            v->env = (uint8_t)(in->attack ? 0 : in->level * 16);
            v->gate = (uint16_t)(dur ? dur : 0xFFFF);
            D.wave[vi] = legacy_wave(in);
            D.live |= (uint8_t)(1u << vi);
            D.keyons++;
        }
        break;
    case MEV_DRUM:
        if (!v || muted(vi, pos)) break;
        {
            int hit = MEV_DRUM_HIT(a);
            v->flags = (uint8_t)(DV_DRUM | DV_DIRTY | DV_KEYON);
            v->chord_q = (uint8_t)(hit | (MEV_DRUM_KIT(a) << 3));
            v->vel = (uint8_t)MEV_DRUM_VEL(a);
            v->age = 0;
            v->env = (uint8_t)(mus_kit_classic[hit].vol * 16);
            v->stage = ST_DECAY;
            D.live |= (uint8_t)(1u << vi);
            D.keyons++;
        }
        break;
    case MEV_OFF:
        if (!v) break;
        if (MEV_OFF_MODE(a) == MOFF_RELEASE) {
            if (!(v->flags & DV_DRUM) && v->stage < ST_RELEASE) { v->stage = ST_RELEASE; v->flags |= DV_DIRTY; }
        } else if (MEV_OFF_MODE(a) == MOFF_CUT) {
            mus_drv_cut(1u << vi);
        } else {
            mus_drv_cut(0xFFFFFFFFu);
        }
        break;
    case MEV_INST:
        if (!v) break;
        v->inst = (uint8_t)MEV_INST_INST(a);
        v->vel = (uint8_t)MEV_INST_VEL(a);
        D.wave[vi] = legacy_wave(inst_row(v->inst));
        v->flags |= DV_DIRTY;
        break;
    case MEV_PAN:
        if (!v) break;
        v->pan = (uint8_t)((a >> 5) & 63u);
        v->flags |= DV_DIRTY;
        break;
    case MEV_GAIN: {
        int t = (int)(a & 31u), g = MEV_GAIN_GAIN(a), ramp = MEV_GAIN_RAMP(a), frames;
        if (t >= 24) { D.bus[t - 24] = (uint8_t)g; for (t = 0; t < MUS_DRV_VOICES; t++) D.v[t].flags |= DV_DIRTY; break; }
        if (!v) break;
        frames = ramp / (D.ctl_mt ? D.ctl_mt : 8);
        v->gain_to = (uint8_t)g;
        if (frames <= 0) { v->gain = (uint8_t)g; D.gstep[vi] = 0; }
        else {
            int d = g - v->gain;
            if (d < 0) d = -d;
            D.gstep[vi] = (uint8_t)DOOMO_CLAMP((d + frames - 1) / frames, 1, 255);
        }
        v->flags |= DV_DIRTY;
        break; }
    case MEV_TEMPO:
        D.upm = a & 0xFFFFFFu;
        if (D.tempo_pending) D.tempo_pending--;
        break;
    case MEV_MARK:
        D.marks |= (uint8_t)(a & 0xFFu);
        break;
    case MEV_MUTE: {
        /* M1: role_mask = driver voice mask (deck A roles are voices 0..3) */
        int i, slot = 0;
        uint32_t mask = (uint32_t)MEV_MUTE_MASK(a);
        for (i = 0; i < MUS_WINDOWS; i++) if (!D.win[i].live) { slot = i; break; }
        if (i == MUS_WINDOWS) {           /* full: replace the one ending first */
            for (i = 1; i < MUS_WINDOWS; i++) if (pdiff(D.win[i].to, D.win[slot].to) < 0) slot = i;
        } else D.nwin++;
        D.win[slot].from = pos;
        D.win[slot].to = (pos + MEV_MUTE_LEN(a)) & MUS_POS_MASK;
        D.win[slot].vmask = (uint16_t)mask;
        D.win[slot].live = 1;
        mus_drv_cut(mask);
        break; }
    default:                              /* CHORD PITCH FX: M4 */
        break;
    }
}

static void dispatch_due(void)
{
    for (;;) {
        int da = D.a_head != D.a_tail && pdiff(D.clk_u, MEV_POS(&D.a[D.a_head])) >= 0;
        int db = D.b_head != D.b_tail && pdiff(D.clk_u, MEV_POS(&D.b[D.b_head])) >= 0;
        DoomoMusEv e;
        if (!da && !db) break;
        /* the earlier head first; on a tie ring A, except a ring-B MUTE,
         * which opens its window before the notes it silences */
        if (da && (!db || (pdiff(MEV_POS(&D.b[D.b_head]), MEV_POS(&D.a[D.a_head])) >= 0 &&
                           !(MEV_KIND(&D.b[D.b_head]) == MEV_MUTE &&
                             MEV_POS(&D.b[D.b_head]) == MEV_POS(&D.a[D.a_head]))))) {
            e = D.a[D.a_head];
            D.a_head = (uint8_t)(D.a_head + 1 == MUS_RING_A ? 0 : D.a_head + 1);
        } else {
            e = D.b[D.b_head];
            D.b_head = (uint8_t)(D.b_head + 1 == MUS_RING_B ? 0 : D.b_head + 1);
        }
        dispatch(&e);
    }
}

/* ---- control frames (the classic law) ------------------------------------------------------- */
static void emit(int i, DrvVoice *v, int32_t p, int level, int tparam, int inst)
{
    DoomoVoice o;
    int vol = 0;
    if (level > 0) {
        vol = mus_vol63_of15[vol15_of(level, v->vel)];
        if (v->gain != 255) vol = (vol * v->gain * 257) >> 16;
        if (D.bus[0] != 255) vol = (vol * D.bus[0] * 257) >> 16;
    }
    o.pitch16 = (uint16_t)p;
    o.vol_l = o.vol_r = (uint8_t)vol;
    o.env = (uint8_t)level;
    o.inst = (uint8_t)inst;
    o.tparam = (uint8_t)tparam;
    o.flags = (uint8_t)((v->flags & DV_KEYON) ? DVF_KEYON : 0);
    o.mt = 0;
    o.prio = 0;
    o.pad[0] = o.pad[1] = 0;
    v->flags &= (uint8_t)~(DV_KEYON | DV_DIRTY);
    plat_voice(i, &o);
}

static void control(int i)
{
    DrvVoice *v = &D.v[i];
    if (v->gain != v->gain_to) {
        int d = v->gain_to - v->gain, s = D.gstep[i] ? D.gstep[i] : 255;
        v->gain = (uint8_t)(d > 0 ? (d > s ? v->gain + s : v->gain_to) : (-d > s ? v->gain - s : v->gain_to));
        v->flags |= DV_DIRTY;
    }
    if (v->stage == ST_OFF) {
        if (v->flags & DV_DIRTY) {
            if (D.bridge && i < 4) { bridge_out(i, v, 0, 0, WAVE_PULSE50); v->flags &= (uint8_t)~(DV_KEYON | DV_DIRTY); }
            else emit(i, v, 0, 0, 0, (v->flags & DV_DRUM) ? MUS_INST_KIT0 : v->inst);
        }
        D.live &= (uint8_t)~(1u << i);
        return;
    }
    if (v->flags & DV_DRUM) {
        int hit = v->chord_q & 7, kit = v->chord_q >> 3, t = v->age, level = v->env;
        const MusKitHit *d = &mus_kit_classic[hit];
        int nl = level - d->decay16;
        if (nl <= 0) { nl = 0; v->stage = ST_OFF; }
        if (hit == KH_CLAP && t == 1) nl = d->vol * 16;
        v->env = (uint8_t)nl;
        if (D.bridge && i < 4) {
            bridge_out(i, v, (t >= d->ticks_sweep || d->ticks_sweep == 0) ? d->f1 :
                       (uint16_t)(d->f0 + ((int32_t)d->f1 - d->f0) * t / d->ticks_sweep), level, WAVE_NOISE);
            v->flags &= (uint8_t)~(DV_KEYON | DV_DIRTY);
        } else emit(i, v, MUS_VOICE_KIT(hit, kit), level, t, MUS_INST_KIT0 + kit);
        if (v->stage == ST_OFF) v->flags |= DV_DIRTY;
        else if (v->age < 255) v->age++;
        return;
    }
    {
        const MusInst *in = inst_row(v->inst);
        int32_t p;
        int level = v->env, mod = 0, st0 = v->stage;
        if (v->gate == 0 && v->stage < ST_RELEASE) v->stage = ST_RELEASE;
        switch (v->stage) {
        case ST_ATTACK: {
            int peak = in->level * 16;
            level += in->attack > 1 ? peak / in->attack : peak;
            if (level >= peak) { level = peak; v->stage = ST_DECAY; }
            break; }
        case ST_DECAY: {
            int sus = in->sustain * 16;
            level -= in->decay;
            if (level <= sus) { level = sus; v->stage = ST_SUSTAIN; }
            break; }
        case ST_SUSTAIN:
            break;
        default:
            level -= in->release > 1 ? in->release : 1;
            if (level <= 0) { level = 0; v->stage = ST_OFF; }
            break;
        }
        if (level != v->env || v->stage != st0) mod = 1;
        v->env = (uint8_t)level;
        if (in->sweep) {
            int32_t s = (int32_t)v->pitch16 + in->sweep;
            v->pitch16 = (int16_t)(s < -32768 ? -32768 : s > 32767 ? 32767 : s);
            mod = 1;
        }
        p = v->pitch16;
        if (in->vib_depth && v->age >= in->vib_delay) {
            p += (vib_sin(v->vib_t * 16) * (int32_t)in->vib_depth) >> 16;   /* fits 32 bits */
            v->vib_t = (uint8_t)(v->vib_t + in->vib_speed);
            mod = 1;
        }
        if (in->arp) { p += arp_tab[in->arp < MARP_COUNT ? in->arp : 0][v->arp_i & 3] * 16; mod = 1; }
        if (p < 1) p = 1;
        if (p > 16 * 12 * 9 - 1) p = 16 * 12 * 9 - 1;
        if (mod || (v->flags & DV_DIRTY)) {
            if (D.bridge && i < 4) { bridge_out(i, v, hz_of(p), level, D.wave[i]); v->flags &= (uint8_t)~(DV_KEYON | DV_DIRTY); }
            else emit(i, v, p, level, in->tparam, v->inst);
        }
        if (v->stage == ST_OFF) { v->flags |= DV_DIRTY; return; }
        if (v->age < 255) v->age++;
        v->arp_i = (uint8_t)((v->arp_i + 1) & 3);
        if (v->gate && v->gate != 0xFFFF) v->gate--;
    }
}

static void frame(void)
{
    int i;
    if (D.master != D.master_sent) {
        D.master_sent = D.master;
        for (i = 0; i < MUS_DRV_VOICES; i++) { D.v[i].flags |= DV_DIRTY; D.live |= (uint8_t)(1u << i); }
    }
    for (i = 0; i < MUS_DRV_VOICES; i++)
        if (D.live & (1u << i)) control(i);
}

void mus_drv_tick(void)
{
    if (!(D.fence_on && pdiff(D.clk_u, D.fence) >= 0)) {
        uint32_t f = (uint32_t)D.clk_f + (D.upm & 0xFFFFu);
        D.clk_u = (D.clk_u + (D.upm >> 16) + (f >> 16)) & MUS_POS_MASK;
        D.clk_f = (uint16_t)f;
        if (D.fence_on && pdiff(D.clk_u, D.fence) > 0) { D.clk_u = D.fence; D.clk_f = 0; }
    }
    if (D.a_head == D.a_tail) {
        /* an under-run: the arranger had not written this far (T4) */
        if (pdiff(D.clk_u, D.written) > 0) D.underruns++;
        if (D.b_head != D.b_tail) dispatch_due();
    } else dispatch_due();
    if (D.nwin) {
        int i;
        for (i = 0; i < MUS_WINDOWS; i++)
            if (D.win[i].live && pdiff(D.clk_u, D.win[i].to) >= 0) { D.win[i].live = 0; D.nwin--; }
    }
    if (++D.ctl_t >= D.ctl_mt) {
        D.ctl_t = 0;
        if (D.live || D.master != D.master_sent) frame();
    }
}

/* The bridge: n mt in one call (music_update; nothing pushes meanwhile),
 * exactly as n mus_drv_tick()s. With no TEMPO event pending and the
 * control frame at the end of the batch (ctl_mt == n, the bridge's 8), the
 * clock moves once by n * upm and the due events dispatch in ring order:
 * nothing between them looks at the clock (mute windows compare event
 * positions), so the result is the per-mt one, at a fraction of the cost
 * (GBA: THUMB from ROM, state in EWRAM). Otherwise it steps mt by mt. */
void mus_drv_run(int n)
{
    if (n <= 0) return;
    if (D.fence_on || D.tempo_pending || D.ctl_t != 0 || n != D.ctl_mt) {
        while (n-- > 0) mus_drv_tick();
        return;
    }
    {
        uint32_t f = (uint32_t)D.clk_f + (D.upm & 0xFFFFu) * (uint32_t)n;
        D.clk_u = (D.clk_u + (D.upm >> 16) * (uint32_t)n + (f >> 16)) & MUS_POS_MASK;
        D.clk_f = (uint16_t)f;
    }
    if (D.a_head != D.a_tail || D.b_head != D.b_tail) dispatch_due();
    if (D.nwin) {
        int i;
        for (i = 0; i < MUS_WINDOWS; i++)
            if (D.win[i].live && pdiff(D.clk_u, D.win[i].to) >= 0) { D.win[i].live = 0; D.nwin--; }
    }
    if (D.live || D.master != D.master_sent) frame();
}
