/* test_pack.c - pack.c (E2): the reader opens the SFW example pack that
 * tools/mpack/mpack.py builds (fixtures/example.dmpk, kept byte-identical
 * by story-check), serves its art and scripts, and refuses every damaged
 * copy. */
#include <stdio.h>
#include <stdlib.h>
#include "test.h"
#include "doomo/pack.h"
#include "doomo/txt.h"
#include "doomo/story_vm.h"
#include "doomo/camfx.h"
#include "doomo/ending.h"
#include "doomo/options_store.h"
#include "doomo/doomo.h"

#define MAXPACK 8192
static uint8_t g_pk[MAXPACK];
static uint32_t g_n;

static int load(void)
{
    char path[512];
    FILE *f;
    snprintf(path, sizeof path, "%s/../fixtures/example.dmpk", test_golden_dir);
    f = fopen(path, "rb");
    if (!f) return 0;
    g_n = (uint32_t)fread(g_pk, 1, MAXPACK, f);
    fclose(f);
    return g_n > 64;
}

static void refix(uint8_t *b, uint32_t n)           /* recompute the header crc after a deliberate change */
{
    uint32_t c = 0xFFFFFFFFu, i;
    int k;
    for (i = 16; i < n; i++) { c ^= b[i]; for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); }
    c ^= 0xFFFFFFFFu;
    memcpy(b + 12, &c, 4);
}

TEST(pack_opens_the_example_and_serves_it)
{
    DoomoPackInfo info;
    uint32_t size = 0, i;
    static uint8_t cg[PACK_CG_W * PACK_CG_H];
    char buf[64];
    test_reset();
    CHECK(load());
    CHECK_EQ(pack_open(), 0);                                 /* nothing from the platform */
    CHECK_EQ(pack_open_image(g_pk, g_n), 1);
    CHECK_EQ(pack_open(), 1);
    CHECK_EQ(pack_info(&info), 1);
    CHECK(strcmp(info.name, "SFW Example") == 0);
    CHECK(strcmp(info.author, "Doomogachi") == 0);
    CHECK_EQ(info.port, PK_PORT_NATIVE);
    CHECK(info.flags & PACK_F_SFW_EXAMPLE);
    CHECK_EQ(info.n_cg, 1);
    CHECK_EQ(info.n_script, 1);
    CHECK_EQ(info.total_size, g_n);
    /* the still is LZ: no in-place pointer, a decode into the caller's buffer */
    CHECK(pack_art(PK_KIND_CG, PACK_UID(PK_KIND_CG, 0), &size) == NULL);
    CHECK_EQ(pack_art_read(PK_KIND_CG, PACK_UID(PK_KIND_CG, 0), cg, 100), 0);   /* too small */
    CHECK_EQ(pack_art_read(PK_KIND_CG, PACK_UID(PK_KIND_CG, 0), cg, sizeof cg), PACK_CG_W * PACK_CG_H);
    for (i = 0; i < sizeof cg; i += 977)                      /* the test pattern mpack.py wrote */
        CHECK_EQ(cg[i], (uint8_t)((((i % PACK_CG_W) / 20 + (i / PACK_CG_W) / 20) * 7) & 255));
    CHECK_EQ(pack_art_read(PK_KIND_FIGURE, PACK_UID(PK_KIND_CG, 0), cg, sizeof cg), 0);   /* wrong kind */
    CHECK_EQ(pack_art_read(PK_KIND_CG, PACK_UID(PK_KIND_CG, 5), cg, sizeof cg), 0);        /* no such entry */
    /* the script is registered with the text engine, in place */
    CHECK_EQ(pack_script(0), TXT_PACK_USER_FIRST);
    CHECK_EQ(pack_script(1), 0);
    CHECK_EQ(pack_script(-1), 0);
    CHECK(txt_pack_is_live(TXT_PACK_USER_FIRST));
    CHECK_EQ(txt_get(TXT_ID(TXT_PACK_USER_FIRST, 0), buf, (int)sizeof buf), 41);
    CHECK(pack_art(PK_KIND_SCRIPT, PACK_UID_SCRIPT(0), &size) != NULL && size > 64);
    /* closing forgets the pack and the script */
    pack_close();
    CHECK_EQ(pack_info(&info), 0);
    CHECK_EQ(info.total_size, 0u);
    CHECK_EQ(pack_script(0), 0);
    CHECK(!txt_pack_is_live(TXT_PACK_USER_FIRST));
}

TEST(pack_refuses_damaged_copies)
{
    static uint8_t b[MAXPACK];
    uint32_t i;
    test_reset();
    CHECK(load());
    CHECK_EQ(pack_open_image(NULL, 100), 0);
    CHECK_EQ(pack_open_image(g_pk, 40), 0);
    CHECK_EQ(pack_open_image(g_pk, g_n - 4), 0);                       /* truncated: total_size */
    for (i = 16; i < g_n; i += 97) {                                   /* any flipped byte: the crc */
        memcpy(b, g_pk, g_n);
        b[i] ^= 0x10;
        CHECK_EQ(pack_open_image(b, g_n), 0);
    }
    memcpy(b, g_pk, g_n); b[0] ^= 1;                    CHECK_EQ(pack_open_image(b, g_n), 0);   /* magic */
    memcpy(b, g_pk, g_n); b[18] = PK_PORT_GBA; refix(b, g_n); CHECK_EQ(pack_open_image(b, g_n), 0);   /* other port */
    memcpy(b, g_pk, g_n); b[19] |= 0x80; refix(b, g_n);       CHECK_EQ(pack_open_image(b, g_n), 0);   /* unknown flags */
    memcpy(b, g_pk, g_n); b[64 + 12] = 9; refix(b, g_n);      CHECK_EQ(pack_open_image(b, g_n), 0);   /* unknown kind */
    memcpy(b, g_pk, g_n); b[64 + 16 + 14] = 5; refix(b, g_n); CHECK_EQ(pack_open_image(b, g_n), 0);   /* not a user pack id */
    memcpy(b, g_pk, g_n); b[64 + 16 + 13] = PK_CODEC_LZ; refix(b, g_n); CHECK_EQ(pack_open_image(b, g_n), 0);   /* a compressed script */
    memcpy(b, g_pk, g_n); b[64 + 4] += 1; refix(b, g_n);      CHECK_EQ(pack_open_image(b, g_n), 0);   /* misaligned payload */
    /* a failed open leaves nothing behind, and the good copy still opens */
    CHECK_EQ(pack_script(0), 0);
    CHECK(!txt_pack_is_live(TXT_PACK_USER_FIRST));
    CHECK_EQ(pack_open_image(g_pk, g_n), 1);
    pack_close();
}

/* ---- E2: SCENE_MSLOT ---- */
static void mframe(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }

TEST(mslot_runs_the_packs_script_and_gives_the_vm_back)
{
    int n = 0;
    test_reset();
    CHECK(load());
    CHECK_EQ(pack_open_image(g_pk, g_n), 1);
    opt_set(OPT_MATURE, MATURE_PACK);
    vm_set_var(5, 77);
    scene_push(&SCENE_MSLOT, 0);
    while (scene_top() == &SCENE_MSLOT && n++ < 200) mframe(0);
    CHECK(n < 60);
    CHECK_EQ(mslot_result(), 1);
    CHECK_EQ(vm_var(5), 77);
    for (n = 0; n < 30; n++) mframe(0);
    CHECK_EQ(camfx_fade_level(), 0);
    pack_close();
}

TEST(mslot_without_a_pack_plays_the_fade_and_reports_0)
{
    int n = 0;
    test_reset();
    opt_set(OPT_MATURE, MATURE_PACK);
    scene_push(&SCENE_MSLOT, 3);
    while (scene_top() == &SCENE_MSLOT && n++ < 200) mframe(0);
    CHECK(n < 100);
    CHECK_EQ(mslot_result(), 0);
    for (n = 0; n < 30; n++) mframe(0);
    CHECK_EQ(camfx_fade_level(), 0);
}

TEST(mslot_with_a_pack_but_mature_below_pack_still_falls_back)
{
    int n = 0;
    test_reset();
    CHECK(load());
    CHECK_EQ(pack_open_image(g_pk, g_n), 1);
    opt_set(OPT_MATURE, MATURE_ON);
    scene_push(&SCENE_MSLOT, 0);
    while (scene_top() == &SCENE_MSLOT && n++ < 200) mframe(0);
    CHECK_EQ(mslot_result(), 0);
    pack_close();
}

/* ---- E2 try-out: every slot, with and without the example pack ------------------------------------
 * Whatever slot a built-in script, a route or an adult ending pushes (0..MSLOT_MAX-1, or a bad
 * number), the scene must finish, hand the previous scene back, leave the fade at 0, and report 1
 * only for a slot the open pack really has a script for. */
static int run_slot(int slot)
{
    int n = 0;
    const DoomoScene *before = scene_top();
    scene_push(&SCENE_MSLOT, slot);
    while (scene_top() == &SCENE_MSLOT && n++ < 300) mframe(0);
    if (scene_top() == &SCENE_MSLOT || scene_top() != before) return -1;     /* stuck or lost the stack */
    for (n = 0; n < 30; n++) mframe(0);
    if (camfx_fade_level() != 0) return -2;                                  /* a fade left on screen */
    return mslot_result();
}

TEST(e2_every_slot_falls_back_safely_without_a_pack)
{
    int s;
    test_reset();
    opt_set(OPT_MATURE, MATURE_PACK);
    for (s = 0; s < MSLOT_MAX; s++) CHECK_EQ(run_slot(s), 0);
    CHECK_EQ(run_slot(MSLOT_MAX), 0);                  /* out of range: still a clean fade */
    CHECK_EQ(run_slot(255), 0);
    opt_set(OPT_MATURE, MATURE_OFF);
    CHECK_EQ(run_slot(2), 0);                          /* Mature Off: never anything but the fade */
}

TEST(e2_the_example_pack_plays_only_the_slots_it_has)
{
    int s, played = 0;
    test_reset();
    CHECK(load());
    CHECK_EQ(pack_open_image(g_pk, g_n), 1);
    opt_set(OPT_MATURE, MATURE_PACK);
    for (s = 0; s < MSLOT_MAX; s++) {
        int r = run_slot(s);
        CHECK(r == 0 || r == 1);                       /* never stuck, never a stray fade */
        if (r == 1) played++;
    }
    CHECK(played >= 1);                                /* slot 0 is the example's */
    CHECK_EQ(run_slot(0), 1);
    CHECK_EQ(run_slot(3), 0);                          /* a slot the pack lacks falls back */
    pack_close();
    CHECK_EQ(run_slot(0), 0);                          /* closed again: back to the fade */
}

/* ---- E9: a streamed pack (the PS1 / DC disc file) ------------------------------------------
 * The test plays the disc: plat_pack_read() serves g_pk while g_stream_on is set (else 0, the
 * default every other test expects), in 7-byte-misaligned reads of at most `len` bytes like the
 * sector window does, and plat_mslot_script_buf() is the port's script buffer. */
static int g_stream_on;
static uint32_t g_stream_reads;
static uint8_t g_sbuf[PACK_SCRIPT_CAP_PS1] __attribute__((aligned(4)));

uint32_t plat_pack_read(uint32_t off, void *dst, uint32_t len)
{
    if (!g_stream_on || off > g_n || len > g_n - off) return 0;
    memcpy(dst, g_pk + off, len);
    g_stream_reads++;
    return len;
}

uint8_t *plat_mslot_script_buf(uint32_t *cap)
{
    if (cap) *cap = g_stream_on ? (uint32_t)sizeof g_sbuf : 0;
    return g_stream_on ? g_sbuf : NULL;
}

TEST(e9_a_streamed_pack_opens_and_binds_one_script_at_a_time)
{
    char buf[64];
    static uint8_t small[16] __attribute__((aligned(4)));
    test_reset();
    CHECK(load());
    pack_close();
    g_stream_on = 1; g_stream_reads = 0;
    CHECK_EQ(pack_open(), 1);                                  /* header, directory and CRC via plat_pack_read */
    CHECK(g_stream_reads > 3);
    CHECK_EQ(pack_streamed(), 1);
    CHECK_EQ(pack_script(0), 0);                               /* streamed: nothing registered until bound */
    CHECK(!txt_pack_is_live(TXT_PACK_USER_FIRST));
    CHECK_EQ(pack_script_bind(0, small, sizeof small), 0);     /* too small a buffer */
    CHECK_EQ(pack_script_bind(5, g_sbuf, sizeof g_sbuf), 0);   /* a slot the pack lacks */
    CHECK_EQ(pack_script_bind(0, g_sbuf, sizeof g_sbuf), TXT_PACK_USER_FIRST);
    CHECK(txt_pack_is_live(TXT_PACK_USER_FIRST));
    CHECK_EQ(txt_get(TXT_ID(TXT_PACK_USER_FIRST, 0), buf, (int)sizeof buf), 41);
    pack_script_release();
    CHECK(!txt_pack_is_live(TXT_PACK_USER_FIRST));
    CHECK_EQ(pack_script_bind(0, g_sbuf, sizeof g_sbuf), TXT_PACK_USER_FIRST);   /* and again */
    pack_close();
    CHECK(!txt_pack_is_live(TXT_PACK_USER_FIRST));
    CHECK_EQ(pack_streamed(), 0);
    g_stream_on = 0;
}

TEST(e9_a_damaged_streamed_pack_is_refused)
{
    uint32_t keep;
    test_reset();
    CHECK(load());
    pack_close();
    g_stream_on = 1;
    g_pk[g_n - 1] ^= 0xFF;                                     /* the CRC covers the last byte */
    CHECK_EQ(pack_open(), 0);
    CHECK_EQ(pack_streamed(), 0);
    g_pk[g_n - 1] ^= 0xFF;
    keep = g_n; g_n = 40;                                      /* the file is shorter than the header says */
    CHECK_EQ(pack_open(), 0);
    g_n = keep;
    g_stream_on = 0;
    CHECK_EQ(pack_open(), 0);                                  /* no disc file: no pack */
}

TEST(e9_the_example_plays_from_a_streamed_pack_and_leaves_nothing_bound)
{
    test_reset();
    CHECK(load());
    pack_close();
    g_stream_on = 1;
    CHECK_EQ(pack_open(), 1);
    opt_set(OPT_MATURE, MATURE_PACK);
    CHECK_EQ(run_slot(0), 1);                                  /* the script plays from the port's buffer */
    CHECK(!txt_pack_is_live(TXT_PACK_USER_FIRST));             /* released when the scene left */
    CHECK_EQ(run_slot(3), 0);                                  /* a slot the pack lacks: the fade */
    CHECK_EQ(run_slot(0), 1);                                  /* and slot 0 works a second time */
    g_stream_on = 0;
    pack_close();
    CHECK_EQ(run_slot(0), 0);
}
