/* test_story_lint.c - the story content lints (S2; SPEC sec.7 S2 acceptance,
 * sec.10.4, Appendix C; SF sec.18.1): the timing ladder in beat scripts,
 * the silence budget, the style rations and the length budget. They run
 * over every pack the build has; the chapter gates apply once a chapter
 * has content (an episode pack with declared minutes), so they hold for
 * the legacy port today and bite as S2-S5 content lands. Legacy packs
 * (lint=legacy) are exempt, as the SPEC's legacy port is. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/story_vm.h"
#include "doomo/txt.h"
#include "doomo/beat.h"
#include "../src/story_int.h"

static int linted(int p) { return !(txt_pack_flags(p) & 1); }

/* chapter c has content: an episode pack of that chapter with minutes */
static int chapter_minutes(int c)
{
    int p, m = 0;
    for (p = 0; p < txt_pack_count(); p++)
        if (linted(p) && doomo_txt_pack_kinds[p] == TXT_KIND_EPISODE && doomo_txt_pack_chapters[p] == c)
            m += doomo_txt_pack_minutes[p];
    return m;
}

/* the beat ladder 2/3/5/8/13/21/26, +-1 (comic_timing.md, SPEC R5) */
static int on_ladder(int gap)
{
    static const int rung[7] = { 2, 3, 5, 8, 13, 21, 26 };
    int i;
    for (i = 0; i < 7; i++) if (gap >= rung[i] - 1 && gap <= rung[i] + 1) return 1;
    return 0;
}

/* a label's time from the script start (WAIT_A anchors count as 0) */
static int label_time(const BeatScript *bs, int label, int depth)
{
    int i;
    if (label == 0 || depth > 8) return 0;
    for (i = 0; i < bs->n; i++) {
        const BeatEv *e = &bs->ev[i];
        if ((e->op == BOP_LABEL || e->op == BOP_WAIT_A || e->op == BOP_HOLD) && e->a == label)
            return label_time(bs, e->anchor, depth + 1) + e->at;
    }
    return -1;
}

/* SF sec.18.1 beat_script_timing: >= 75% of the gaps on the ladder, inserts
 * 3-4 frames, a button 12-25 frames after L_PUNCH or an explicit NOBUTTON */
TEST(beat_script_timing)
{
    int p, scripts = 0;
    for (p = 0; p < txt_pack_count(); p++) {
        int k, nb;
        if (!linted(p)) continue;
        nb = txt_pack_beats(p);
        for (k = 0; k < nb; k++) {
            BeatScript bs;
            int i, gaps = 0, ladder = 0, punch, button, nobutton = 0;
            CHECK(txt_beat((uint16_t)TXT_ID(p, k), &bs));
            for (i = 0; i < bs.n; i++) {
                const BeatEv *e = &bs.ev[i];
                if (i > 0 && bs.ev[i - 1].anchor == e->anchor && e->at > bs.ev[i - 1].at) {
                    gaps++;
                    ladder += on_ladder(e->at - bs.ev[i - 1].at);
                }
                if (e->op == BOP_INSERT)
                    CHECK_MSG(e->b >= 3 && e->b <= 4, "pack %s beat %d: an insert of %d frames", doomo_txt_pack_names[p], k, e->b);
                if (e->op == BOP_NOBUTTON) nobutton = 1;
            }
            CHECK_MSG(gaps == 0 || ladder * 4 >= gaps * 3, "pack %s beat %d: %d of %d gaps on the ladder",
                      doomo_txt_pack_names[p], k, ladder, gaps);
            punch = label_time(&bs, L_PUNCH, 0);
            button = label_time(&bs, L_BUTTON, 0);
            if (punch >= 0)
                CHECK_MSG(nobutton || (button >= 0 && button - punch >= 12 && button - punch <= 25),
                          "pack %s beat %d: L_BUTTON %d frames after L_PUNCH", doomo_txt_pack_names[p], k, button - punch);
            scripts++;
        }
    }
    CHECK(scripts >= 1);
}

/* frames a script takes and the frames of them without music (MUTE) */
static void script_frames(const StoryOp *o, int n, int *total, int *silent)
{
    int i, mute_left = 0, forever = 0;
    *total = *silent = 0;
    for (i = 0; i < n; i++) {
        int f = 0;
        if (o[i].op == OP_WAIT || o[i].op == OP_HOLD || o[i].op == OP_WAIT_A) f = o[i].a;
        else if (o[i].op == OP_MUTE) { if (o[i].b == 0xFFFFu) forever = 1; else mute_left = o[i].b; }
        else if (o[i].op == OP_MUSIC) forever = mute_left = 0;
        *total += f;
        if (forever) *silent += f;
        else if (mute_left) { int s = f < mute_left ? f : mute_left; *silent += s; mute_left -= s; }
    }
}

/* SF sec.18.1 silence_budget: 20-25% of a chapter's frames without music
 * (Ch1-2 for S2; Interlude III is exempt) */
TEST(silence_budget)
{
    int t, s, c, p, n0 = 0;
    const StoryOp *o0 = txt_script(SCR_VMT_SILENCE, &n0);
    CHECK(o0 != NULL);
    script_frames(o0, n0, &t, &s);
    CHECK_EQ(t, 15);
    CHECK_EQ(s, 5);
    for (c = 1; c <= 4; c++) {
        int total = 0, silent = 0;
        if (!chapter_minutes(c)) continue;
        for (p = 0; p < txt_pack_count(); p++) {
            int k, ns;
            if (!linted(p) || doomo_txt_pack_chapters[p] != c || doomo_txt_pack_kinds[p] != TXT_KIND_EPISODE) continue;
            if (strcmp(doomo_txt_pack_names[p], "int3") == 0) continue;
            ns = txt_pack_scripts(p);
            for (k = 1; k < ns; k++) {
                int n;
                const StoryOp *o = txt_script((uint16_t)TXT_ID(p, k), &n);
                script_frames(o, n, &t, &s);
                total += t;
                silent += s;
            }
        }
        if (total)
            CHECK_MSG(silent * 100 >= total * 20 && silent * 100 <= total * 25,
                      "chapter %d: %d of %d frames silent", c, silent, total);
    }
}

static int count_sub(const char *s, const char *sub)
{
    int n = 0, k = (int)strlen(sub);
    for (; (s = strstr(s, sub)) != NULL; s += k) n++;
    return n;
}

/* SF sec.18.1 story_style_rations: <= 1 {d} per conversation and < 80 in
 * the game; "Ten years I" only in its four packs, at most once each;
 * NOBUTTON on at most half of a chapter's sincere (HEART) scripts */
TEST(story_style_rations)
{
    int p, deadpans = 0, ten = 0;
    int sincere[6] = { 0 }, nobutton[6] = { 0 };
    for (p = 0; p < txt_pack_count(); p++) {
        int k, ns, c = doomo_txt_pack_chapters[p];
        if (!linted(p)) continue;
        ns = txt_pack_scripts(p);
        for (k = 0; k < ns; k++) {
            int n, i, d = 0, heart = 0, nob = 0;
            const StoryOp *o = txt_script((uint16_t)TXT_ID(p, k), &n);
            for (i = 0; i < n; i++) {
                char buf[256];
                if (o[i].op == OP_SAY || o[i].op == OP_SAY_X) {
                    if (txt_get(o[i].b, buf, (int)sizeof(buf)) >= 0) {
                        d += count_sub(buf, "{d}");
                        ten += count_sub(buf, "Ten years I");
                    }
                    if (i + 1 < n && o[i + 1].op == OP_DATA && o[i + 1].a == VMD_LINE) {
                        if (o[i + 1].b & DL_DEADPAN) d++;
                        if (o[i + 1].b & DL_HEART) heart = 1;
                    }
                }
                if (o[i].op == OP_BEAT) {
                    BeatScript bs;
                    int e;
                    if (txt_beat(o[i].b, &bs))
                        for (e = 0; e < bs.n; e++) if (bs.ev[e].op == BOP_NOBUTTON) nob = 1;
                }
            }
            CHECK_MSG(d <= 1, "pack %s script %d: %d deadpans in one conversation", doomo_txt_pack_names[p], k, d);
            deadpans += d;
            if (heart && c >= 0 && c < 6) { sincere[c]++; nobutton[c] += nob; }
        }
    }
    CHECK(deadpans < 80);
    CHECK(ten <= 4);
    {
        int c;
        for (c = 1; c < 6; c++)
            CHECK_MSG(nobutton[c] * 2 <= sincere[c], "chapter %d: NOBUTTON on %d of %d sincere scripts", c,
                      nobutton[c], sincere[c]);
    }
}

/* the new story_length_budget (SPEC S2): each chapter's declared minutes
 * within +-10% of SF sec.8 once the chapter has content */
TEST(story_length_budget)
{
    static const int target[5] = { 0, 150, 180, 0, 0 };   /* S2's chapters; S3 adds Ch3-4 */
    int c;
    for (c = 1; c <= 2; c++) {
        int m = chapter_minutes(c);
        if (!m) continue;
        CHECK_MSG(m * 10 >= target[c] * 9 && m * 10 <= target[c] * 11, "chapter %d declares %d minutes (SF: %d)",
                  c, m, target[c]);
    }
}
