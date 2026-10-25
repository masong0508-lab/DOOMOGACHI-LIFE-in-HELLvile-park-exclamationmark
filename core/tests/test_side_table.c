/* test_side_table.c - the table-wide content checks of the side missions
 * (owner: S5; docs/design/story/SPEC.md sec.7 S5 acceptance; story_final.md
 * sec.14 "design rules for all 109"): 118 rows, 109 live, the retired ones
 * empty; the chapter counts 17/19/18/32/23; every verb used; the givers of
 * both towns; chains monotone and finishable under the S2 gate (grafts and
 * the post-game exempt from one-part-per-chapter); the rows fit STRY.side2;
 * no chain withholds its button twice in a row; exactly four news-ticker
 * Laters (S140, S156, S168, S191); Grumbleguts' LOVE lines never in a
 * child's conversation; and S186, the Feast, a row only until its package.
 * The withheld buttons, the ticker and the speakers are source markers the
 * compiler drops, so those checks read core/data/story/side_?/ *.dsc. */
#include "test.h"
#include "../src/story_int.h"
#include "doomo/roster.h"
#include <stdio.h>
#include <stdlib.h>

static const char *const side_files[] = {
    "side_a/a_pip", "side_a/b_rivet", "side_a/c_mayor", "side_a/d_nana", "side_a/e_towers", "side_a/f_fuzz",
    "side_a/g_party", "side_a/h_mascot", "side_a/i_solo",
    "side_b/k_vex", "side_b/m_bloom", "side_b/o_lulla", "side_b/q_tock", "side_b/r_wisp", "side_b/l_vapula",
    "side_b/t_gate", "side_b/x_solo1", "side_b/x_solo2", "side_b/x_solo3", NULL };

static char *load(const char *name)
{
    char path[700];
    FILE *f;
    long n;
    char *t;
    snprintf(path, sizeof(path), "%s/../../data/story/%s.dsc", test_golden_dir, name);
    f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    t = (char *)malloc((size_t)n + 1);
    if (t && fread(t, 1, (size_t)n, f) != (size_t)n) n = 0;
    if (t) t[n] = 0;
    fclose(f);
    return t;
}

/* per mission, from its sources: withholds its button, runs the ticker */
static uint8_t withholds[STORY_SIDE2_ROWS], ticker[STORY_SIDE2_ROWS], seen[STORY_SIDE2_ROWS];
static int love_in_kid_talk, love_lines, sources;

static int kid(int id) { return id >= 0 && (roster_traits(id) & (ROSTER_T_KID | ROSTER_T_TEEN)) != 0; }

/* the resident a `say` / `choice` speaker word names (-1 narrator / none) */
static int speaker(const char *w)
{
    static const struct { const char *n; int id; } cast[] = {
        { "pip", ROSTER_ID_PIP }, { "mayor", 0 }, { "nana", 1 }, { "rivet", 13 }, { "fuzz", 77 }, { "bloom", 111 },
        { "vex", 666 }, { "lulla", 36 }, { "nox", 778 }, { "smog", 502 }, { "haze", 709 }, { "fizz", 69 },
        { "tallyworn", 712 }, { "cantor", 426 }, { "grumble", ROSTER_ID_GRUMBLEGUTS }, { "ID_WISP", 563 },
        { "ID_TALLYWORN", 712 }, { "ID_PIP", ROSTER_ID_PIP } };
    unsigned k;
    if (*w >= '0' && *w <= '9') return atoi(w);
    for (k = 0; k < sizeof(cast) / sizeof(cast[0]); k++) {
        size_t n = strlen(cast[k].n);
        if (!strncmp(w, cast[k].n, n) && (w[n] == ' ' || w[n] == '\t')) return cast[k].id;
    }
    return -1;
}

static void scan_sources(void)
{
    int f;
    memset(withholds, 0, sizeof(withholds));
    memset(ticker, 0, sizeof(ticker));
    memset(seen, 0, sizeof(seen));
    love_in_kid_talk = love_lines = sources = 0;
    for (f = 0; side_files[f]; f++) {
        char *t = load(side_files[f]), *p, *line;
        int sid = -1, kid_here = 0, love_here = 0, marked = 0, ends = -1;
        if (!t) continue;
        sources++;
        for (line = t; line && *line; line = p) {
            char *s = line;
            p = strchr(line, '\n');
            if (p) *p++ = 0;
            while (*s == ' ' || *s == '\t') s++;
            if (!strncmp(s, "@mission S", 10)) {
                int m = atoi(s + 10);
                sid = m >= 100 && m < 100 + STORY_SIDE2_ROWS ? m - 100 : -1;
                if (sid >= 0) seen[sid] = 1;
                continue;
            }
            if (!strncmp(s, "script ", 7)) {          /* a conversation of its own */
                love_in_kid_talk += love_here && kid_here;
                if (ends >= 0) withholds[ends] |= (uint8_t)marked;   /* the ending: 1 NO BUTTON, 2 NO GAG */
                kid_here = love_here = marked = 0;
                ends = -1;
                continue;
            }
            if (sid < 0) continue;
            marked |= (strstr(s, "[NO BUTTON]") ? 1 : 0) | (strstr(s, "[NO GAG]") ? 2 : 0);
            if (!strncmp(s, "quest S", 7) && strstr(s, " done")) {
                int m = atoi(s + 7);
                if (m >= 100 && m < 100 + STORY_SIDE2_ROWS) ends = m - 100;
            }
            if (strstr(s, "native CB_SIDE_NEWS")) ticker[sid]++;
            if (!strncmp(s, "say ", 4) || !strncmp(s, "choice ", 7)) {
                int who = speaker(s + (s[0] == 's' ? 4 : 7));
                if (kid(who)) kid_here = 1;
                if (!strncmp(s, "say grumble LOVE", 16)) { love_here = 1; love_lines++; }
            }
        }
        love_in_kid_talk += love_here && kid_here;
        if (ends >= 0) withholds[ends] |= (uint8_t)marked;
        free(t);
    }
}

TEST(side_table_rows_counts_and_verbs)
{
    int i, live = 0, retired = 0, ch[6] = { 0 }, hill = 0, hell = 0;
    unsigned verbs = 0;
    town_new(TOWN_STORY, 118, TOWN_POP_PREMADE);
    CHECK_EQ(STORY_SIDE2_ROWS, 118);
    CHECK(STORY_SIDE2_ROWS * 2 <= (int)sizeof(story_stry()->side2) * 8);   /* 2 bits a row in STRY */
    for (i = 0; i < STORY_SIDE2_ROWS; i++) {
        const StorySideRow *r = &doomo_side_rows[i];
        if (!r->kind) {                                    /* retired: KIND_NONE, nothing to run */
            retired++;
            CHECK_MSG(r->offer == 0xFFFFu && r->active == 0xFFFFu && r->title == 0xFFFFu && !r->chain,
                      "retired S%d carries content", 100 + i);
            continue;
        }
        live++;
        CHECK(r->ch_lo >= 1 && r->ch_lo <= 5);
        ch[r->ch_lo]++;
        verbs |= r->verbs;
        if (RES_DISTRICT(r->giver)) hell++; else hill++;
    }
    CHECK_EQ(live, 109);
    CHECK_EQ(retired, 9);
    CHECK_MSG(ch[1] == 17 && ch[2] == 19 && ch[3] == 18 && ch[4] == 32 && ch[5] == 23,
              "chapter counts %d/%d/%d/%d/%d", ch[1], ch[2], ch[3], ch[4], ch[5]);
    CHECK_MSG(verbs == 0xFFFFu, "verbs used 0x%x", verbs);   /* fetch .. timing, every one */
    CHECK_MSG(hill >= 45 && hell >= 35, "givers: %d Hillvile, %d Hellvile", hill, hell);
}

/* steps 1..n, the chapters never go backwards, and every chain finishes
 * under the S2 gate, best case, in five chapters */
TEST(side_table_chains_monotone_and_finishable)
{
    uint8_t side2[30], done_ch[26];
    int c, i, ch, pass, n = 0, live = 0;
    for (c = 'A'; c <= 'Z'; c++) {
        int k, steps = 0, last_lo = 0;
        for (k = 1; k <= 8; k++) {
            int hits = 0;
            for (i = 0; i < STORY_SIDE2_ROWS; i++) {
                const StorySideRow *r = &doomo_side_rows[i];
                if (!r->kind || r->chain != c || r->step != k) continue;
                hits++;
                CHECK_MSG(r->ch_lo >= last_lo, "chain %c step %d starts in Ch%d before step %d", c, k, r->ch_lo, k - 1);
                last_lo = r->ch_lo;
            }
            CHECK_MSG(hits <= 1, "chain %c has %d parts at step %d", c, hits, k);
            if (hits) steps = k;
            else break;
        }
        for (i = 0; i < STORY_SIDE2_ROWS; i++)
            if (doomo_side_rows[i].kind && doomo_side_rows[i].chain == c)
                CHECK_MSG(doomo_side_rows[i].step <= steps, "chain %c: a gap before step %d", c, doomo_side_rows[i].step);
    }
    memset(side2, 0, sizeof(side2));
    memset(done_ch, 0, sizeof(done_ch));
    for (i = 0; i < STORY_SIDE2_ROWS; i++) live += doomo_side_rows[i].kind != 0;
    for (ch = 1; ch <= 5; ch++)
        for (pass = 0; pass < 10; pass++)
            for (i = 0; i < STORY_SIDE2_ROWS; i++) {
                const StorySideRow *r = &doomo_side_rows[i];
                if (!story_side2_gate(doomo_side_rows, i, side2, ch, ch >= 4, done_ch)) continue;
                side2[i >> 2] = (uint8_t)(side2[i >> 2] | (3u << ((i & 3) * 2)));
                if (r->chain && i < STORY_SIDE2_GRAFT_FIRST) done_ch[r->chain - 'A'] = (uint8_t)ch;
                n++;
            }
    CHECK_EQ(n, live);
    /* the post-game exemption: two parts of one chain in Chapter 5 */
    memset(done_ch, 0, sizeof(done_ch));
    done_ch['L' - 'A'] = 5;
    memset(side2, 0, sizeof(side2));
    for (i = 0; i < STORY_SIDE2_ROWS; i++)
        if (doomo_side_rows[i].chain == 'L' && doomo_side_rows[i].step == 1)
            side2[i >> 2] = (uint8_t)(side2[i >> 2] | (3u << ((i & 3) * 2)));
    for (i = 0; i < STORY_SIDE2_ROWS; i++)
        if (doomo_side_rows[i].kind && doomo_side_rows[i].chain == 'L' && doomo_side_rows[i].step == 2) {
            CHECK(story_side2_gate(doomo_side_rows, i, side2, 5, 1, done_ch));
            CHECK(!story_side2_gate(doomo_side_rows, i, side2, 4, 1, done_ch) || doomo_side_rows[i].ch_lo > 4);
        }
}

TEST(side_table_buttons_ticker_and_kids)
{
    int c, i, news = 0;
    scan_sources();
    CHECK_EQ(sources, 19);
    for (i = 0; i < STORY_SIDE2_ROWS; i++)
        CHECK_MSG(!doomo_side_rows[i].kind || seen[i], "S%d has no @mission in side_a/ side_b/", 100 + i);
    /* a chain never withholds its button twice in a row */
    for (c = 'A'; c <= 'Z'; c++) {
        int k, prev = 0;
        for (k = 1; k <= 8; k++) {
            int cur = -1;
            for (i = 0; i < STORY_SIDE2_ROWS; i++)
                if (doomo_side_rows[i].kind && doomo_side_rows[i].chain == c && doomo_side_rows[i].step == k) cur = i;
            if (cur < 0) break;
            CHECK_MSG(!(prev & withholds[cur]), "chain %c withholds twice in a row (step %d: %d after %d)", c, k, withholds[cur], prev);
            prev = withholds[cur];
        }
    }
    /* about a quarter of the missions end on a withheld button, never half */
    for (i = 0, c = 0; i < STORY_SIDE2_ROWS; i++) c += withholds[i] != 0;
    CHECK_MSG(c >= 15 && c <= 54, "%d missions withhold their button", c);
    /* exactly four news-ticker Laters */
    for (i = 0; i < STORY_SIDE2_ROWS; i++) news += ticker[i];
    CHECK_EQ(news, 4);
    CHECK(ticker[40] && ticker[56] && ticker[68] && ticker[91]);
    /* Grumbleguts flirts with adults only */
    CHECK(love_lines >= 10);
    CHECK_EQ(love_in_kid_talk, 0);
    /* S186, the Feast: a live row with no scripts until its package lands */
    CHECK(doomo_side_rows[86].kind && doomo_side_rows[86].offer == 0xFFFFu && !doomo_side_rows[86].spot);
}
