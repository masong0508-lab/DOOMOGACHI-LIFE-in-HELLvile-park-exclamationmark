/* test_music_hash.c - T2 (native part): the engine is deterministic (owner:
 * MU6; docs/design/music/SPEC.md sec.19 T2, CF-33, CF-34).
 *
 *   - music_hash(cue, 2): the hash-mode decision stream of every cue (2
 *     minutes, generation-time, merge order) equals the golden recorded in
 *     core/tests/data/music_golden.h - on every compiler and, from MU2/MU3's
 *     `music-hash` harness command, in the GBA and PS1 ROMs;
 *   - the native DoomoVoice golden: what the legacy bridge sends for every
 *     classic song over 600 ticks.
 * A cue whose bytes changed (someone edited its MML) is reported and
 * skipped, not failed: re-record with `core/build/tests --update-golden
 * music_t2`. The same bytes giving a new hash is an engine change - FAIL. */
#include "test.h"
#include "../src/audio_int.h"
#include "../src/mus_int.h"
#include "data/music_golden.h"

static uint32_t fnv_bytes(uint32_t h, const uint8_t *p, uint32_t n)
{
    while (n--) { h ^= *p++; h *= 0x01000193u; }
    return h;
}

static uint32_t cue_bytes_fnv(int cue)
{
    MusCueView cv;
    if (!mus_bank_cue(cue, &cv)) return 0;
    return fnv_bytes(0x811C9DC5u, cv.blob, cv.len);
}

/* every channel state the bridge produced, 600 ticks per song */
static uint32_t tone_hash(int song)
{
    uint32_t h = 0x811C9DC5u;
    int t, c;
    test_reset();
    audio_init();
    audio_music(song);
    for (t = 0; t < 600; t++) {
        audio_tick();
        for (c = 0; c < AUDIO_CH; c++) {
            DoomoToneState st;
            uint8_t b[4];
            audio_channel_state(c, &st);
            b[0] = (uint8_t)st.freq; b[1] = (uint8_t)(st.freq >> 8); b[2] = st.vol; b[3] = st.wave;
            h = fnv_bytes(h, b, 4);
        }
    }
    return h;
}

static void write_golden(void)
{
    char path[600];
    FILE *f;
    int i;
    snprintf(path, sizeof(path), "%s/../data/music_golden.h", test_golden_dir);
    f = fopen(path, "w");
    if (!f) { fprintf(stderr, "    cannot write %s\n", path); return; }
    fprintf(f, "/* music_golden.h - T2's goldens (owner: MU6). RECORDED by\n"
               " * `core/build/tests --update-golden music_t2` (test_music_hash.c); do\n"
               " * not edit. Per classic cue: the FNV of its bank bytes, music_hash(cue, 2)\n"
               " * and the bridge's channel-state hash over 600 ticks. */\n"
               "#define MUSIC_GOLDEN_N %d\n"
               "static const uint32_t music_golden[MUSIC_GOLDEN_N][3] = {\n", MC_CLASSIC_COUNT);
    for (i = 0; i < MC_CLASSIC_COUNT; i++)
        fprintf(f, "    { 0x%08Xu, 0x%08Xu, 0x%08Xu },   /* %s */\n", (unsigned)cue_bytes_fnv(MC_CLASSIC_FIRST + i),
                (unsigned)music_hash(MC_CLASSIC_FIRST + i, 2), (unsigned)tone_hash(i + 1),
                audio_song_name(i + 1));
    fprintf(f, "};\n");
    fclose(f);
    fprintf(stderr, "    recorded %s\n", path);
}

TEST(music_t2_hashes_equal_the_golden)
{
    int i, skipped = 0;
    if (test_update_golden) { write_golden(); return; }
    CHECK_EQ(MUSIC_GOLDEN_N, MC_CLASSIC_COUNT);
    for (i = 0; i < MC_CLASSIC_COUNT; i++) {
        int cue = MC_CLASSIC_FIRST + i;
        if (cue_bytes_fnv(cue) != music_golden[i][0]) {
            fprintf(stderr, "    note: %s changed since the golden was recorded (re-record T2)\n",
                    audio_song_name(i + 1));
            skipped++;
            continue;
        }
        CHECK_MSG(music_hash(cue, 2) == music_golden[i][1], "%s: music_hash 0x%08X, golden 0x%08X",
                  audio_song_name(i + 1), (unsigned)music_hash(cue, 2), (unsigned)music_golden[i][1]);
        CHECK_MSG(tone_hash(i + 1) == music_golden[i][2], "%s: bridge hash 0x%08X, golden 0x%08X",
                  audio_song_name(i + 1), (unsigned)tone_hash(i + 1), (unsigned)music_golden[i][2]);
    }
    CHECK(skipped < MC_CLASSIC_COUNT);
}

TEST(music_t2_hash_mode_is_pure)
{
    /* the hash never disturbs the playing music, and repeats itself */
    uint32_t a, b;
    DoomoToneState st0, st1;
    test_reset();
    audio_init();
    audio_music(SONG_TITLE);
    { int t; for (t = 0; t < 50; t++) audio_tick(); }
    audio_channel_state(0, &st0);
    a = music_hash(MC_CLASSIC_FIRST + SONG_BOSS - 1, 2);
    b = music_hash(MC_CLASSIC_FIRST + SONG_BOSS - 1, 2);
    CHECK_EQ_U(a, b);
    CHECK(a != 0);
    CHECK(a != music_hash(MC_CLASSIC_FIRST + SONG_BOSS - 1, 3));
    CHECK(a != music_hash(MC_CLASSIC_FIRST + SONG_TITLE - 1, 2));
    audio_channel_state(0, &st1);
    CHECK(st0.freq == st1.freq && st0.vol == st1.vol);
    CHECK_EQ(music_cue(), MC_CLASSIC_FIRST + SONG_TITLE - 1);
    CHECK_EQ(music_hash(MC_NONE, 2), 0);
}
