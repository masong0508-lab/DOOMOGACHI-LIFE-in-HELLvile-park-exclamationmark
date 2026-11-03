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
