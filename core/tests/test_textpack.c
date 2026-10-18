/* test_textpack.c - the text packs and their decoder (S2; SPEC sec.5,
 * R6, C3; SF sec.18.1 textpack_roundtrip, story_text_fits_the_box). The
 * python tool already round-trips every string through its reference
 * decoder at build time; this checks the C decoder (the one the GBA and
 * the PS1 run) on the generated packs. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/txt.h"
#include "doomo/beat.h"
#include "doomo/story_vm.h"
#include "../src/story_int.h"

TEST(textpack_decodes_known_lines)
{
    char buf[200];
    CHECK(txt_get(TXT_OBJ_C1_MAYOR, buf, (int)sizeof(buf)) > 0);
    CHECK_STR(buf, "Say hi to Mayor Giggs by the fountain");
    CHECK(txt_get(TXT_VMT_HELLO, buf, (int)sizeof(buf)) > 0);
    CHECK_STR(buf, "Hello, @P! You have @1 hats.");
    CHECK(txt_get(TXT_VEX_PA1, buf, (int)sizeof(buf)) > 0);           /* ch2/ep27.dsc */
    CHECK(strcmp(buf, "VEX: Customer satisfaction is our TOP priority!") == 0);
    /* a short buffer truncates, always terminated */
    CHECK_EQ(txt_get(TXT_OBJ_C1_MAYOR, buf, 8), 7);
    CHECK_STR(buf, "Say hi ");
    /* unknown ids */
    CHECK_EQ(txt_get(TXT_NONE, buf, (int)sizeof(buf)), -1);
    CHECK_STR(buf, "");
    CHECK_EQ(txt_get(TXT_ID(127, 0), buf, (int)sizeof(buf)), -1);
    CHECK_EQ(txt_get(TXT_ID(PACK_VMTEST, 500), buf, (int)sizeof(buf)), -1);
}

TEST(textpack_roundtrip_every_string_and_the_caps)
{
    int p, np = txt_pack_count(), strings = 0;
    for (p = 0; p < np; p++) {
        int i, n = txt_pack_strings(p), kind = txt_pack_kind(p);
        unsigned cap = kind == TXT_KIND_COMMON ? TXT_CAP_COMMON : kind == TXT_KIND_SIDE ? TXT_CAP_SIDE :
                       kind == TXT_KIND_FEAST ? TXT_CAP_FEAST : TXT_CAP_EPISODE;
        CHECK_MSG(doomo_txt_pack_bytes[p] <= cap, "pack %s is %u B, cap %u", doomo_txt_pack_names[p],
                  (unsigned)doomo_txt_pack_bytes[p], cap);
        CHECK(n >= 0 && n <= TXT_INDEX_MAX);
        for (i = 0; i < n; i++) {
            char buf[256];
            int len = txt_get((uint16_t)TXT_ID(p, i), buf, (int)sizeof(buf));
            CHECK_MSG(len >= 0 && len < 200, "pack %s string %d: %d", doomo_txt_pack_names[p], i, len);
            CHECK_EQ((int)strlen(buf), len);
            {
                int k;
                for (k = 0; k < len; k++)
                    CHECK_MSG((unsigned char)buf[k] >= 32 && (unsigned char)buf[k] < 0xA0,
                              "pack %s string %d: byte %d", doomo_txt_pack_names[p], i, (unsigned char)buf[k]);
            }
            strings++;
        }
    }
    CHECK(strings > 400);
}

/* the box (SF sec.15.6): every page of every non-legacy pack fits 3 lines
 * of the 240x160 box with the real wrap, and 4 at 320x240 */
TEST(story_text_fits_the_box)
{
    int p, np = txt_pack_count(), checked = 0;
    int w240 = 28 * FONT_ADVANCE;
    for (p = 0; p < np; p++) {
        int i, n = txt_pack_strings(p);
        if (txt_pack_flags(p) & 1) continue;                 /* legacy text pages itself */
        for (i = 0; i < n; i++) {
            char buf[256], page[256];
            const char *s = buf;
            txt_get((uint16_t)TXT_ID(p, i), buf, (int)sizeof(buf));
            while (*s) {
                int k = 0, lines = 0;
                const char *q = s;
                while (*q && !(q[0] == '/' && q[1] == '/')) {
                    if (*q == '{') { while (*q && *q != '}') q++; if (*q) q++; continue; }
                    if (*q == '/') {                       /* as ui_dialog.c: spaces around '/' go */
                        while (k > 0 && page[k - 1] == ' ') k--;
                        page[k++] = '\n';
                        q++;
                        while (*q == ' ') q++;
                        continue;
                    }
                    page[k++] = *q++;
                }
                page[k] = 0;
                {
                    const char *t = page;
                    while (*t == ' ') t++;
                    while (*t) {
                        int adv = ui_wrap_skip(t, w240, 1);
                        if (adv <= 0) break;
                        t += adv;
                        while (*t == ' ') t++;
                        lines++;
                    }
                }
                CHECK_MSG(lines <= 3, "pack %s string %d needs %d lines: %s", doomo_txt_pack_names[p], i, lines, page);
                checked++;
                s = *q ? q + 2 : q;
            }
        }
    }
    CHECK(checked >= 2);
}

TEST(textpack_beats_are_valid_scripts)
{
    BeatScript bs;
    CHECK(txt_beat(BSCR_VMT_BEAT, &bs));
    CHECK(beat_script_valid(&bs));
    CHECK(bs.n >= 5);
    CHECK_EQ(bs.ev[bs.n - 1].op, BOP_END);
    CHECK(!txt_beat(TXT_ID(PACK_VMTEST, 100), &bs));
}

TEST(textpack_stash_keeps_lines_for_a_pushed_scene)
{
    txt_stash(0, TXT_OBJ_C1_KEY);
    txt_stash(3, TXT_NONE);
    CHECK_STR(txt_stash_get(0), "Your key is with Nana at Brimstone Bites");
    CHECK_STR(txt_stash_get(3), "");
    CHECK_STR(txt_stash_get(9), "");
}
