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

TEST(music_t4_the_bank_is_well_formed)
{
    const MusBank *b = mus_bank();
    const MusDirEnt *dir = (const MusDirEnt *)(const void *)(doomo_mus_bank + b->dir_off);
    int e, last_id = -1;
    char why[160];
    CHECK_EQ(b->magic, MUS_BANK_MAGIC);
    CHECK(b->n_cues >= MC_CLASSIC_COUNT);
    CHECK(b->inst_off + 20u * b->n_inst <= b->total);
    for (e = 0; e < b->n_cues + b->n_stings; e++) {
        const MusDirEnt *d = &dir[e];
        MusCueView cv;
        int n, r;
        CHECK_MSG(d->off % 4 == 0 && d->off + d->len <= b->total, "dir %d out of the bank", e);
        if (e < b->n_cues) { CHECK_MSG((int)d->id > last_id, "directory not sorted at %d", e); last_id = d->id; }
        if (e >= b->n_cues) continue;
        CHECK(mus_bank_cue(d->id, &cv));
        CHECK(cv.cue->n_roles <= 12 && cv.cue->n_nodes >= 1 && cv.cue->n_nodes <= 32);
        CHECK(cv.cue->entry < cv.cue->n_nodes && cv.cue->reentry < cv.cue->n_nodes);
        CHECK(cv.cue->tag < cv.cue->n_nodes);
        CHECK(cv.cue->meter >= 1 && cv.cue->bpm >= 30);
        CHECK((unsigned)cv.cue->common_off + cv.cue->common_len <= cv.len);
        for (r = 0; r < cv.cue->n_roles; r++) CHECK(cv.roles[r].inst < b->n_inst);
        for (n = 0; n < cv.cue->n_nodes; n++) {
            const MusNode *nd = &cv.nodes[n];
            const uint8_t *pack = cv.blob + nd->pack_off;
            long want = (long)nd->bars * cv.cue->meter * MUS_UPQ;
            CHECK_MSG((unsigned)nd->pack_off + nd->pack_len <= cv.len && nd->pack_len <= MUS_PACK_MAX,
                      "cue %d node %d pack out of bounds", d->id, n);
            CHECK(nd->n_edge >= 1 && nd->edge0 < cv.cue->n_edges && cv.edges[nd->edge0].to < cv.cue->n_nodes);
            for (r = 0; r < pack[0] && r < cv.cue->n_roles; r++) {
                unsigned off = rd16(pack + 2 + 2 * r);
                long got;
                if (!off) continue;
                CHECK(off < nd->pack_len);
                got = lane_units(pack, nd->pack_len, off, why, sizeof(why));
                CHECK_MSG(got == want, "cue %d (%s) node %d role %d: %ld units, want %ld %s", d->id,
                          music_cue_name(d->id), n, r, got, want, got < 0 ? why : "");
            }
        }
    }
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
