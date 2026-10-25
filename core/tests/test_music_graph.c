/* test_music_graph.c - MU1 M4: the walk, GEN bags, chapters and lifts,
 * the transition matrix, session resume, quotes, the twin, beds, the
 * adaptive hash and seam legality (owner: MU6; docs/design/music/SPEC.md
 * sec.6.3, 6.5, 8.3, 8.4, CF-20). Played on the test bank
 * (core/tests/music) through mus_bank_use().
 *
 * The walk tests run a deck copy (a local MusGen, as music_hash does): the
 * walk reads only the deck, the committed intensity and the params, so the
 * copy walks exactly as deck A would (CF-20), without the driver. */
#include "test.h"
#include "../src/audio_int.h"
#include "../src/mus_int.h"
#include "data/mus_testbank.h"

/* ---- a deck copy ---------------------------------------------------------------- */
typedef struct Pick { uint16_t picks; uint8_t node, var; int8_t ktrans; uint8_t twin, dial, pad; } Pick;

static void drop(void *ctx, uint32_t pk, uint32_t arg) { (void)pk; (void)arg; ++*(uint32_t *)ctx; }

/* walk cue view `cv` at intensity I for n node entries (the entry node first) */
static int walk_cv(const MusCueView *cv, int I, uint32_t seed, Pick *out, int n)
{
    static MusGen g;
    static MusVoiceRt v[MUS_A_ROLES];
    MusSink s;
    uint32_t at = 0, cnt = 0;
    int k = 0, last = -1, guard = 0, r;
    uint8_t keep = mus_intensity_now;
    memset(&g, 0, sizeof(g));
    g.v = v;
    g.max_roles = MUS_A_ROLES;
    mus_intensity_now = (uint8_t)I;
    mus_gen_start(&g, cv, 0, mus_rate256(cv->cue->bpm), 0, NULL);
    g.d.seed = seed;
    mus_gen_enter(&g, cv->cue->entry, 0, -1, 0xFF);          /* re-deal the entry with this seed */
    s.emit = drop;
    s.ctx = &cnt;
    while (k < n && g.d.state == MGS_PLAY && guard++ < n * 64) {
        if ((int)g.d.picks != last) {
            last = g.d.picks;
            out[k].picks = g.d.picks;
            out[k].node = g.d.node;
            out[k].var = g.var;
            out[k].ktrans = g.ktrans;
            out[k].dial = g.d.dial;
            out[k].twin = 0;
            for (r = 0; r < g.n_roles; r++) if (g.v[r].flags & MVF_TWIN) out[k].twin = 1;
            k++;
        }
        s.room = 1 << 30;
        at += 48;                                           /* a beat per step: < 1 pick */
        mus_gen_until(&g, at & MUS_POS_MASK, &s);
    }
    mus_intensity_now = keep;
    return k;
}

static int walk(int cue, int I, uint32_t seed, Pick *out, int n)
{
    MusCueView cv;
    return mus_bank_cue(cue, &cv) ? walk_cv(&cv, I, seed, out, n) : 0;
}

static int var_family(int cue, int node, int var)
{
    MusCueView cv;
    const uint8_t *pack, *dir;
    if (!mus_bank_cue(cue, &cv)) return -1;
    pack = cv.blob + cv.nodes[node].pack_off;
    if (!(pack[1] & MNP_VARIANTS) || var >= 8) return -1;
    dir = pack + mus_rd16(pack + 2 + 2 * pack[0]);
    return pack[mus_rd16(dir + 4 + 2 * var)] >> 4;
}

enum { N_A, N_B, N_C, N_BRK };          /* the test hub's nodes */

static void use_test_bank(void)
{
    test_reset();
    mus_bank_use(mus_testbank);
    audio_init();
    mus_core_init();
}

static void use_game_bank(void)
{
    mus_drv_trace = NULL;
    mus_bank_use(NULL);
    mus_core_init();
}

/* rules 1 and 4, the intensity windows and the conditional edge */
TEST(music_m4_the_hub_walks_its_graph)
{
    static Pick p[400];
    static const int levels[3] = { 230, 102, 40 };
    int L, s;
    use_test_bank();
    for (L = 0; L < 3; L++)
        for (s = 0; s < 4; s++) {
            int n = walk(MCT_HUB, levels[L], 0x1234u + (uint32_t)s * 77u, p, 400), i, seen[4] = { 0, 0, 0, 0 };
            CHECK_EQ(n, 400);
            CHECK_EQ(p[0].node, N_A);
            for (i = 0; i < n; i++) {
                seen[p[i].node]++;
                if (i) CHECK_MSG(p[i].node != p[i - 1].node, "I %d seed %d: %d -> %d at pick %d", levels[L], s,
                                 p[i - 1].node, p[i].node, i);
                if (i >= 2)
                    CHECK_MSG(p[i].node == N_A || p[i - 1].node == N_A || p[i - 2].node == N_A,
                              "I %d: three non-anchors in a row at pick %d", levels[L], i);
            }
            CHECK(seen[N_A] > 0 && seen[N_B] > 0);
            if (levels[L] >= 102) CHECK_MSG(seen[N_C] > 0 && seen[N_BRK] == 0, "I %d: C %d BRK %d", levels[L], seen[N_C],
                                            seen[N_BRK]);
            else CHECK_MSG(seen[N_C] == 0 && seen[N_BRK] > 0, "I %d: C %d BRK %d", levels[L], seen[N_C], seen[N_BRK]);
        }
    use_game_bank();
}

/* the GEN bag: every epoch of n passes is a permutation, the boundary
 * never repeats, and a node's passes never repeat a variant */
TEST(music_m4_gen_bags_deal_every_variant)
{
    uint32_t seed;
    int n, node;
    for (seed = 1; seed < 40; seed++)
        for (node = 0; node < 4; node++)
            for (n = 2; n <= 8; n++) {
                int pass, prev = -1;
                for (pass = 0; pass < 8 * n; pass++) {
                    int v = mus_gen_variant(seed * 2654435761u, node, pass, n);
                    CHECK(v >= 0 && v < n);
                    CHECK_MSG(v != prev, "seed %u node %d n %d: pass %d repeats %d", (unsigned)seed, node, n, pass, v);
                    prev = v;
                    if (pass % n == n - 1) {               /* the epoch just closed: a permutation */
                        int k, mask = 0;
                        for (k = pass - n + 1; k <= pass; k++)
                            mask |= 1 << mus_gen_variant(seed * 2654435761u, node, k, n);
                        CHECK_EQ(mask, (1 << n) - 1);
                    }
                }
            }
}

/* the hub's GEN slot plays its variants: the family of the chapter, never
 * the same variant twice running, all four over a long walk */
TEST(music_m4_gen_nodes_play_the_chapter_family)
{
    static Pick p[600];
    int s;
    use_test_bank();
    for (s = 0; s < 4; s++) {
        uint32_t seed = 0xC0FFEEu + (uint32_t)s;
        int n = walk(MCT_HUB, 230, seed, p, 600), i, prev = -1, seen = 0, visits = 0;
        CHECK_EQ(n, 600);
        for (i = 0; i < n; i++) {
            int fam;
            if (p[i].node != N_C) continue;
            visits++;
            CHECK_MSG(p[i].var < 4, "C entered with variant %d", p[i].var);
            fam = var_family(MCT_HUB, N_C, p[i].var);
            CHECK_MSG(fam == mus_pool_value(seed, N_C, p[i].picks / 5, 2), "pick %d: variant %d of family %d, chapter %d",
                      p[i].picks, p[i].var, fam, p[i].picks / 5);
            CHECK_MSG(p[i].var != prev, "pick %d: variant %d twice running", p[i].picks, p[i].var);
            prev = p[i].var;
            seen |= 1 << p[i].var;
        }
        CHECK_MSG(visits > 50 && seen == 15, "seed %d: %d visits, variants seen %x", s, visits, seen);
    }
    use_game_bank();
}

/* chapters: picks / 5; B's lift edge (lift=1) is live from chapter 2, lifts
 * the key by a semitone at most twice, and the key comes home at the next
 * chapter (where the chapter's own lift may take it up again) */
TEST(music_m4_chapters_lift_the_key_and_bring_it_home)
{
    static Pick p[400];
    int s, lifted_runs = 0;
    use_test_bank();
    for (s = 0; s < 16; s++) {
        int n = walk(MCT_HUB, 230, 0xBEEF00u + (uint32_t)s, p, 400), i, lifts = 0;
        CHECK_EQ(n, 400);
        CHECK_EQ(p[0].ktrans, 0);
        for (i = 1; i < n; i++) {
            CHECK(p[i].ktrans == 0 || p[i].ktrans == 1);
            if (!p[i].ktrans) continue;
            CHECK_MSG(p[i].picks / 5 >= 2, "a lift in chapter %d", p[i].picks / 5);
            if (!p[i - 1].ktrans || p[i].picks / 5 != p[i - 1].picks / 5) {      /* a lift */
                lifts++;
                CHECK_MSG(p[i].node == N_A && p[i - 1].node == N_B, "lifted on %d -> %d at pick %d", p[i - 1].node,
                          p[i].node, p[i].picks);
            }
        }
        CHECK_MSG(lifts <= 2, "seed %d: %d lifts", s, lifts);
        lifted_runs += lifts > 0;
    }
    CHECK_MSG(lifted_runs >= 8, "only %d of 16 walks lifted", lifted_runs);
    use_game_bank();
}

/* the twin: a diatonic 3rd below in the key; COUNTER doubles the lead at
 * dial >= S3 (MP_HELL drives the dial towards the cue's range top) */
TEST(music_m4_the_twin_is_a_third_below)
{
    static Pick p[40];
    int n, i, twin_lo = 0, twin_hi = 0;
    CHECK_EQ(mus_twin_pitch(0, 64), 60);              /* C major: E -> C */
    CHECK_EQ(mus_twin_pitch(0, 67), 64);              /*          G -> E */
    CHECK_EQ(mus_twin_pitch(0, 60), 57);              /*          C -> A */
    CHECK_EQ(mus_twin_pitch(0, 71), 67);              /*          B -> G */
    CHECK_EQ(mus_twin_pitch(9 | MMODE_MINOR << 4, 72), 69);   /* A minor: C -> A */
    CHECK_EQ(mus_twin_pitch(9 | MMODE_MINOR << 4, 69), 65);   /*          A -> F */
    use_test_bank();
    n = walk(MCT_HUB, 230, 7, p, 40);
    for (i = 0; i < n; i++) twin_lo += p[i].twin;
    music_set_param_now(MP_HELL, 255);
    n = walk(MCT_HUB, 230, 7, p, 40);
    for (i = 2; i < n; i++) { twin_hi += p[i].twin; CHECK(p[i].dial >= 192); }
    music_set_param_now(MP_HELL, 0);
    CHECK_EQ(twin_lo, 0);
    CHECK_MSG(twin_hi >= n - 3, "twin on %d of %d entries at S4", twin_hi, n);
    use_game_bank();
}

/* ---- the engine with the driver ---------------------------------------------------- */
typedef struct TrEv { uint32_t pos, arg; uint8_t kind, ring, pad[2]; } TrEv;
static TrEv tr[8192];
static int tr_n;

static void tr_hook(const DoomoMusEv *e, int ring)
{
    if (tr_n >= (int)(sizeof(tr) / sizeof(tr[0]))) return;
    tr[tr_n].pos = MEV_POS(e);
    tr[tr_n].arg = e->arg;
    tr[tr_n].kind = (uint8_t)MEV_KIND(e);
    tr[tr_n].ring = (uint8_t)ring;
    tr_n++;
}

static void on_bank(const uint8_t *bank)
{
    test_reset();
    mus_bank_use(bank);
    audio_init();
    mus_core_init();
    tr_n = 0;
    mus_drv_trace = tr_hook;
}

static void run(int n) { while (n-- > 0) music_update(); }

static int run_until_cue(int cue, int max)
{
    int i;
    for (i = 0; i < max; i++) {
        MusBeatEv ev[8];
        int n, k;
        music_update();
        n = music_beat_events(ev, 8);
        for (k = 0; k < n; k++) if ((ev[k].kind & MB_CUE) && ev[k].cue == cue) return i;
    }
    return -1;
}

/* the matrix: HUB -> TWO is FILL:1 on the bar (a snare run into the crash on
 * the bar line, TWO starts there), TWO -> HUB is VINYL:NEEDLE on the beat
 * (the needle lands on TWO's beat, the hub starts when it ends) */
TEST(music_m4_the_matrix_fills_on_the_bar_and_needles_back)
{
    uint32_t hub0, crash = 0, needle = 0;
    int i, snares = 0, crashes = 0, starts = 0;
    on_bank(mus_testbank);
    music_set_cue(MCT_HUB, MQ_NOW);
    hub0 = mus_drv_clock();
    run(150);
    tr_n = 0;
    music_set_cue(MCT_TWO, MQ_BAR);
    music_set_cue(MCT_TWO, MQ_BAR);                     /* armed: a no-op */
    CHECK_MSG(run_until_cue(MCT_TWO, 400) >= 0, "TWO never started");
    for (i = 0; i < tr_n; i++) {
        if (tr[i].ring != 1 || tr[i].kind != MEV_DRUM) continue;
        if (MEV_DRUM_HIT(tr[i].arg) == KH_CRASH) { crashes++; crash = tr[i].pos; }
        else if (MEV_DRUM_HIT(tr[i].arg) == KH_SNARE || MEV_DRUM_HIT(tr[i].arg) == KH_TOM) snares++;
    }
    CHECK_EQ(crashes, 1);
    CHECK_MSG(snares >= 3, "%d fill hits", snares);
    CHECK_MSG(((crash - hub0) & MUS_POS_MASK) % (4 * MUS_UPQ) == 0, "the cut at %u, the hub's bars from %u",
              (unsigned)crash, (unsigned)hub0);
    CHECK_EQ(music_cue(), MCT_TWO);
    run(200);
    tr_n = 0;
    music_set_cue(MCT_HUB, MQ_BEAT);
    CHECK_MSG(run_until_cue(MCT_HUB, 600) >= 0, "the hub never came back");
    for (i = 0; i < tr_n; i++)
        if (tr[i].kind == MEV_FX && (tr[i].arg & 15u) == MFX_VINYL_START) { needle = tr[i].pos; starts++; }
    CHECK_EQ(starts, 1);
    CHECK_MSG(((needle - crash) & MUS_POS_MASK) % MUS_UPQ == 0, "the needle at %u, TWO's beats from %u",
              (unsigned)needle, (unsigned)crash);
    CHECK_EQ(music_cue(), MCT_HUB);
    mus_drv_trace = NULL;
    use_game_bank();
}

/* the classic calls still CUT now (audio_music -> mus_core_start) */
TEST(music_m4_classic_songs_still_cut_now)
{
    on_bank(NULL);
    audio_music(SONG_HAHALAND_DAY);
    run(100);
    audio_music(SONG_TITLE);
    CHECK_EQ(music_cue(), mus_bind_cue(SONG_TITLE, -1, -1, 0));
    run(1);
    CHECK_EQ(music_cue(), mus_bind_cue(SONG_TITLE, -1, -1, 0));
    use_game_bank();
}

/* session memory: music_resume() picks the walk up where it was left
 * (picks, history, passes) at the cue's re-entry; music_set_cue() starts
 * it fresh */
TEST(music_m4_resume_returns_to_the_walk)
{
    uint16_t picks;
    uint8_t passes[32];
    const MusGen *a;
    int i, more = 0;
    on_bank(mus_testbank);
    music_set_cue(MCT_HUB, MQ_NOW);
    run(1500);
    music_set_cue(MCT_TWO, MQ_NOW);
    CHECK(run_until_cue(MCT_TWO, 400) >= 0);
    run(200);
    music_resume(MCT_HUB);
    CHECK(run_until_cue(MCT_HUB, 600) >= 0);
    a = mus_core_deck(0);
    picks = a->d.picks;
    memcpy(passes, a->d.passes, 32);
    CHECK_MSG(picks >= 8, "resumed at %u picks", (unsigned)picks);
    for (i = 0; i < 32; i++) more += passes[i];
    CHECK(more >= 8);
    music_set_cue(MCT_TWO, MQ_NOW);
    CHECK(run_until_cue(MCT_TWO, 400) >= 0);
    music_set_cue(MCT_HUB, MQ_BEAT);
    CHECK(run_until_cue(MCT_HUB, 600) >= 0);
    CHECK_MSG(mus_core_deck(0)->d.picks <= 1, "a fresh start at %u picks", (unsigned)mus_core_deck(0)->d.picks);
    mus_drv_trace = NULL;
    use_game_bank();
}

/* quotes: the motif's head on the QUOTE role's voice at a bar line, in the
 * cue's key (HOOK = E E G E in C); cooldowns: 4 bars, the same motif 16 */
TEST(music_m4_quotes_play_the_motif_in_key)
{
    static const int want[4] = { 64, 64, 67, 64 };
    uint32_t hub0, q0 = 0, q1 = 0;
    int i, got = 0, j;
    on_bank(mus_testbank);
    music_set_cue(MCT_HUB, MQ_NOW);
    hub0 = mus_drv_clock();
    run(200);
    tr_n = 0;
    music_quote(MOTIF_HOOK, 1);
    run(300);
    for (i = 0; i < tr_n && got < 4; i++) {
        if (tr[i].ring != 1 || tr[i].kind != MEV_NOTE || MEV_VOICE(tr[i].arg) != 0) continue;
        if (!got) q0 = tr[i].pos;
        CHECK_MSG(MEV_NOTE_PITCH(tr[i].arg) == want[got], "quote note %d: %d, want %d", got,
                  MEV_NOTE_PITCH(tr[i].arg), want[got]);
        got++;
    }
    CHECK_EQ(got, 4);
    CHECK_MSG(((q0 - hub0) & MUS_POS_MASK) % (4 * MUS_UPQ) == 0, "the quote at %u", (unsigned)q0);
    /* another motif waits >= 4 bars after the first */
    tr_n = 0;
    music_quote(MOTIF_TITLE, 1);
    run(600);
    for (j = 0; j < tr_n; j++)
        if (tr[j].ring == 1 && tr[j].kind == MEV_INST && MEV_VOICE(tr[j].arg) == 0 &&
            mus_pdiff(tr[j].pos, q0) > 0) { q1 = tr[j].pos; break; }
    CHECK_MSG(q1 && mus_pdiff(q1, q0) >= 4 * 4 * (int)MUS_UPQ, "the second quote %d units after the first",
              (int)mus_pdiff(q1, q0));
    mus_drv_trace = NULL;
    use_game_bank();
}

/* beds: seeded emitters on ring B while nothing plays; none while the
 * music sounds; none without a mask; the same seed, the same beds */
static int bed_hits(int inst)
{
    int i, n = 0;
    for (i = 0; i < tr_n; i++)
        if (tr[i].ring == 1 && tr[i].kind == MEV_INST && MEV_INST_INST(tr[i].arg) == inst) n++;
    return n;
}

TEST(music_m4_beds_play_while_the_music_is_silent)
{
    static TrEv first[512];
    int n1, i;
    on_bank(NULL);
    music_beds(1u << MBED_RAIN);
    run(300);
    n1 = bed_hits(MUS_INST_BED0 + 2);
    CHECK_MSG(n1 >= 40, "%d rain drops in 10 s", n1);
    memcpy(first, tr, sizeof(first));
    music_beds(0);
    run(2);                                             /* a drop already written plays */
    tr_n = 0;
    run(300);
    CHECK_EQ(bed_hits(MUS_INST_BED0 + 2), 0);
    /* the music sounds: no beds */
    music_beds(1u << MBED_RAIN);
    audio_music(SONG_HAHALAND_DAY);
    run(30);
    tr_n = 0;
    run(300);
    CHECK_EQ(bed_hits(MUS_INST_BED0 + 2), 0);
    /* the same again from scratch: the same drops */
    mus_drv_trace = NULL;
    on_bank(NULL);
    music_beds(1u << MBED_RAIN);
    run(300);
    CHECK_EQ(bed_hits(MUS_INST_BED0 + 2), n1);
    for (i = 0; i < 512 && i < tr_n; i++)
        CHECK_MSG(tr[i].pos == first[i].pos && tr[i].arg == first[i].arg, "bed event %d differs", i);
    music_beds(0);
    mus_drv_trace = NULL;
    use_game_bank();
}

/* music_hash on an adaptive cue: stable, cue-specific and pure (playback
 * around the call is the playback without it) */
TEST(music_m4_the_adaptive_hash_is_pure)
{
    static int16_t ref[300][MUS_A_VOICES];
    uint32_t h1, h2;
    int i, v, diff = 0;
    use_test_bank();
    h1 = music_hash(MCT_HUB, 2);
    h2 = music_hash(MCT_HUB, 2);
    CHECK(h1 != 0);
    CHECK_EQ_U(h1, h2);
    CHECK(music_hash(MCT_TWO, 2) != h1);
    music_set_cue(MCT_HUB, MQ_NOW);
    for (i = 0; i < 300; i++) {
        music_update();
        for (v = 0; v < MUS_A_VOICES; v++) ref[i][v] = mus_drv_voice(v)->pitch16;
    }
    use_test_bank();
    music_set_cue(MCT_HUB, MQ_NOW);
    for (i = 0; i < 300; i++) {
        if (i == 120) CHECK_EQ_U(music_hash(MCT_HUB, 2), h1);
        music_update();
        for (v = 0; v < MUS_A_VOICES; v++) diff += mus_drv_voice(v)->pitch16 != ref[i][v];
    }
    CHECK_EQ(diff, 0);
    use_game_bank();
}

/* walk rule 5: the seam chord_out -> chord_in in the dial band's family */
TEST(music_m4_seams_follow_the_cadence_tables)
{
    static MusGen g;
    static MusNode nodes[2];
    memset(&g, 0, sizeof(g));
    g.cv.nodes = nodes;
    nodes[0].chord_out = (uint8_t)(7 | MCH_DOM7 << 4);  /* G7 */
    nodes[1].chord_in = (uint8_t)(0 | MCH_MAJ << 4);    /* -> C: V7-I, legal everywhere */
    g.d.dial = 64;  CHECK(mus_seam_ok(&g, 0, 1));
    g.d.dial = 255; CHECK(mus_seam_ok(&g, 0, 1));
    nodes[1].chord_in = (uint8_t)(4 | MCH_MAJ << 4);    /* -> E: never */
    g.d.dial = 64;  CHECK(!mus_seam_ok(&g, 0, 1));
    g.d.dial = 160; CHECK(!mus_seam_ok(&g, 0, 1));
    g.d.dial = 255; CHECK(!mus_seam_ok(&g, 0, 1));
    nodes[1].chord_in = (uint8_t)(2 | MCH_MAJ << 4);    /* -> D: metal only */
    g.d.dial = 64;  CHECK(!mus_seam_ok(&g, 0, 1));
    g.d.dial = 255; CHECK(mus_seam_ok(&g, 0, 1));
    nodes[1].chord_in = MCH_NC;                          /* N.C. passes */
    g.d.dial = 64;  CHECK(mus_seam_ok(&g, 0, 1));
}

/* the walk keeps rule 5 while it can and counts the seams it must break:
 * a hand-built cue X (anchor, ends on G7) -> Y (starts on E, illegal) w 200
 * or Z (starts on C) w 1; then X -> Y alone */
TEST(music_m4_the_walk_keeps_its_seams)
{
    static Pick p[200];
    static uint8_t blob[4] = { 0, MNP_NO_CHART, 0, 0 };
    static MusNode nodes[3];
    static MusEdge edges[4];
    static MusCue c;
    MusCueView cv;
    const MusBank *b;
    const MusDirEnt *dir;
    int i, n, e, y = 0, I;
    memset(&cv, 0, sizeof(cv));
    memset(&c, 0, sizeof(c));
    c.n_nodes = 3; c.n_edges = 4; c.bpm = 120; c.meter = 4;
    c.dial_home = c.dial_lo = c.dial_hi = 64; c.tag = 2; c.flags = MCF_LOOP;
    for (i = 0; i < 3; i++) {
        nodes[i].bars = 1; nodes[i].i_max = 255; nodes[i].pack_len = 2;
        nodes[i].chord_out = (uint8_t)(7 | MCH_DOM7 << 4);
    }
    nodes[0].flags = MNF_ANCHOR;
    nodes[0].chord_in = nodes[2].chord_in = MCH_MAJ << 4;
    nodes[1].chord_in = 4 | MCH_MAJ << 4;
    nodes[0].edge0 = 0; nodes[0].n_edge = 2;
    nodes[1].edge0 = 2; nodes[1].n_edge = 1;
    nodes[2].edge0 = 3; nodes[2].n_edge = 1;
    edges[0].to = 1; edges[0].w = 200;
    edges[1].to = 2; edges[1].w = 1;
    edges[2].to = 0; edges[2].w = 1;
    edges[3].to = 0; edges[3].w = 1;
    cv.blob = blob; cv.cue = &c; cv.nodes = nodes; cv.edges = edges; cv.len = sizeof(blob); cv.id = 250;
    mus_bank_use(NULL);
    mus_seams = 0;
    n = walk_cv(&cv, 128, 5, p, 200);
    CHECK_EQ(n, 200);
    for (i = 0; i < n; i++) y += p[i].node == 1;
    CHECK_MSG(y == 0 && mus_seams == 0, "Y entered %d times, %u seams broken", y, (unsigned)mus_seams);
    nodes[0].n_edge = 1;                                 /* X -> Y only: relaxed, counted */
    n = walk_cv(&cv, 128, 5, p, 200);
    for (i = 0; i < n; i++) y += p[i].node == 1;
    CHECK_MSG(y >= 90 && (int)mus_seams == y, "Y entered %d times, %u seams counted", y, (unsigned)mus_seams);
    /* the test bank's authored seams are all legal (mmlc lints them) */
    mus_bank_use(mus_testbank);
    mus_core_init();
    mus_seams = 0;
    b = mus_bank();
    dir = (const MusDirEnt *)(const void *)((const uint8_t *)b + b->dir_off);
    for (e = 0; e < b->n_cues; e++) {
        if (MUS_DIR_KIND(&dir[e]) != MDK_CUE) continue;
        for (I = 0; I < 256; I += 85) CHECK(walk(dir[e].id, I, 99u + (uint32_t)I, p, 200) == 200);
    }
    CHECK_EQ(mus_seams, 0);
    use_game_bank();
}
