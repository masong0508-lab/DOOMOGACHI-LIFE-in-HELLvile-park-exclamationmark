/* test_music_data.c - T4: the bank and the engine under stress (owner: MU6;
 * docs/design/music/SPEC.md sec.5, 6.2, 19 T4, D22).
 *
 *   - the bank: checksum, every offset in bounds, every lane of every node
 *     sums to the node's length, CALL / RET well formed, every SONG_* cue
 *   - 30 minutes of random play (songs, stingers, duals, mutes, ducks,
 *     tape-stops, the volume option): no crash, voices within the driver's
 *     range, every key-on closed within 16 bars
 *   - no under-run with an IRQ-style driver at 15 Hz logic for 30 minutes
 *     on the densest cues, nor during a 0.9 s load after music_prefill(1500)
 *     (native rings are the PS1's 128 entries, D22) */
#include "test.h"
#include "../src/audio_int.h"
#include "../src/mus_int.h"
#include "data/mus_testbank.h"

static const uint16_t t4_len[16] = MUS_LEN_TAB_INIT;

static unsigned rd16(const uint8_t *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }

/* the length (units) of one lane, walking the token grammar; -1 = broken */
static long lane_units(const uint8_t *pack, unsigned plen, unsigned off, char *why, int cap)
{
    unsigned i = off, ret = 0, guard = 0;
    int in_call = 0, deflen = 24;
    long total = 0;
    while (guard++ < 4096) {
        int b, len = -1;
        if (i >= plen) { snprintf(why, (size_t)cap, "lane runs off the pack at %u", i); return -1; }
        b = pack[i++];
        if (b >= MT_CMD && b < MT_CMD + MTC_COUNT) {
            int a = pack[i++], c = b - MT_CMD;
            if (c == MTC_DEFLEN) deflen = a ? a : 256;
            else if (c == MTC_CALL) {
                if (in_call) { snprintf(why, (size_t)cap, "a CALL inside a subroutine"); return -1; }
                if ((unsigned)a * 4 >= plen) { snprintf(why, (size_t)cap, "CALL past the pack"); return -1; }
                ret = i; i = (unsigned)a * 4; in_call = 1;
            } else if (c == MTC_RET) {
                if (!in_call) { snprintf(why, (size_t)cap, "RET outside a subroutine"); return -1; }
                i = ret; in_call = 0;
            }
            continue;
        }
        if (b == MT_END) {
            if (in_call) { snprintf(why, (size_t)cap, "END inside a subroutine"); return -1; }
            return total;
        }
        if (b >= MT_EXT && b < MT_HOLD) { snprintf(why, (size_t)cap, "a stray EXT"); return -1; }
        if (b < MT_NOTE_L || (b >= MT_HIT && b < MT_TONE) || b >= MT_HOLD) len = deflen;
        else if (b < MT_TONE) len = t4_len[b & 15];
        else { snprintf(why, (size_t)cap, "reserved token 0x%02x", b); return -1; }
        if (b >= MT_NOTE_L && b < MT_REST) {
            if (pack[i] > 95) { snprintf(why, (size_t)cap, "pitch byte %u", pack[i]); return -1; }
            i++;
        }
        while (i < plen && pack[i] >= MT_EXT && pack[i] < MT_HOLD) {
            len += pack[i] < MT_EXTS ? t4_len[pack[i] & 15] : (pack[i] & 7) + 1;
            i++;
        }
        total += len;
    }
    snprintf(why, (size_t)cap, "lane never ends");
    return -1;
}

/* every cue and stinger of the current bank (mus_bank_use): header fields,
 * offsets, edges and the lanes of every node - the variants of a GEN node
 * too - summing to the node's length */
static void check_bank(void)
{
    const MusBank *b = mus_bank();
    const uint8_t *B = (const uint8_t *)(const void *)b;
    const MusDirEnt *dir = (const MusDirEnt *)(const void *)(B + b->dir_off);
    int e, last_id = -1;
    char why[160];
    CHECK_EQ(b->magic, MUS_BANK_MAGIC);
    CHECK(b->inst_off + 20u * b->n_inst <= b->total);
    for (e = 0; e < b->n_cues + b->n_stings; e++) {
        const MusDirEnt *d = &dir[e];
        MusCueView cv;
        int n, r;
        CHECK_MSG(d->off % 4 == 0 && d->off + d->len <= b->total, "dir %d out of the bank", e);
        if (e == b->n_cues) last_id = -1;
        CHECK_MSG((int)d->id > last_id, "directory not sorted at %d", e);
        last_id = d->id;
        if (e < b->n_cues) {
            if (MUS_DIR_KIND(d) != MDK_CUE && MUS_DIR_KIND(d) != MDK_LEGACY) continue;
            if (!mus_bank_cue(d->id, &cv)) { CHECK_MSG(0, "cue %d not found", d->id); continue; }
        } else {
            if (!mus_bank_sting(d->id, &cv)) { CHECK_MSG(0, "sting %d not found", d->id); continue; }
            CHECK(cv.sting != NULL);
        }
        CHECK(cv.cue->n_roles <= 12 && cv.cue->n_nodes >= 1 && cv.cue->n_nodes <= 32);
        CHECK(cv.cue->entry < cv.cue->n_nodes && cv.cue->reentry < cv.cue->n_nodes);
        CHECK(cv.cue->tag < cv.cue->n_nodes);
        CHECK(cv.cue->meter >= 1 && cv.cue->bpm >= 30);
        CHECK((unsigned)cv.cue->common_off + cv.cue->common_len <= cv.len);
        for (r = 0; r < cv.cue->n_roles; r++) CHECK(cv.roles[r].inst < b->n_inst);
        for (r = 0; r < cv.cue->n_binds; r++) CHECK(cv.binds[r].param < MP_COUNT);
        for (n = 0; n < cv.cue->n_nodes; n++) {
            const MusNode *nd = &cv.nodes[n];
            const uint8_t *pack = cv.blob + nd->pack_off;
            long want = (long)nd->bars * cv.cue->meter * MUS_UPQ;
            int k;
            CHECK_MSG((unsigned)nd->pack_off + nd->pack_len <= cv.len &&
                      ((pack[1] & MNP_VARIANTS) || nd->pack_len <= MUS_PACK_MAX),
                      "cue %d node %d pack out of bounds", d->id, n);
            CHECK(nd->n_edge >= 1 && nd->edge0 + nd->n_edge <= cv.cue->n_edges);
            for (k = nd->edge0; k < nd->edge0 + nd->n_edge && k < cv.cue->n_edges; k++)
                CHECK(cv.edges[k].to < cv.cue->n_nodes);
            for (r = 0; r < pack[0] && r < cv.cue->n_roles; r++) {
                unsigned off = rd16(pack + 2 + 2 * r);
                long got;
                if (!off) continue;
                CHECK(off < nd->pack_len);
                got = lane_units(pack, nd->pack_len, off, why, sizeof(why));
                CHECK_MSG(got == want, "cue %d (%s) node %d role %d: %ld units, want %ld %s", d->id,
                          music_cue_name(d->id), n, r, got, want, got < 0 ? why : "");
            }
            if (pack[1] & MNP_VARIANTS) {                /* MusVarDir + the variants */
                const uint8_t *vd = pack + rd16(pack + 2 + 2 * pack[0]);
                int nv = vd[0], v, owned = 0;
                unsigned mask = rd16(vd + 2);
                for (r = 0; r < 16; r++) owned += (mask >> r) & 1;
                CHECK(nv >= 1 && nv <= 8 && vd[1] >= 1 && owned >= 1);
                for (v = 0; v < nv && v < 8; v++) {
                    const uint8_t *vb = pack + rd16(vd + 4 + 2 * v);
                    int kv = (vb[0] & 1) ? nd->bars * 2 : 0, j;
                    CHECK((vb[0] >> 4) < vd[1]);
                    for (j = 0; j < owned; j++) {
                        unsigned off = rd16(vb + 1 + kv + 2 * j);
                        long got;
                        if (!off) continue;
                        got = lane_units(pack, nd->pack_len, off, why, sizeof(why));
                        CHECK_MSG(got == want, "cue %d node %d variant %d lane %d: %ld units, want %ld %s", d->id,
                                  n, v, j, got, want, got < 0 ? why : "");
                    }
                }
            }
        }
    }
}

TEST(music_t4_the_bank_is_well_formed)
{
    CHECK(mus_bank()->n_cues >= MC_CLASSIC_COUNT);
    check_bank();
}

/* the test bank (core/tests/music: mmlc's authored cues and stingers) */
TEST(music_t4_the_test_bank_is_well_formed)
{
    MusCueView cv;
    const MusTrans *t;
    mus_bank_use(mus_testbank);
    check_bank();
    CHECK_EQ(mus_bank()->n_stings, 2);
    CHECK(mus_bank_cue(MCT_HUB, &cv));
    CHECK_EQ(cv.cue->n_nodes, 4);
    CHECK(cv.nodes[0].flags & MNF_ANCHOR);
    CHECK(cv.nodes[3].flags & MNF_BREAKDOWN);
    CHECK(cv.blob[cv.nodes[2].pack_off + 1] & MNP_VARIANTS);      /* node C: the GEN slot */
    CHECK_EQ(cv.cue->n_binds, 1);
    CHECK(mus_bank_sting(STGT_TADA, &cv));
    CHECK_EQ(cv.sting->prio, 2);
    CHECK(mus_bank_sting(STGT_HIT, &cv));
    CHECK_EQ(cv.sting->hit_units, 0);
    t = mus_trans_find(MCT_HUB, MCT_TWO);
    CHECK(t && t->type == MTT_FILL);
    t = mus_trans_find(MCT_TWO, MCT_HUB);
    CHECK(t && t->type == MTT_VINYL && t->arg == VINYL_NEEDLE);
    mus_bank_use(NULL);
}

/* M1.5: the shared banks (MU4d, core/music/common) - every index and
 * record inside the bank, the 16 vinyl patterns + the crackle toggle, the
 * motif and instrument locks, sorted transitions, the turnaround table and
 * the synth SFX imported from audio_fx.c (D14, CF-3) */
static int in_bank(uint32_t off, uint32_t len) { return off && off + len <= mus_bank()->total; }

TEST(music_t4_the_shared_banks_are_in_bounds)
{
    const MusBank *b = mus_bank();
    const uint8_t *B = doomo_mus_bank;
    const uint32_t *misc;
    const MusDirEnt *dir = (const MusDirEnt *)(const void *)(B + b->dir_off);
    int i, sfx = 0;
    CHECK(in_bank(b->misc_off, 4u * MUS_MISC_COUNT));
    misc = (const uint32_t *)(const void *)(B + b->misc_off);
    CHECK_EQ(b->n_inst, INST_BED_FIREWORKS + 1);               /* 24 classic + the shared bank */
    /* comps: u16 index from the section start, then 4 + 2 per event */
    CHECK(b->n_comp >= 16 && in_bank(b->comp_off, 2u * b->n_comp));
    for (i = 0; i < b->n_comp; i++) {
        unsigned o = rd16(B + b->comp_off + 2 * i);
        const MusComp *c = (const MusComp *)(const void *)(B + b->comp_off + o);
        CHECK_MSG(o && in_bank(b->comp_off + o, 4u) && in_bank(b->comp_off + o, 4u + 2u * c->n_ev), "comp %d", i);
        CHECK_MSG(c->n_ev >= 1 && c->len_beats >= 1 && c->len_beats <= 8 && c->pad, "comp %d header", i);
    }
    /* grooves: 64 B each; steps 12 or 16; hits are KH_* */
    CHECK(b->n_groove >= 8 && in_bank(b->groove_off, 64u * b->n_groove));
    for (i = 0; i < b->n_groove; i++) {
        const MusGroove *g = (const MusGroove *)(const void *)(B + b->groove_off) + i;
        int r;
        CHECK_MSG(g->kit < KIT_COUNT && (g->steps == 12 || g->steps == 16), "groove %d", i);
        for (r = 0; r < 6; r++) CHECK_MSG(g->hit[r] < KH_COUNT, "groove %d row %d", i, r);
    }
    /* fills, motifs, vinyl: u16 index (0 = absent) + records */
    {
        static const int kinds[3] = { MUS_MISC_FILLS, MUS_MISC_MOTIFS, MUS_MISC_VINYL };
        const int counts[3] = { b->n_fill, b->n_motif, b->n_vinyl };
        int k;
        for (k = 0; k < 3; k++) {
            uint32_t sec = misc[kinds[k]];
            CHECK_MSG(in_bank(sec, 2u * (uint32_t)counts[k]), "misc table %d", kinds[k]);
            for (i = 0; i < counts[k]; i++) {
                unsigned o = rd16(B + sec + 2 * i);
                const uint8_t *rec = B + sec + o;
                uint32_t len;
                if (!o) continue;
                len = kinds[k] == MUS_MISC_FILLS ? 2u + 2u * rec[0] :
                      kinds[k] == MUS_MISC_MOTIFS ? 4u + 2u * rec[0] : 4u + 4u * rec[0];
                CHECK_MSG(in_bank(sec + o, len), "misc table %d record %d", kinds[k], i);
            }
        }
        CHECK_EQ(b->n_motif, MOTIF_GOODBYE + 1);
        for (i = 0; i <= MOTIF_GOODBYE; i++) {
            unsigned o = rd16(B + misc[MUS_MISC_MOTIFS] + 2 * i);
            const MusMotif *m = (const MusMotif *)(const void *)(B + misc[MUS_MISC_MOTIFS] + o);
            CHECK_MSG(o && m->n_note >= 1 && m->head <= m->n_note, "motif %d", i);   /* BEEP_EGG is one note */
        }
        CHECK_EQ(b->n_vinyl, VINYL_TAPESTOP + 1);
        for (i = 1; i <= VINYL_TAPESTOP; i++) {
            unsigned o = rd16(B + misc[MUS_MISC_VINYL] + 2 * i);
            const MusVinyl *v = (const MusVinyl *)(const void *)(B + misc[MUS_MISC_VINYL] + o);
            CHECK_MSG(o, "vinyl %d has no pattern", i);
            CHECK_MSG(i == VINYL_CRACKLE ? v->n_seg == 0 : v->n_seg >= 1, "vinyl %d: %d segments", i, v->n_seg);
            CHECK_MSG(v->pad <= AFTER_NEXT_CUE, "vinyl %d after", i);
        }
    }
    /* transitions: sorted by (from, to), the (*, *) row present */
    CHECK(b->n_trans >= 1 && in_bank(misc[MUS_MISC_TRANS], 8u * b->n_trans));
    {
        const MusTrans *t = (const MusTrans *)(const void *)(B + misc[MUS_MISC_TRANS]);
        int any = 0;
        for (i = 0; i < b->n_trans; i++) {
            if (i) CHECK_MSG(t[i - 1].from < t[i].from || (t[i - 1].from == t[i].from && t[i - 1].to < t[i].to),
                             "transitions not sorted at %d", i);
            CHECK(t[i].grid < MQ_COUNT && t[i].type <= MTT_SYNC);
            any |= t[i].from == 0xFF && t[i].to == 0xFF;
        }
        CHECK(any);
    }
    /* turnarounds: header, cadence bitmaps per family, the MusTurn rows */
    {
        const uint8_t *tt = B + misc[MUS_MISC_TURNS];
        CHECK(in_bank(misc[MUS_MISC_TURNS], 4u));
        CHECK(tt[0] >= 8 && tt[1] == 3);
        CHECK(in_bank(misc[MUS_MISC_TURNS], 4u + (uint32_t)tt[1] * MUS_CAD_BYTES + 8u * tt[0]));
        for (i = 0; i < tt[0]; i++) {
            const MusTurn *u = (const MusTurn *)(const void *)(tt + 4 + tt[1] * MUS_CAD_BYTES) + i;
            CHECK_MSG(u->bands && u->targets && u->weight, "turnaround %d", i);
        }
    }
    /* the synth SFX: one MDK_SFX cue per audio_fx.c recipe, segments verbatim */
    for (i = 0; i < b->n_cues; i++) {
        MusCueView cv;
        const uint8_t *c;
        int s;
        if (MUS_DIR_KIND(&dir[i]) != MDK_SFX) continue;
        sfx++;
        CHECK(mus_bank_cue(dir[i].id, &cv));
        CHECK(dir[i].id >= MC_SFX_SCRATCH && dir[i].id <= 255);
        c = cv.blob + cv.cue->common_off;
        CHECK_MSG(c[0] >= 1 && cv.cue->common_len == 1 + 6 * c[0], "sfx cue %d common", dir[i].id);
        for (s = 0; s < c[0]; s++) CHECK_MSG(c[1 + 6 * s + 1] >= 1, "sfx cue %d segment %d", dir[i].id, s);
    }
    CHECK_EQ(sfx, SFX_SYNTH_COUNT);
}

TEST(music_t4_every_song_has_its_cue)
{
    int s;
    for (s = SONG_NONE + 1; s < SONG_COUNT; s++) {
        int cue = mus_bind_cue(s, -1, -1, 0);
        CHECK_MSG(cue == MC_CLASSIC_FIRST + s - 1 && music_cue_exists(cue), "song %d -> cue %d", s, cue);
    }
    CHECK_EQ(mus_bind_cue(SONG_NONE, -1, -1, 0), MC_NONE);
    CHECK(!music_cue_exists(MC_NONE));
    CHECK(!music_cue_exists(199));
}

TEST(music_t4_thirty_minutes_of_random_play)
{
    DoomoRng r;
    uint32_t t, next = 0, open_since[MUS_DRV_VOICES];
    uint32_t k0 = 0;
    int v, worst = 0;
    test_reset();
    audio_init();
    rng_seed(&r, 77);
    for (v = 0; v < MUS_DRV_VOICES; v++) open_since[v] = 0;
    for (t = 0; t < 30u * 60u * 30u; t++) {
        if (t == next) {
            int op = (int)(rng_next(&r) % 10), song = 1 + (int)(rng_next(&r) % (SONG_COUNT - 1));
            if (op < 4) audio_music(song);
            else if (op == 4) audio_music_once(song);
            else if (op == 5) audio_music_dual(song, 1 + (int)(rng_next(&r) % (SONG_COUNT - 1)));
            else if (op == 6) audio_music_mute((int)(rng_next(&r) % 40));
            else if (op == 7) audio_music_duck(64 + (int)(rng_next(&r) % 160), (int)(rng_next(&r) % 60));
            else if (op == 8) audio_tapestop(1 + (int)(rng_next(&r) % 20));
            else opt_set(OPT_MUSIC_VOL, (int)(rng_next(&r) % 11));
            next = t + 1 + rng_next(&r) % 600;
        }
        audio_tick();
        if (mus_drv_keyons() != k0) {
            k0 = mus_drv_keyons();
            for (v = 0; v < MUS_DRV_VOICES; v++)
                if (mus_drv_voice(v)->age <= 1 && mus_drv_voice(v)->stage != 4) open_since[v] = t;   /* a new note */
        }
        for (v = 0; v < MUS_DRV_VOICES; v++) {
            if (mus_drv_voice(v)->stage == 4) { open_since[v] = t; continue; }
            if ((int)(t - open_since[v]) > worst) worst = (int)(t - open_since[v]);
        }
        CHECK(mus_drv_ring_used(0) < MUS_RING_A && mus_drv_ring_used(1) < MUS_RING_B);
    }
    /* 16 bars at the slowest classic tempo (66 bpm) = 1,746 ticks */
    CHECK_MSG(worst <= 1746, "a voice stayed open for %d ticks", worst);
    opt_set(OPT_MUSIC_VOL, 10);
}

static void t4_irq_play(int song, uint32_t mts_per_update, uint32_t updates)
{
    uint32_t u, m;
    test_reset();
    audio_init();
    mus_core_test_irq(1);
    audio_music(song);
    for (u = 0; u < updates; u++) {
        music_update();
        for (m = 0; m < mts_per_update; m++) mus_drv_tick();
    }
}

TEST(music_t4_no_under_run_at_15_hz_logic_for_30_minutes)
{
    static const uint8_t dense[] = { SONG_BOSS, SONG_INVASION, SONG_FESTIVAL, SONG_CREDITS, SONG_HELL_BREACH };
    int i;
    for (i = 0; i < DOOMO_COUNTOF(dense); i++) {
        uint32_t k;
        t4_irq_play(dense[i], 16, 30u * 60u * 15u);
        k = mus_drv_keyons();
        CHECK_MSG(mus_drv_underruns() == 0, "%s: %u under-runs", audio_song_name(dense[i]),
                  (unsigned)mus_drv_underruns());
        CHECK_MSG(k > 1000, "%s played %u notes", audio_song_name(dense[i]), (unsigned)k);
    }
    mus_core_test_irq(0);
}

TEST(music_t4_a_load_after_prefill_does_not_under_run)
{
    static const uint8_t dense[] = { SONG_BOSS, SONG_INVASION, SONG_FESTIVAL };
    int i, m;
    for (i = 0; i < DOOMO_COUNTOF(dense); i++) {
        t4_irq_play(dense[i], 8, 200);
        music_prefill(1500);
        for (m = 0; m < 216; m++) mus_drv_tick();           /* a 0.9 s CdReadSync */
        CHECK_MSG(mus_drv_underruns() == 0, "%s: %u under-runs in the load", audio_song_name(dense[i]),
                  (unsigned)mus_drv_underruns());
    }
    mus_core_test_irq(0);
}
