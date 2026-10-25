/* test_music_rep.c - T3, the non-repetition gates, and T18, the proof
 * that the metric catches a loop (owner: MU6; docs/design/music/SPEC.md
 * sec.8.5, CF-37, CF-38; the metric is music_rep_lib.c).
 *
 * T3 runs every adaptive cue of the bank on real events - a private deck
 * driven like music_hash() (CF-33 hash mode), the decision stream of deck
 * A cut into bars:
 *   - every cue with >= 2 nodes, 10 minutes: no identical 8-bar S or
 *     16-bar P window, LRR(P) <= 8;
 *   - hubs (MDF_HUB), 60 minutes, 8 seeds x steady / walk / morph: the
 *     CF-38 gates 1-5.
 * Classic (MDK_LEGACY) cues are exact imports of today's loops and are
 * exempt; T18(a) proves the metric fails them. The 5 worst near-repeat
 * spans of every gated stream are listed with timestamps (CF-38 rule 7).
 *
 * T18 (CF-38 rule 6): the metric must FAIL
 *   (a) today's classic HAHALAND_DAY, gates 1-3;
 *   (b) a hub with GEN off and comp / groove rotation off, drum dice on:
 *       gate 1 or 2;
 *   (c) A-B-A-B with only V1/V2 ornaments: gate 2;
 *   (d) a 4-bar loop varied only in velocity: gate 1;
 * and PASS
 *   (e) the prototype composer (tools/musgen), velocity removed: every
 *       gate, 60 minutes, 8 seeds x 3 scripts x the 4 themes. */
#include <stdlib.h>
#include "test.h"
#include "../src/mus_int.h"
#include "music_rep.h"

/* ---- the engine's deck-A stream, as bars of notes -------------------------------- */
typedef struct EngBar { RepMeta m; int first, n; } EngBar;
typedef struct EngStream {
    RepNote *note;
    EngBar  *bar;
    int      n_note, cap_note, n_bar;
    int      beats, bpm;
} EngStream;

typedef struct RawEv { uint32_t pos, arg; uint8_t kind, pad[3]; } RawEv;
typedef struct RawBuf { RawEv *e; int n, cap; } RawBuf;

static void raw_emit(void *ctx, uint32_t pos_kind, uint32_t arg)
{
    RawBuf *b = (RawBuf *)ctx;
    if (b->n == b->cap) {
        int cap = b->cap ? b->cap * 2 : 4096;
        RawEv *ne = (RawEv *)realloc(b->e, sizeof(RawEv) * (size_t)cap);
        if (!ne) return;
        b->e = ne;
        b->cap = cap;
    }
    b->e[b->n].pos = pos_kind & MUS_POS_MASK;
    b->e[b->n].kind = (uint8_t)(pos_kind >> 28);
    b->e[b->n].arg = arg;
    b->n++;
}

static int role_class(int kind)
{
    switch (kind) {
    case RK_LEAD: return RC_LEAD;
    case RK_COUNTER: return RC_COUNTER;
    case RK_HARM: case RK_ARP: case RK_PAD: case RK_METAL: case RK_CHOIR: return RC_HARM;
    case RK_BASS: return RC_BASS;
    case RK_KIT: case RK_PERC: return RC_KIT;
    default: return RC_N;                          /* beds and FX are not music */
    }
}

static void es_free(EngStream *s) { free(s->note); free(s->bar); memset(s, 0, sizeof(*s)); }

static void es_push(EngStream *s, const RepNote *n)
{
    if (s->n_note == s->cap_note) {
        int cap = s->cap_note ? s->cap_note * 2 : 8192;
        RepNote *nn = (RepNote *)realloc(s->note, sizeof(RepNote) * (size_t)cap);
        if (!nn) return;
        s->note = nn;
        s->cap_note = cap;
    }
    s->note[s->n_note++] = *n;
}

/* `minutes` of cue `cue` from a private deck (hash mode: generation by
 * position, 1 bar ahead per step) into bars of notes. A note lasts until
 * its voice's next note, drum or release. */
static int engine_stream(EngStream *s, int cue, int minutes)
{
    MusGen g;
    MusVoiceRt v[16];
    MusCueView cv;
    MusSink sink;
    RawBuf raw;
    uint32_t end, at = 0, bar_u;
    int i, j, nbars;
    memset(s, 0, sizeof(*s));
    if (!mus_bank_cue(cue, &cv)) return 0;
    memset(&g, 0, sizeof(g));
    memset(&raw, 0, sizeof(raw));
    g.v = v;
    g.max_roles = MUS_A_VOICES;
    mus_gen_start(&g, &cv, 0, mus_rate256(cv.cue->bpm), 0, NULL);
    sink.emit = raw_emit;
    sink.ctx = &raw;
    s->beats = cv.cue->meter ? cv.cue->meter : 4;
    s->bpm = cv.cue->bpm;
    bar_u = (uint32_t)s->beats * MUS_UPQ;
    end = (uint32_t)minutes * cv.cue->bpm * MUS_UPQ;
    while (at < end && g.d.state == MGS_PLAY) {
        uint32_t until = DOOMO_MIN(at + bar_u, end);
        sink.room = 1 << 30;
        mus_gen_until(&g, until, &sink);
        at = until;
    }
    nbars = (int)(end / bar_u);
    s->bar = (EngBar *)calloc((size_t)nbars + 1, sizeof(EngBar));
    if (!s->bar) { free(raw.e); return 0; }
    s->n_bar = nbars;
    for (i = 0; i < nbars; i++) {
        s->bar[i].m.dial = (uint8_t)(cv.cue->dial_home >> 6);
        s->bar[i].m.key = cv.cue->key;
        s->bar[i].m.bpm = cv.cue->bpm;
    }
    /* raw events are in time order: notes, with their lengths, bar by bar */
    for (i = 0; i < raw.n; i++) {
        const RawEv *e = &raw.e[i];
        int voice = MEV_VOICE(e->arg), b = (int)(e->pos / bar_u);
        RepNote n;
        uint32_t stop = e->pos + 2 * bar_u;
        if ((e->kind != MEV_NOTE && e->kind != MEV_DRUM) || b >= nbars || voice >= cv.cue->n_roles) continue;
        for (j = i + 1; j < raw.n && raw.e[j].pos < stop; j++)
            if (MEV_VOICE(raw.e[j].arg) == voice &&
                (raw.e[j].kind == MEV_NOTE || raw.e[j].kind == MEV_DRUM || raw.e[j].kind == MEV_OFF)) {
                stop = raw.e[j].pos;
                break;
            }
        memset(&n, 0, sizeof(n));
        n.at = (uint16_t)(e->pos - (uint32_t)b * bar_u);
        n.len = (uint16_t)DOOMO_MIN(stop - e->pos, 0xFFFFu);
        n.role = (uint8_t)voice;
        n.cls = (uint8_t)role_class(cv.roles[voice].kind);
        if (e->kind == MEV_NOTE) {
            n.pitch = (uint8_t)MEV_NOTE_PITCH(e->arg);
            n.vel = (uint8_t)MEV_NOTE_VEL(e->arg);
        } else {
            n.pitch = (uint8_t)(MEV_DRUM_HIT(e->arg) | MEV_DRUM_KIT(e->arg) << 3);
            n.vel = (uint8_t)MEV_DRUM_VEL(e->arg);
            n.flags = RNF_DRUM | RNF_CORE;            /* literal lane hits */
        }
        if (s->bar[b].n == 0) s->bar[b].first = s->n_note;
        es_push(s, &n);
        s->bar[b].n++;
    }
    free(raw.e);
    return 1;
}

static void stream_to_run(RepRun *r, const EngStream *s, int know)
{
    int i, k;
    rep_init(r, s->beats, s->bpm, know);
    for (i = 0; i < s->n_bar; i++) {
        rep_begin_bar(r, &s->bar[i].m);
        for (k = 0; k < s->bar[i].n; k++) rep_note(r, &s->note[s->bar[i].first + k]);
        rep_end_bar(r);
    }
}

/* a stream built from the bars of `src`: bar i plays src bar map[i] */
static uint32_t xs(uint32_t *x) { *x ^= *x << 13; *x ^= *x >> 17; *x ^= *x << 5; return *x; }

static void print_worst(const char *what, const RepRun *r, const RepStats *st)
{
    int i;
    char line[400];
    int o = snprintf(line, sizeof(line), "    %s: worst near-repeats", what);
    for (i = 0; i < 5 && st->worst[i].len; i++)
        o += snprintf(line + o, sizeof(line) - (size_t)o, "%s %d bars @%ds lag %d", i ? "," : "",
                      st->worst[i].len, rep_seconds(r, st->worst[i].at), st->worst[i].lag);
    if (!st->worst[0].len) snprintf(line + o, sizeof(line) - (size_t)o, " none >= 4 bars");
    fprintf(stderr, "%s\n", line);
}

/* ---- the metric itself ----------------------------------------------------------------- */
TEST(music_t3_lrr_and_similarity_basics)
{
    static uint32_t s[64];
    int i, at = -1, lag = -1;
    for (i = 0; i < 64; i++) s[i] = 1000u + (uint32_t)i;         /* nothing repeats */
    CHECK_EQ(rep_lrr(s, 64, NULL, NULL), 0);
    for (i = 0; i < 64; i++) s[i] = 7u + (uint32_t)(i % 16);      /* a 16-bar loop */
    CHECK_EQ(rep_lrr(s, 64, &at, &lag), 48);
    CHECK_EQ(lag, 16);
    for (i = 0; i < 64; i++) s[i] = (i % 16) == 3 ? 0 : 7u + (uint32_t)(i % 16);  /* rests never match */
    CHECK_EQ(rep_lrr(s, 64, NULL, NULL), 15);
    for (i = 0; i < 64; i++) s[i] = 1000u + (uint32_t)i;
    s[40] = s[10]; s[41] = s[11]; s[42] = s[12];                 /* one 3-bar quote */
    CHECK_EQ(rep_lrr(s, 64, &at, &lag), 3);
    CHECK_EQ(at, 10);
    CHECK_EQ(lag, 30);
    {
        RepRun r;
        RepNote n;
        memset(&n, 0, sizeof(n));
        rep_init(&r, 4, 120, 0);
        for (i = 0; i < 3; i++) {
            rep_begin_bar(&r, NULL);
            n.cls = RC_LEAD; n.role = 0; n.len = 48;
            n.at = 0; n.pitch = 60; n.vel = (uint8_t)(10 + i * 20); rep_note(&r, &n);
            n.at = 96; n.pitch = (uint8_t)(i == 2 ? 67 : 64); rep_note(&r, &n);
            if (i == 1) { n.at = 144; n.len = 6; n.pitch = 62; n.flags = RNF_ORN; rep_note(&r, &n); n.flags = 0; }
            rep_end_bar(&r);
        }
        CHECK(r.b[0].S == r.b[1].S);           /* velocity and the grace note are not variation */
        CHECK(r.b[0].S != r.b[2].S);
        CHECK_EQ(rep_sim(&r, 0, 1), 1000);
        CHECK(rep_sim(&r, 0, 2) < 850);        /* one of two lead pitch classes moved */
        rep_free(&r);
    }
}

TEST(music_t3_prototype_is_the_design_composer)
{
    static const uint32_t fp[4] = { 0x37d71b12u, 0x8d898ef0u, 0xc33cfa26u, 0xbdcb0bd2u };
    int t;
    for (t = 0; t < 4; t++) CHECK_EQ_U(rep_proto_fingerprint(t), fp[t]);
}

/* ---- T18: the metric fails on known loops ------------------------------------------------- */
TEST(music_t18a_classic_hahaland_fails)
{
    EngStream s;
    RepRun r;
    RepStats st;
    char why[300];
    int f;
    CHECK(engine_stream(&s, MC_CLASSIC_HAHALAND_DAY, 60));
    stream_to_run(&r, &s, 0);
    rep_measure(&r, &st);
    f = rep_hub_gates(&r, &st, 1, why, sizeof(why));
    fprintf(stderr, "    T18(a) classic HAHALAND_DAY, 60 min: LRR S %d P %d L %d, near-loop %d, recency %d.%d %%\n",
            st.lrr_s, st.lrr_p, st.lrr_l, st.near_run, st.recency_pm / 10, st.recency_pm % 10);
    CHECK_MSG((f & (1 << 1)) && (f & (1 << 2)) && (f & (1 << 3)), "gates failed 0x%x (want 1-3)", f);
    CHECK(st.lrr_s >= st.bars - 32);           /* today: one 16-bar loop */
    rep_free(&r);
    es_free(&s);
}

/* (b): a 4-node hub, nodes = the 4 x 8 bars of classic FESTIVAL, walked
 * at random (no A -> A), every node literal (no GEN, no comp or groove
 * rotation) - but the drums roll dice every bar: hats, ghosts, accents */
TEST(music_t18b_static_hub_with_drum_dice_fails)
{
    EngStream s;
    RepRun r;
    RepStats st;
    uint32_t x = 0x9E3779B9u;
    int i, k, node = 0, bars = 60 * 144 / 4, f;
    CHECK(engine_stream(&s, MC_CLASSIC_FESTIVAL, 1));
    CHECK(s.n_bar >= 32);
    rep_init(&r, s.beats, s.bpm, 0);
    for (i = 0; i < bars; i++) {
        const EngBar *src;
        RepNote n;
        if (i % 8 == 0 && i) { int nx = (int)(xs(&x) % 3); node = nx >= node ? nx + 1 : nx; }
        src = &s.bar[node * 8 + i % 8];
        rep_begin_bar(&r, &src->m);
        for (k = 0; k < src->n; k++) {
            n = s.note[src->first + k];
            n.vel = (uint8_t)(xs(&x) & 63);
            rep_note(&r, &n);
        }
        memset(&n, 0, sizeof(n));               /* the dice: p < 15 steps and ghosts */
        n.cls = RC_KIT; n.role = 3; n.flags = RNF_DRUM; n.len = 12;
        for (k = 0; k < 16; k++)
            if (xs(&x) % 15 < 6) { n.at = (uint16_t)(k * 12); n.pitch = (uint8_t)(k & 1 ? KH_HAT : KH_SNARE); rep_note(&r, &n); }
        rep_end_bar(&r);
    }
    rep_measure(&r, &st);
    f = rep_hub_gates(&r, &st, 1, NULL, 0);
    fprintf(stderr, "    T18(b) static hub + drum dice, 60 min: LRR S %d P %d, near-loop %d\n",
            st.lrr_s, st.lrr_p, st.near_run);
    CHECK_MSG(f & ((1 << 1) | (1 << 2)), "gates failed 0x%x (want 1 or 2)", f);
    rep_free(&r);
    es_free(&s);
}

/* (c): A-B-A-B (classic HAHALAND_DAY's two nodes) where every pass plays
 * V1/V2 ornaments on the lead: grace notes, mordents, slides and vibrato
 * (ornament-blind / surface), octave displacements, anticipations and
 * laybacks, dotted swaps and passing notes - at the V2 chance of 35 % */
TEST(music_t18c_ornamented_abab_fails_gate_2)
{
    EngStream s;
    RepRun r;
    RepStats st;
    uint32_t x = 0x2545F491u;
    int i, k, bars = 60 * 124 / 4, f, exact;
    CHECK(engine_stream(&s, MC_CLASSIC_HAHALAND_DAY, 1));
    CHECK(s.n_bar >= 16);
    rep_init(&r, s.beats, s.bpm, 0);
    for (i = 0; i < bars; i++) {
        const EngBar *src = &s.bar[i % 16];
        RepNote buf[128];
        int nb = 0, lead[64], nl = 0, op = xs(&x) % 100 < 35 ? (int)(xs(&x) % 9) : -1;
        for (k = 0; k < src->n && nb < 120; k++) {
            buf[nb] = s.note[src->first + k];
            if (buf[nb].cls == RC_LEAD && !(buf[nb].flags & RNF_DRUM) && nl < 64) lead[nl++] = nb;
            nb++;
        }
        if (nl >= 2 && op >= 0) {
            int a = lead[xs(&x) % (uint32_t)(nl - 1)], b = a + 1;
            RepNote *na = &buf[a];
            for (b = a + 1; b < nb && !(buf[b].cls == RC_LEAD && !(buf[b].flags & RNF_DRUM)); b++) {}
            switch (op) {
            case 0: case 1:                                   /* GRACE / MORDENT */
                buf[nb] = *na; buf[nb].len = 6; buf[nb].pitch = (uint8_t)(na->pitch + 2); buf[nb].flags = RNF_ORN; nb++;
                break;
            case 2: case 3:                                   /* SLIDE_IN / VIB */
                buf[nb] = *na; buf[nb].flags = RNF_SURF; nb++;
                break;
            case 4: na->pitch = (uint8_t)(na->pitch + (na->pitch < 72 ? 12 : -12)); break;   /* OCTAVE */
            case 5: if (na->at >= 12) { na->at = (uint16_t)(na->at - 12); na->len = (uint16_t)(na->len + 12); } break;
            case 6: if (na->len > 12) { na->at = (uint16_t)(na->at + 12); na->len = (uint16_t)(na->len - 12); } break;
            case 7:                                           /* DOTSWAP */
                if (b < nb && na->len >= 24) { int d = na->len / 2; na->len = (uint16_t)(na->len - d); buf[b].at = (uint16_t)(buf[b].at - d); buf[b].len = (uint16_t)(buf[b].len + d); }
                break;
            default:                                          /* PASSING */
                if (na->len >= 24) {
                    buf[nb] = *na; buf[nb].at = (uint16_t)(na->at + na->len / 2); buf[nb].len = (uint16_t)(na->len / 2);
                    buf[nb].pitch = (uint8_t)(na->pitch + 2); na->len = (uint16_t)(na->len / 2); nb++;
                }
                break;
            }
        }
        rep_begin_bar(&r, &src->m);
        for (k = 0; k < nb; k++) rep_note(&r, &buf[k]);
        rep_end_bar(&r);
    }
    rep_measure(&r, &st);
    f = rep_hub_gates(&r, &st, 1, NULL, 0);
    exact = st.lrr_s;
    fprintf(stderr, "    T18(c) ornamented A-B-A-B, 60 min: LRR S %d (exact) , near-loop %d bars at lag %d\n",
            exact, st.near_run, st.near_lag);
    CHECK_MSG(f & (1 << 2), "gate 2 passed an ornamented A-B-A-B (near-loop %d)", st.near_run);
    rep_free(&r);
    es_free(&s);
}

/* (d): a 4-bar loop whose velocities are random: the old signature
 * (velocity hashed, CF-37) calls it fresh, the new one fails gate 1 */
TEST(music_t18d_velocity_only_loop_fails_gate_1)
{
    EngStream s;
    RepRun r;
    RepStats st;
    uint32_t x = 0xB5297A4Du, *old;
    int i, k, bars = 60 * 124 / 4, f, old_lrr;
    CHECK(engine_stream(&s, MC_CLASSIC_HAHALAND_DAY, 1));
    old = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)bars);
    CHECK(old != NULL);
    rep_init(&r, s.beats, s.bpm, 0);
    for (i = 0; i < bars; i++) {
        const EngBar *src = &s.bar[i % 4];
        uint32_t h = 0x811C9DC5u;
        rep_begin_bar(&r, &src->m);
        for (k = 0; k < src->n; k++) {
            RepNote n = s.note[src->first + k];
            n.vel = (uint8_t)(xs(&x) & 63);
            h = (h ^ ((uint32_t)n.at | (uint32_t)n.pitch << 16 | (uint32_t)(n.vel >> 2) << 24)) * 0x01000193u;
            rep_note(&r, &n);
        }
        old[i] = h | 1u;
        rep_end_bar(&r);
    }
    rep_measure(&r, &st);
    f = rep_hub_gates(&r, &st, 1, NULL, 0);
    old_lrr = rep_lrr(old, bars, NULL, NULL);
    fprintf(stderr, "    T18(d) 4-bar loop, velocity only: LRR S %d (old velocity signature: %d)\n", st.lrr_s, old_lrr);
    CHECK_MSG(f & (1 << 1), "gate 1 passed a velocity-only loop (LRR S %d)", st.lrr_s);
    CHECK(old_lrr <= 4);                       /* the hole CF-37 closed */
    free(old);
    rep_free(&r);
    es_free(&s);
}

/* (e): the prototype composer passes every gate, velocity removed */
TEST(music_t18e_prototype_passes_every_gate)
{
    static const char *const scr[RS_COUNT] = { "steady", "walk", "morph" };
    int t, sc, seed, fails = 0;
    char first[400] = "";
    for (t = 0; t < 4; t++)
        for (sc = 0; sc < RS_COUNT; sc++) {
            int w_s = 0, w_p = 0, w_l = 0, w_near = 0, w_rec = 1000, w_tex = 1000, w_min10 = 1 << 20, w_head = 0;
            int cad_n = 0, cad_ok = 0;
            for (seed = 1; seed <= 8; seed++) {
                RepRun r;
                RepStats st;
                char why[300];
                int f;
                rep_proto_run(&r, t, 0x1234u * (uint32_t)seed + 7u, 60, sc);
                rep_measure(&r, &st);
                f = rep_hub_gates(&r, &st, sc == RS_STEADY, why, sizeof(why));
                if (f) {
                    fprintf(stderr, "    T18(e) %s %s seed %d: %s\n", rep_proto_name(t), scr[sc], seed, why);
                    if (!fails++) snprintf(first, sizeof(first), "%s %s seed %d: %s", rep_proto_name(t), scr[sc], seed, why);
                }
                if (seed == 1 && sc == RS_WALK) print_worst(rep_proto_name(t), &r, &st);
                w_s = DOOMO_MAX(w_s, st.lrr_s); w_p = DOOMO_MAX(w_p, st.lrr_p); w_l = DOOMO_MAX(w_l, st.lrr_l);
                w_near = DOOMO_MAX(w_near, st.near_run); w_rec = DOOMO_MIN(w_rec, st.recency_pm);
                w_tex = DOOMO_MIN(w_tex, st.texture_pm); w_min10 = DOOMO_MIN(w_min10, st.texture_min10);
                w_head = DOOMO_MAX(w_head, st.head_gap);
                cad_n += st.cad_n; cad_ok += st.cad_ok;
                rep_free(&r);
            }
            fprintf(stderr, "    T18(e) %-16s %-6s 60 min x 8: LRR S %d P %d L %d, near %d, recency %d.%d %%, "
                    "texture %d.%d %% (%d / 10 min), head gap %d, cadences %d %%\n",
                    rep_proto_name(t), scr[sc], w_s, w_p, w_l, w_near, w_rec / 10, w_rec % 10, w_tex / 10,
                    w_tex % 10, w_min10, w_head, cad_n ? cad_ok * 100 / cad_n : 100);
        }
    /* CF-37: the old full-mix figure was velocity noise */
    for (t = 0; t < 4; t++) {
        RepRun r;
        RepStats st;
        rep_proto_run(&r, t, 0x1234u + 7u, 60, RS_STEADY);
        rep_measure(&r, &st);
        fprintf(stderr, "    CF-37 %-16s steady 60 min: full-mix LRR %d bars with velocity, %d without\n",
                rep_proto_name(t), rep_proto_old_lrr(t, 0x1234u + 7u, 60, RS_STEADY), st.lrr_s);
        rep_free(&r);
    }
    CHECK_MSG(!fails, "the prototype must pass every gate; %d of 96 streams fail, first: %s", fails, first);
}

/* ---- T3: every adaptive cue of the bank -------------------------------------------------- */
TEST(music_t3_every_adaptive_cue_passes)
{
    const MusBank *bank = mus_bank();
    const MusDirEnt *dir = (const MusDirEnt *)(const void *)(doomo_mus_bank + bank->dir_off);
    int i, gated = 0, classic = 0;
    for (i = 0; i < bank->n_cues; i++) {
        MusCueView cv;
        EngStream s;
        RepRun r;
        RepStats st;
        char why[300];
        int kind = MUS_DIR_KIND(&dir[i]), hub = (MUS_DIR_FLAGS(&dir[i]) & MDF_HUB) != 0;
        if (kind == MDK_LEGACY) { classic++; continue; }        /* exact imports: T18(a) */
        if (kind != MDK_CUE || !mus_bank_cue(dir[i].id, &cv) || cv.cue->n_nodes < 2) continue;
        CHECK(engine_stream(&s, dir[i].id, hub ? 60 : 10));
        stream_to_run(&r, &s, 0);
        rep_measure(&r, &st);
        CHECK_MSG(!rep_cue_gates(&st, why, sizeof(why)), "cue %d: %s", dir[i].id, why);
        if (hub) CHECK_MSG(!rep_hub_gates(&r, &st, 1, why, sizeof(why)), "hub %d: %s", dir[i].id, why);
        print_worst("cue", &r, &st);
        gated++;
        rep_free(&r);
        es_free(&s);
    }
    CHECK(classic >= 28);
    fprintf(stderr, "    T3: %d adaptive cues gated (%d classic cues exempt)\n", gated, classic);
}
