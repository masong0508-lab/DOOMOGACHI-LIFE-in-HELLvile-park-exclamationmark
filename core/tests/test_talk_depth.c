/* test_talk_depth.c - TM2d: topic depth, hooks and the unknown word
 * (core/src/talk_plan.c, talk_gen.c, talk_dm.c, talk_lex.c, talk_parse.c, talk_save.c, talk_outcome.c;
 * docs/TALK.md 6n; design docs/design/talk/TALK_MULTILEVEL.md 2.3 to 2.5, 3, 4, 13 TM2d).
 * Owner: TALK. Each test fails without the code it covers.
 *
 *   depth_pure_gate_and_step        the gates and the rises, falls and resets as pure functions
 *   depth_gates                     a stranger never reaches 2, a rival never past 1, a friend needs a bond of 40 for 3,
 *                                   a best friend with 3 turns and engagement 70 tells exactly one real secret
 *   depth_resets                    another root resets, an insult lowers, a refusal and a walk-out reset, the memory
 *                                   keeps the level (the next talk starts one lower) and the secrets of the day
 *   depth_never_adult_for_kids      2,000 sessions with children: every depth 2 and 3 line is kid-safe, no secret is a
 *                                   romance, a body or a harm; no unsafe problem is ever told (stage x PROB_*)
 *   depth_dm_secret_learned         the drama manager: the second line, TEV_LEARNED, the sim applies it (likes known)
 *   unknown_word_named              200 invented words: a guess or the word named back, quoted, lower case, once a talk
 *   unknown_word_never_echoed_when_unsafe
 *                                   the adversarial and the echo corpus: no hate, register, minor, leet, fuzzy, person,
 *                                   digit or shouted word is ever said back; a kid room says no word over 12 letters
 *   unknown_word_taught             "it's a food": the kind is taken, remembered for the talk, never saved
 *   hook_after_counter_and_refuse   every counter and every refused request leaves a hook; no hook after a sincere line
 *   lex_near_guess                  talk_lex_near finds a real concept two edits away and flags words one edit from a
 *                                   register word
 *   parse_unknown_offsets           talk_parse points at the first unknown word and nothing it may not say
 */
#include <stdlib.h>
#include "test.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/talk_traits.h"
#include "doomo/lang.h"
#include "doomo/sim.h"
#include "doomo/resident_acc.h"

static void premade_town(void)
{
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
}

static int open_en(void) { return talk_db_open(lang_find("en")) == TALK_DB_OK && talk_gen_ready(); }

static uint16_t concept(const char *key)
{
    uint16_t c = 0, wf = 0;
    uint32_t tags[4];
    if (talk_lex_find(key, &c, &wf, tags) < 0) return 0;
    return c;
}

static uint32_t rs = 0x2545F491u;
static uint32_t rnd(uint32_t n) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return n ? rs % n : 0; }

static int adults[40], nadult, kids[8], nkid, teens[8], nteen;
static void people(void)
{
    int id;
    nadult = nkid = nteen = 0;
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
        if (town_res(id)->status != RES_ALIVE || (town_res(id)->flags & RF_CUSTOM)) continue;
        if (sim_life_stage(id) == LIFE_TEEN) { if (nteen < 8) teens[nteen++] = id; }
        else if (talk_safe_minor(id)) { if (nkid < 8) kids[nkid++] = id; }
        else if (id < DOOMO_PLAYER_ID && nadult < 40) adults[nadult++] = id;
    }
}

/* the bond of resident id with the player: a kind and a value (kind -1: no edge at all) */
static void set_rel(int id, int kind, int value)
{
    DoomoEdge *e;
    e = rel_get_or_add(DOOMO_PLAYER_ID, id);
    if (e) { e->kind = (uint8_t)kind; e->value = (int8_t)value; }
    e = rel_get_or_add(id, DOOMO_PLAYER_ID);
    if (e) { e->kind = (uint8_t)kind; e->value = (int8_t)value; }
}

/* ---- a walk: one resident, one topic, `turns` answers, the depth after each, the second lines ---------- */
typedef struct Walk {
    int lvl[10];                  /* the level after each turn */
    int more;                     /* second lines made */
    int secrets;                  /* ... that were secrets */
    int stories;
    char secret_text[TALK_LINE_MAX];
    TalkConv c;
} Walk;

static int lower_has(const char *text, const char *w)
{
    char a[TALK_LINE_MAX], b[64];
    int i;
    for (i = 0; text[i] && i < TALK_LINE_MAX - 1; i++) a[i] = (char)tolower((unsigned char)text[i]);
    a[i] = 0;
    for (i = 0; w[i] && i < 63; i++) b[i] = (char)tolower((unsigned char)w[i]);
    b[i] = 0;
    return b[0] && strstr(a, b) != NULL;
}

/* which of the five likes (food, hobby, music, fear, hated food) the text names, 1..5, 0 none */
static int names_like(int id, const char *text)
{
    int w;
    for (w = 0; w < 5; w++) {
        char f[48];
        uint16_t cid = talk_gen_like(id, w);
        if (cid && talk_db_form(cid, TF_LEMMA, f, (int)sizeof f) > 0 && lower_has(text, f)) return w + 1;
    }
    return 0;
}

static void walk(Walk *w, int id, int turns, int act, uint16_t topic, int engage, uint32_t seed, const int *with, int nwith)
{
    int ids[4], n = 0, t;
    ids[n++] = id;
    while (nwith-- > 0 && n < 4) ids[n++] = *with++;
    memset(w, 0, sizeof *w);
    talk_gen_begin(&w->c, DOOMO_PLAYER_ID, ids, n, seed);
    for (t = 0; t < turns && t < 10; t++) {
        TalkLine l, m;
        TalkTrace tr, tm;
        int kind;
        memset(&tr, 0, sizeof tr);
        w->c.engage = (uint8_t)engage;
        w->c.turn++;
        talk_gen_line(&w->c, 0, -1, act, topic, &l, &tr);
        w->lvl[t] = (int)TDEP_LEVEL(w->c.dep[0]);
        kind = (int)(w->c.more & 3u);
        if (w->c.more && talk_gen_more(&w->c, &m, &tm)) {
            w->more++;
            CHECK(m.flags & TLF_MORE);
            CHECK(m.text[0]);
            if (kind == TMORE_SECRET) { w->secrets++; strcpy(w->secret_text, m.text); }
            else w->stories++;
        }
    }
}

/* ---- pure ------------------------------------------------------------------------------------------- */
static void din(TalkDepthIn *in, int rel, int engage, int level, int act)
{
    memset(in, 0, sizeof *in);
    in->act = (uint8_t)act;
    in->rel = (uint8_t)rel;
    in->engage = (uint8_t)engage;
    in->level = (uint8_t)level;
    in->stage = LIFE_ADULT;
}

TEST(depth_pure_gate_and_step)
{
    TalkDepthIn in;
    uint8_t b = 0;
    uint16_t tw = 0;
    int t, lv[8];
    /* the gate */
    din(&in, TRB_STRANGER, 100, 100, TA_ASK_OPINION);
    CHECK_EQ(talk_depth_gate(&in, 7), 1);                       /* a stranger never reaches 2 */
    din(&in, TRB_RIVAL, 100, 100, TA_ASK_OPINION);
    CHECK_EQ(talk_depth_gate(&in, 7), 1);                       /* a rival: a jab, never deeper */
    din(&in, TRB_ACQUAINTANCE, 49, 20, TA_ASK_OPINION);
    CHECK_EQ(talk_depth_gate(&in, 7), 1);                       /* engagement 50 for a story */
    in.engage = 50;
    CHECK_EQ(talk_depth_gate(&in, 1), 1);                       /* 2 turns on the thread */
    CHECK_EQ(talk_depth_gate(&in, 2), 2);
    in.engage = 100;
    CHECK_EQ(talk_depth_gate(&in, 7), 2);                       /* an acquaintance never tells a secret */
    din(&in, TRB_FRIEND, 100, 39, TA_ASK_OPINION);
    CHECK_EQ(talk_depth_gate(&in, 7), 2);                       /* a friend with a bond of 39 */
    in.level = 40;
    CHECK_EQ(talk_depth_gate(&in, 7), 3);
    in.engage = 69;
    CHECK_EQ(talk_depth_gate(&in, 7), 2);                       /* engagement 70 */
    in.engage = 100;
    CHECK_EQ(talk_depth_gate(&in, 2), 2);                       /* 3 turns */
    in.level = 0;
    in.loyal = 1;
    CHECK_EQ(talk_depth_gate(&in, 3), 3);                       /* a loyal friend needs no bond */
    din(&in, TRB_BEST, 100, 0, TA_ASK_OPINION);
    in.gloomy = 1;
    CHECK_EQ(talk_depth_gate(&in, 3), 3);                       /* gloomy raises the story gate, not the secret gate */
    in.gloomy = 0;
    in.grief = 1;
    CHECK_EQ(talk_depth_gate(&in, 7), 0);                       /* grief is never mined */
    in.grief = 0;
    in.shy = 1;
    CHECK_EQ(talk_depth_gate(&in, 7), 3);                       /* a shy best friend is still a best friend */
    din(&in, TRB_STRANGER, 100, 0, TA_ASK_OPINION);
    in.shy = 1;
    CHECK_EQ(talk_depth_gate(&in, 7), 0);                       /* a shy stranger says nothing deep */
    din(&in, TRB_ACQUAINTANCE, 100, 0, TA_ASK_OPINION);
    in.shy = 1;
    CHECK_EQ(talk_depth_gate(&in, 7), 1);                       /* shy: one band more for the story */
    in.shy = 0;
    in.gloomy = 1;
    CHECK_EQ(talk_depth_gate(&in, 7), 1);
    in.gloomy = 0;
    in.outgoing = 1;
    CHECK_EQ(talk_depth_gate(&in, 7), 2);
    /* the steps: a best friend on one topic rises one level a turn */
    din(&in, TRB_BEST, 100, 90, TA_ASK_OPINION);
    in.root = TCR_FOOD; in.same = 0; in.thread_root = 0;
    for (t = 0; t < 6; t++) {
        lv[t] = talk_depth_step(&in, &b, &tw, 77);
        in.same = 1; in.thread_root = TCR_FOOD;
    }
    CHECK(lv[0] == 0 && lv[1] == 1 && lv[2] == 2 && lv[3] == 3 && lv[4] == 3 && lv[5] == 3);
    CHECK_EQ((int)TDEP_TOPIC(tw), 77);
    /* an insult: down one; the thread stays */
    in.act = TA_INSULT;
    talk_depth_step(&in, &b, &tw, 77);
    CHECK_EQ((int)TDEP_LEVEL(b), 2);
    in.act = TA_MOCK; talk_depth_step(&in, &b, &tw, 77);
    CHECK_EQ((int)TDEP_LEVEL(b), 1);
    in.act = TA_DISMISS; talk_depth_step(&in, &b, &tw, 77);
    CHECK_EQ((int)TDEP_LEVEL(b), 0);
    in.act = TA_THREATEN; talk_depth_step(&in, &b, &tw, 77);
    CHECK_EQ((int)TDEP_LEVEL(b), 0);
    /* back up, then a greeting holds, a topic of another root resets */
    in.act = TA_ASK_OPINION;
    CHECK_EQ(talk_depth_step(&in, &b, &tw, 77), 1);
    in.act = TA_GREET;
    CHECK_EQ(talk_depth_step(&in, &b, &tw, 0), 0);
    CHECK_EQ((int)TDEP_LEVEL(b), 1);                            /* nothing moved */
    in.act = TA_ASK_OPINION; in.root = TCR_MUSIC; in.same = 0; in.thread_root = TCR_FOOD;
    CHECK_EQ(talk_depth_step(&in, &b, &tw, 88), 0);
    CHECK_EQ((int)TDEP_LEVEL(b), 0);
    CHECK_EQ((int)TDEP_TOPIC(tw), 88);
    /* a change of topic inside the root holds the level */
    in.same = 1; in.thread_root = TCR_MUSIC;
    talk_depth_step(&in, &b, &tw, 88);
    talk_depth_step(&in, &b, &tw, 88);
    CHECK_EQ((int)TDEP_LEVEL(b), 2);
    in.act = TA_CHANGE_TOPIC; in.same = 0;
    CHECK_EQ(talk_depth_step(&in, &b, &tw, 89), 0);
    CHECK_EQ((int)TDEP_LEVEL(b), 2);                            /* held */
    CHECK_EQ((int)TDEP_TOPIC(tw), 89);
    talk_depth_reset(&b);                                       /* a walk-out, a K-5 refusal */
    CHECK_EQ((int)TDEP_LEVEL(b), 0);
    /* the hook rule */
    CHECK_EQ(talk_hook_wanted(TA_ASK_ABOUT, TK_COUNTER, 0, 0, 0), TH_ASK);
    CHECK_EQ(talk_hook_wanted(TA_INVITE, TK_REFUSE, 0, 0, 0), TH_OFFER);
    CHECK_EQ(talk_hook_wanted(TA_ASK_FAVOR, TK_DENY, 0, 1, 0), TH_OFFER);
    CHECK_EQ(talk_hook_wanted(TA_ASK_FAVOR, TK_ACCEPT, 0, 0, 0), TH_NONE);
    CHECK_EQ(talk_hook_wanted(TA_ASK_ABOUT, TK_COUNTER, 0, 0, 1), TH_NONE);     /* a sincere line takes no hook */
    CHECK_EQ(talk_hook_wanted(TA_FLIRT, TK_COUNTER, 1, 0, 0), TH_NONE);         /* a romance answer is the target's alone */
    CHECK_EQ(talk_hook_wanted(TA_DISTRESS, TK_COUNTER, 0, 0, 0), TH_NONE);
}

/* ---- the engine: the gates ------------------------------------------------------------------------------- */
static int peak(const Walk *w, int turns)
{
    int t, p = 0;
    for (t = 0; t < turns; t++) if (w->lvl[t] > p) p = w->lvl[t];
    return p;
}

TEST(depth_gates)
{
    Walk w;
    uint16_t pie;
    int id, i, strangers = 0, tested;
    premade_town();
    CHECK(open_en());
    people();
    CHECK(nadult >= 10);
    pie = concept("pie");
    CHECK(pie != 0);
    id = adults[0];
    /* a best friend: 0 1 2 3 on the first four turns, and exactly one secret, a real unknown like or fear */
    set_rel(id, REL_BEST_FRIEND, 85);
    walk(&w, id, 8, TA_ASK_OPINION, pie, 75, 4242u, NULL, 0);
    CHECK(w.lvl[0] == 0 && w.lvl[1] == 1 && w.lvl[2] == 2 && w.lvl[3] == 3);
    CHECK_EQ(peak(&w, 8), 3);
    CHECK_EQ(w.secrets, 1);                                     /* exactly one, once a talk */
    CHECK(w.stories >= 1 || w.secrets == 1);
    {
        int which = names_like(id, w.secret_text);
        CHECK_MSG(which > 0 || strncmp(w.secret_text, "Can I tell you something?", 25) == 0, "secret: %s", w.secret_text);
        if (which > 0) {
            CHECK(!talk_mem_like_known(id, which - 1));          /* something the player does not know yet */
            CHECK(w.c.mem_dep[0] & (TDEP_MEM_SECRET(TSEC_LIKE) | TDEP_MEM_SECRET(TSEC_FEAR) | TDEP_MEM_SECRET(TSEC_HATE)));
        }
    }
    /* the secret is marked: the same day, the next talk tells none (the talk memory keeps it) */
    w.c.spoke = 1;
    talk_mem_end(&w.c);
    {
        int ids[1];
        TalkConv c;
        ids[0] = id;
        talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 1, 4244u);
        talk_mem_begin(&c);
        CHECK(c.mem_dep[0] & 0x3Cu);                                         /* the day's secret is remembered */
    }
    /* the next day another may be told */
    doomo_town.day++;
    {
        TalkConv c;
        int ids[1];
        ids[0] = id;
        talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 1, 4245u);
        talk_mem_begin(&c);
        CHECK_EQ((int)(c.mem_dep[0] & 0x3Cu), 0);
    }
    talk_mem_reset();
    /* a friend: 39 never 3, 40 does; a loyal friend does with a bond of 0 */
    set_rel(id, REL_FRIEND, 39);
    walk(&w, id, 8, TA_ASK_OPINION, pie, 90, 7u, NULL, 0);
    CHECK_MSG(peak(&w, 8) == 2 && w.secrets == 0, "friend 39: peak %d, secrets %d", peak(&w, 8), w.secrets);
    set_rel(id, REL_FRIEND, 40);
    walk(&w, id, 8, TA_ASK_OPINION, pie, 90, 8u, NULL, 0);
    CHECK_MSG(peak(&w, 8) == 3, "friend 40: peak %d", peak(&w, 8));
    /* engagement 69: never 3; engagement 70: 3 (a best friend) */
    set_rel(id, REL_BEST_FRIEND, 85);
    walk(&w, id, 8, TA_ASK_OPINION, pie, 69, 9u, NULL, 0);
    CHECK_EQ(peak(&w, 8), 2);
    walk(&w, id, 8, TA_ASK_OPINION, pie, 70, 9u, NULL, 0);
    CHECK_EQ(peak(&w, 8), 3);
    /* an acquaintance: a story, never a secret */
    set_rel(id, REL_ACQUAINTANCE, 15);
    walk(&w, id, 8, TA_ASK_OPINION, pie, 90, 10u, NULL, 0);
    CHECK_EQ(peak(&w, 8), 2);
    CHECK_EQ(w.secrets, 0);
    /* a rival: never past 1 */
    set_rel(id, REL_RIVAL, -50);
    walk(&w, id, 8, TA_ASK_OPINION, pie, 100, 11u, NULL, 0);
    CHECK_EQ(peak(&w, 8), 1);
    CHECK_EQ(w.more, 0);
    /* strangers: never 2, over every adult who is one */
    tested = 0;
    for (i = 0; i < nadult; i++) {
        int a = adults[i];
        if (rel_kind(a, DOOMO_PLAYER_ID) >= 0 || rel_value(a, DOOMO_PLAYER_ID) > 0) continue;
        strangers++;
        if (tested++ > 12) break;
        walk(&w, a, 8, TA_ASK_OPINION, pie, 100, 100u + (uint32_t)a, NULL, 0);
        CHECK_MSG(peak(&w, 8) <= 1 && w.more == 0, "stranger %d reached %d", a, peak(&w, 8));
    }
    CHECK(strangers > 0);
    talk_db_close();
}

TEST(depth_resets)
{
    Walk w;
    uint16_t pie, tune;
    int id;
    TalkLine l;
    TalkTrace tr;
    TalkConv c;
    int ids[1];
    premade_town();
    CHECK(open_en());
    people();
    pie = concept("pie");
    tune = concept("jazz");
    if (!tune) tune = concept("music");
    CHECK(pie && tune);
    id = adults[1];
    set_rel(id, REL_BEST_FRIEND, 85);
    /* a topic of another root resets to 0 */
    walk(&w, id, 3, TA_ASK_OPINION, pie, 80, 21u, NULL, 0);
    CHECK_EQ(w.lvl[2], 2);
    c = w.c;
    c.engage = 80;
    memset(&tr, 0, sizeof tr);
    talk_gen_line(&c, 0, -1, TA_ASK_OPINION, tune, &l, &tr);
    CHECK_EQ((int)TDEP_LEVEL(c.dep[0]), 0);
    /* an insult lowers by one (and the thread stays) */
    c = w.c;
    c.engage = 80;
    memset(&tr, 0, sizeof tr);
    talk_gen_line(&c, 0, -1, TA_INSULT, pie, &l, &tr);
    CHECK_EQ((int)TDEP_LEVEL(c.dep[0]), 1);
    /* a K-5 refusal in the drama manager: back to 0 (a romance act refused by the gate) */
    {
        TalkState s;
        TalkUtter u;
        int dm_ids[2];
        dm_ids[0] = adults[2];
        dm_ids[1] = kids[0];
        talk_dm_begin(&s, DOOMO_PLAYER_ID, dm_ids, 2, 31u);
        s.conv.dep[0] = 2;
        memset(&u, 0, sizeof u);
        u.act[0] = TA_FLIRT;
        u.target = 0;
        u.conf = 15;
        talk_dm_say(&s, &u);
        while (talk_dm_step(&s, NULL)) {}
        CHECK_EQ((int)TDEP_LEVEL(s.conv.dep[0]), 0);
    }
    /* the memory keeps the level: after a save and a load the next talk starts one lower; the secrets of the day too */
    talk_mem_reset();
    ids[0] = id;
    walk(&w, id, 5, TA_ASK_OPINION, pie, 80, 22u, NULL, 0);
    CHECK_EQ(peak(&w, 5), 3);
    w.c.spoke = 1;
    talk_mem_note_topic(id, pie);
    talk_mem_end(&w.c);
    CHECK_EQ(talk_mem_find(id)->spare & 3, 3);
    {
        uint8_t buf[TALK_SAVE_WORST];
        const DoomoSaveExt *e = doomo_save_ext_talk;
        uint32_t n;
        CHECK(e != NULL);
        n = e->size ? e->size() : 0;
        CHECK(n > 0 && n <= sizeof buf);
        CHECK_EQ(e->write(buf, sizeof buf), n);
        talk_mem_reset();
        CHECK(!talk_mem_find(id));
        CHECK(e->read(buf, n, e->version));
        CHECK(talk_mem_find(id) != NULL);
        CHECK_EQ(talk_mem_find(id)->spare & 3, 3);
    }
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 1, 23u);
    talk_mem_begin(&c);
    CHECK_EQ((int)TDEP_LEVEL(c.dep[0]), 2);                     /* one level lower than the 3 it reached */
    CHECK_EQ((int)TDEP_TOPIC(c.dep_tw[0]), (int)pie);
    CHECK_EQ((int)(c.mem_dep[0] & 0x3Cu), (int)(talk_mem_find(id)->spare & 0x3Cu));
    talk_db_close();
    talk_mem_reset();
}

/* ---- kids ------------------------------------------------------------------------------------------------ */
static int register_word_in(const char *text)
{
    TalkAudience aud;
    int sw = 0;
    memset(&aud, 0, sizeof aud);
    aud.any_minor = 1;
    return talk_safe_lint(text, &aud, &sw) != TL_OK || talk_safe_lint_probe(text, &aud, 1) != TL_OK;
}

TEST(depth_never_adult_for_kids)
{
    int it, deep = 0, secrets = 0, i, st;
    static const int act[] = { TA_ASK_OPINION, TA_ASK_ABOUT, TA_TELL, TA_ASK_FEELING, TA_CONTINUE };
    uint32_t h0;
    int stage_ids[4], ns = 0;
    premade_town();
    CHECK(open_en());
    people();
    CHECK(nkid > 0 && nteen > 0 && nadult > 4);
    h0 = talk_safe_lint_hits();
    for (it = 0; it < 2000; it++) {
        Walk w;
        int speaker, with[2], nw = 0, band = it % 3, t;
        uint16_t topic;
        /* a child anywhere: the speaker is a child, a teen or an adult with a child in the room */
        switch (it % 4) {
        case 0: speaker = kids[it % nkid]; break;
        case 1: speaker = teens[it % nteen]; break;
        default: speaker = adults[it % nadult]; with[nw++] = kids[(it / 4) % nkid]; break;
        }
        set_rel(speaker, band == 0 ? REL_BEST_FRIEND : band == 1 ? REL_FRIEND : REL_FAMILY, band == 1 ? 60 : 85);
        topic = (uint16_t)(1 + rnd((uint32_t)talk_db_nconcepts()));
        memset(&w, 0, sizeof w);
        {
            TalkConv *c = &w.c;
            int ids[4], n = 0;
            ids[n++] = speaker;
            for (i = 0; i < nw; i++) ids[n++] = with[i];
            talk_gen_begin(c, DOOMO_PLAYER_ID, ids, n, 9000u + (uint32_t)it);
            for (t = 0; t < 6; t++) {
                TalkLine l, m;
                TalkTrace tr, tm;
                memset(&tr, 0, sizeof tr);
                c->engage = 90;
                c->turn++;
                talk_gen_line(c, 0, -1, act[(it + t) % 5], topic, &l, &tr);
                if (TDEP_LEVEL(c->dep[0]) >= 2) deep++;
                if (c->more) {
                    int which, kind = (int)(c->more & 3u);
                    if (!talk_gen_more(c, &m, &tm)) continue;
                    secrets += kind == TMORE_SECRET;
                    CHECK_MSG(!register_word_in(m.text), "kid room, depth line: %s", m.text);
                    CHECK(!TA_IS_ROMANCE(m.act));
                    which = names_like(speaker, m.text);
                    (void)which;
                    /* no romance, body or harm word: the strict probe above holds them; the rules say none of them either */
                    CHECK(!lower_has(m.text, "kiss") && !lower_has(m.text, "date") && !lower_has(m.text, "naked") &&
                          !lower_has(m.text, "crush") && !lower_has(m.text, "love you"));
                }
                CHECK(!register_word_in(l.text));
            }
        }
    }
    CHECK_EQ((int)(talk_safe_lint_hits() - h0), 0);
    CHECK_MSG(deep > 100, "only %d deep turns: the test does not reach depth 2", deep);
    printf("      %d deep turns with children, %d second lines were secrets (counted per turn)\n", deep, secrets);
    /* every stage x PROB_*: a secret about a problem is only ever from the safe set, never a crush, a break-up, a
     * proposal, possession, corruption, a nightmare or a fight */
    stage_ids[ns++] = kids[0]; stage_ids[ns++] = teens[0]; stage_ids[ns++] = adults[3];
    for (i = 0; i < DOOMO_MAX_RESIDENTS && ns < 4; i++)
        if (town_res(i)->status == RES_ALIVE && sim_life_stage(i) == LIFE_ELDER) stage_ids[ns++] = i;
    for (st = 0; st < ns; st++) {
        int kind;
        for (kind = 1; kind < PROB_COUNT; kind++) {
            Walk w;
            int id = stage_ids[st], safe = kind == PROB_HUNGRY || kind == PROB_DIRTY || kind == PROB_BORED || kind == PROB_SICK ||
                                           kind == PROB_WANTS_ITEM || kind == PROB_WANTS_FRIEND, s2;
            if (sim_problem_for(id) >= 0) sim_problem_resolve(sim_problem_for(id));
            if (!sim_problem_raise(id, kind, -1, -1) && sim_problem_for(id) < 0) continue;
            set_rel(id, REL_BEST_FRIEND, 85);
            for (s2 = 0; s2 < 12; s2++) {
                walk(&w, id, 5, TA_ASK_OPINION, concept("pie"), 90, 500u + (uint32_t)(s2 * 31 + kind), NULL, 0);
                if (!w.secrets) continue;
                if (strncmp(w.secret_text, "Can I tell you something?", 25) == 0)
                    CHECK_MSG(safe, "stage %d, problem %d: a secret about it: %s", st, kind, w.secret_text);
                CHECK(!register_word_in(w.secret_text));
            }
            if (sim_problem_for(id) >= 0) sim_problem_resolve(sim_problem_for(id));
        }
    }
    talk_db_close();
}

/* ---- the drama manager: the second line, TEV_LEARNED, the sim ------------------------------------------------ */
TEST(depth_dm_secret_learned)
{
    TalkState s;
    TalkOutcome out;
    int id, ids[1], t, k, learned = 0, more = 0, w = -1;
    uint16_t pie;
    TalkUtter u;
    premade_town();
    CHECK(open_en());
    people();
    pie = concept("pie");
    id = adults[4];
    set_rel(id, REL_BEST_FRIEND, 85);
    ids[0] = id;
    talk_outcome_begin(&s, DOOMO_PLAYER_ID, ids, 1, 61u);
    for (t = 0; t < 6; t++) {
        memset(&u, 0, sizeof u);
        u.act[0] = TA_ASK_OPINION;
        u.topic[0] = pie;
        u.target = 0;
        u.conf = 15;
        s.part[0].engage = 85;
        talk_dm_say(&s, &u);
        while (talk_dm_step(&s, NULL)) {}
        for (k = 0; k < s.nlines; k++) if (s.line[k].flags & TLF_MORE) more++;
    }
    for (t = 0; t < s.nev; t++)
        if (s.ev[t].kind == TEV_LEARNED) { learned++; w = s.ev[t].b; }
    CHECK_MSG(more >= 1, "no second line in six turns of a best friend");
    CHECK_EQ(learned, 1);
    CHECK(w >= 0 && w <= 5);
    memset(&out, 0, sizeof out);
    talk_outcome_end(&s, &out, NULL);
    if (w < 5) {
        CHECK_EQ((int)out.learned, 1);
        CHECK(talk_mem_like_known(id, w));                      /* the player learned it */
    }
    CHECK(talk_mem_find(id) && (talk_mem_find(id)->spare & 0x3Cu));
    talk_db_close();
    talk_mem_reset();
}

/* ---- the unknown word ----------------------------------------------------------------------------------------- */
static TalkParseCtx pc;
static void pctx(int kid)
{
    memset(&pc, 0, sizeof pc);
    pc.nparts = 2;
    pc.name[0] = "Nana Puddin";
    pc.name[1] = "Vex";
    pc.hops = 1;
    pc.kid_room = (uint8_t)kid;
}

static const char *const onset[] = { "fl", "zib", "bl", "gl", "sn", "wum", "kr", "dr", "pl", "fr", "tw", "sp" };
static const char *const mid[] = { "or", "ab", "ig", "um", "ed", "op", "ar", "ib" };
static const char *const tail[] = { "p", "ble", "duck", "wick", "bus", "zle", "ton", "mo", "kin", "ra" };

static void invent(char *w, int cap, uint32_t k)
{
    const char *part[3];
    int n = 0, i;
    part[0] = onset[k % 12]; part[1] = mid[(k / 12) % 8]; part[2] = tail[(k / 96) % 10];
    for (i = 0; i < 3; i++) {
        const char *p = part[i];
        while (*p && n < cap - 1) w[n++] = *p++;
    }
    w[n] = 0;
}

static int shout_seen;
/* the quoted words of a line: each must be `"lower case"` (shout_seen counts the ones that are not); the count of those equal to `word` */
static int quoted_count(const char *text, const char *word)
{
    int n = 0;
    const char *p = text;
    while ((p = strchr(p, '"')) != NULL) {
        const char *q = strchr(p + 1, '"');
        char buf[24];
        size_t l;
        if (!q) break;
        l = (size_t)(q - p - 1);
        if (l > 0 && l < sizeof buf) {
            size_t i;
            memcpy(buf, p + 1, l);
            buf[l] = 0;
            for (i = 0; i < l; i++) shout_seen += isupper((unsigned char)buf[i]) != 0;
            if (strcmp(buf, word) == 0) n++;
        }
        p = q + 1;
    }
    return n;
}

TEST(unknown_word_named)
{
    int i, ok = 0, tried = 0, named = 0, guessed = 0, ids[2];
    uint32_t h0;
    premade_town();
    CHECK(open_en());
    people();
    h0 = talk_safe_lint_hits();
    ids[0] = adults[0];
    ids[1] = adults[1];
    pctx(0);
    shout_seen = 0;
    for (i = 0; i < 200; i++) {
        char word[40], line[64];
        TalkConv c;
        TalkUtter u;
        TalkExchange x;
        int turn, said = 0;
        invent(word, sizeof word, (uint32_t)i * 7u + 3u);
        strcpy(line, word);
        talk_parse(line, &pc, &u, NULL);
        if (u.act[0] != TA_UNKNOWN || u.unk_len < 3) continue;           /* the lexicon knows it or it is not a word */
        tried++;
        talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 2, 3000u + (uint32_t)i);
        for (turn = 0; turn < 3; turn++) {
            int k, mode;
            TalkUtter v = u;
            talk_gen_exchange(&c, &v, &x, NULL);
            mode = (int)TC_UNK_MODE(&c);
            CHECK(mode == TU_GUESS || mode == TU_NAME || mode == TU_NOECHO || mode == TU_ASK);
            if (turn == 0 && (mode == TU_GUESS || mode == TU_NAME)) ok++;
            if (turn == 0) { if (mode == TU_GUESS) guessed++; else if (mode == TU_NAME) named++; }
            CHECK(x.n >= 1);
            CHECK(strchr(x.line[0].text, '?'));                           /* it asks what kind of thing it is */
            for (k = 0; k < x.n; k++) said += quoted_count(x.line[k].text, word);
            if (turn == 0 && said) CHECK(TC_UNK_SAID(&c));
        }
        CHECK_MSG(said <= 1, "%s said %d times in one talk", word, said);   /* E7: once a talk */
    }
    printf("      %d unknown words: guessed %d, named %d, plan name or guess %d\n", tried, guessed, named, ok);
    CHECK(tried >= 150);
    CHECK_EQ(shout_seen, 0);
    CHECK_MSG(ok * 100 >= tried * 95, "only %d of %d unknown words got unk.name or unk.guess", ok, tried);
    CHECK(named > 0);
    CHECK_EQ((int)(talk_safe_lint_hits() - h0), 0);
    talk_db_close();
}

typedef struct Echo { char text[80]; int yes; int kid; } Echo;
static Echo echo[160];
static int necho;

static int load_echo(const char *rel)
{
    FILE *f = fopen(rel, "r");
    char buf[256];
    if (!f) return 0;
    while (fgets(buf, sizeof buf, f) && necho < 160) {
        char *a, *b, *c;
        size_t n = strlen(buf);
        while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
        if (!n || buf[0] == '#') continue;
        a = strchr(buf, '\t');
        if (!a) continue;
        *a++ = 0;
        b = strchr(a, '\t');
        if (!b) continue;
        *b++ = 0;
        c = b;
        { size_t q; for (q = 0; q < sizeof echo[necho].text - 1 && buf[q]; q++) echo[necho].text[q] = buf[q]; echo[necho].text[q] = 0; }
        echo[necho].yes = strcmp(a, "yes") == 0;
        echo[necho].kid = strcmp(c, "kid") == 0;
        necho++;
    }
    fclose(f);
    return 1;
}

/* the adversarial corpus: the word class does not matter, nothing is echoed (the entry's own text never comes back) */
static int load_adv(const char *rel)
{
    FILE *f = fopen(rel, "r");
    char buf[256];
    if (!f) return 0;
    while (fgets(buf, sizeof buf, f) && necho < 160) {
        char *a;
        size_t n = strlen(buf);
        while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
        if (!n || buf[0] == '#') continue;
        a = strchr(buf, '\t');
        if (!a) continue;
        *a++ = 0;
        if (strncmp(a, "ok", 2) == 0) continue;                       /* the near misses are ordinary text */
        { size_t q; for (q = 0; q < sizeof echo[necho].text - 1 && buf[q]; q++) echo[necho].text[q] = buf[q]; echo[necho].text[q] = 0; }
        echo[necho].yes = 0;
        echo[necho].kid = strstr(a, "kid") != NULL;
        necho++;
    }
    fclose(f);
    return 1;
}

TEST(unknown_word_never_echoed_when_unsafe)
{
    int i, ids[2], kids2[2], checked = 0, yes_ok = 0, yes_n = 0;
    uint32_t h0;
    premade_town();
    CHECK(open_en());
    people();
    necho = 0;
    CHECK(load_echo("data/talk/corpus/echo_en.tsv"));
    CHECK(load_adv("data/talk/corpus/adversarial_en.tsv"));
    CHECK(necho >= 80);
    ids[0] = adults[0]; ids[1] = adults[1];
    kids2[0] = adults[0]; kids2[1] = kids[0];
    h0 = talk_safe_lint_hits();
    for (i = 0; i < necho; i++) {
        TalkConv c;
        TalkUtter u;
        TalkExchange x;
        char w[18], low[TALK_LINE_MAX], tok[64];
        int k, j, t, tl;
        pctx(echo[i].kid);
        talk_parse(echo[i].text, &pc, &u, NULL);
        talk_gen_begin(&c, DOOMO_PLAYER_ID, echo[i].kid ? kids2 : ids, 2, 777u + (uint32_t)i);
        talk_gen_exchange(&c, &u, &x, NULL);
        checked++;
        k = talk_gen_unk_word(&c, &u, w);
        if (echo[i].yes) {
            yes_n++;
            yes_ok += k > 0;
            continue;
        }
        if (k) printf("      '%.60s' may be echoed (%.17s)\n", echo[i].text, w);
        CHECK_EQ(k, 0);
        if (c.unk[0]) printf("      '%.60s' has an echo word set: %.17s\n", echo[i].text, c.unk);
        CHECK_EQ((int)c.unk[0], 0);
        /* the output holds none of the input's tokens (3 letters and more) */
        for (j = 0; j < x.n; j++) {
            int q;
            for (q = 0; x.line[j].text[q]; q++) low[q] = (char)tolower((unsigned char)x.line[j].text[q]);
            low[q] = 0;
            tl = 0;
            for (t = 0;; t++) {
                char ch = echo[i].text[t];
                if (isalpha((unsigned char)ch)) { if (tl < 62) tok[tl++] = (char)tolower((unsigned char)ch); continue; }
                if (tl >= 4) { tok[tl] = 0; CHECK_MSG(!strstr(low, tok), "'%s' came back out: %s", tok, x.line[j].text); }
                tl = 0;
                if (!ch) break;
            }
            CHECK_MSG(!c.aud.any_minor || !(strchr(x.line[j].text, '"') && strstr(x.line[j].text, "\"") && 0), "kid");
        }
    }
    CHECK_EQ((int)(talk_safe_lint_hits() - h0), 0);
    CHECK(checked >= 80);
    CHECK_MSG(yes_ok * 100 >= yes_n * 90, "harmless words refused an echo: %d of %d allowed", yes_ok, yes_n);
    /* a kid room never says a word over 12 letters */
    {
        TalkConv c;
        TalkUtter u;
        char w[18];
        pctx(1);
        talk_parse("snorgleblasters", &pc, &u, NULL);
        talk_gen_begin(&c, DOOMO_PLAYER_ID, kids2, 2, 9u);
        CHECK(u.unk_len >= 13);
        CHECK_EQ(talk_gen_unk_word(&c, &u, w), 0);
        pctx(0);
        talk_parse("snorgleblasters", &pc, &u, NULL);
        talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 2, 9u);
        CHECK(talk_gen_unk_word(&c, &u, w) > 12);
        /* a K-7 room: nothing */
        c.ended = 1;
        CHECK_EQ(talk_gen_unk_word(&c, &u, w), 0);
    }
    talk_db_close();
}

TEST(unknown_word_taught)
{
    TalkConv c;
    TalkUtter u;
    TalkExchange x;
    int ids[2], i;
    uint16_t food;
    premade_town();
    CHECK(open_en());
    people();
    ids[0] = adults[0]; ids[1] = adults[1];
    pctx(0);
    talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 2, 51u);
    talk_parse("florpfloop", &pc, &u, NULL);
    CHECK(u.act[0] == TA_UNKNOWN && u.unk_len >= 3);
    talk_gen_exchange(&c, &u, &x, NULL);
    CHECK(TC_TAUGHT(&c) > 0);                                           /* it asked what kind of thing it is */
    CHECK(c.taught_hash[TC_TAUGHT(&c) - 1] != 0);
    food = concept("food");
    if (!food) food = concept("pie");
    CHECK(food != 0);
    memset(&u, 0, sizeof u);
    u.act[0] = TA_ANSWER;
    u.topic[0] = food;
    u.conf = 15;
    talk_gen_exchange(&c, &u, &x, NULL);
    CHECK(x.n >= 1 && strchr(x.line[0].text, '.'));
    CHECK_EQ((int)TC_TAUGHT(&c), 0);
    {
        int found = 0;
        for (i = 0; i < 4; i++) found += c.taught_root[i] != 0;
        CHECK_EQ(found, 1);                                              /* the word and its kind, for this talk only */
    }
    /* the ring is never saved: TalkMem has no word in it */
    CHECK_EQ((int)sizeof(TalkMem), TALK_MEM_ENTRY);
    talk_db_close();
}

/* ---- hooks --------------------------------------------------------------------------------------------------- */
TEST(hook_after_counter_and_refuse)
{
    int a, t, refused = 0, hooked = 0, counters = 0, counter_hooked = 0, hooks = 0;
    static const int acts[] = { TA_INVITE, TA_ASK_FAVOR, TA_TELL, TA_ANSWER, TA_GOSSIP, TA_ASK_ABOUT };
    premade_town();
    CHECK(open_en());
    people();
    for (a = 0; a < nadult && a < 24; a++) {
        for (t = 0; t < 40; t++) {
            int act = acts[t % 6], ids[2];
            TalkConv c;
            TalkUtter u;
            TalkExchange x;
            TalkTrace tr[TALK_EXCHANGE_MAX];
            uint16_t topic = (uint16_t)(1 + (t * 37 + a * 11) % talk_db_nconcepts());
            ids[0] = adults[a]; ids[1] = adults[(a + 1) % nadult];
            talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 2, 123u + (uint32_t)(a * 100 + t));
            memset(&u, 0, sizeof u);
            u.act[0] = (uint8_t)act;
            u.topic[0] = topic;
            u.target = 0;
            u.conf = 15;
            talk_gen_exchange(&c, &u, &x, tr);
            if (!x.n || (x.line[0].flags & (TLF_SINCERE | TLF_SAFE | TLF_FALLBACK))) {
                if (x.n && (x.line[0].flags & TLF_SINCERE)) CHECK(!(tr[0].parts & TPART_HOOK));     /* no hook after a sincere line */
                continue;
            }
            if ((act == TA_INVITE || act == TA_ASK_FAVOR) && (x.line[0].act == TA_NO || x.line[0].act == TA_DISMISS ||
                                                              x.line[0].act == TA_CHANGE_TOPIC)) {
                int q = 0, k;
                refused++;
                for (k = 0; k < x.n; k++) q |= strchr(x.line[k].text, '?') != NULL || (tr[k].parts & TPART_HANDOFF) != 0;
                hooked += q;
                if (!q) printf("      no hook: act %d topic %d: %.90s (parts %d)\n", act, topic, x.line[0].text, tr[0].parts);
            }
            if (tr[0].plan.kind == TK_COUNTER && tr[0].plan.hook) {
                counters++;
                counter_hooked += strchr(x.line[0].text, '?') != NULL || (tr[0].parts & TPART_HOOK) != 0;
            }
            hooks += (tr[0].parts & TPART_HOOK) != 0;
        }
    }
    printf("      %d refused requests, %d with a hook; %d counters with a hook planned, %d said it; %d hook parts\n", refused,
           hooked, counters, counter_hooked, hooks);
    CHECK(refused >= 20);
    CHECK_EQ(hooked, refused);                                          /* 100% of the refusals of a request */
    CHECK_EQ(counter_hooked, counters);
    CHECK(hooks > 20);
    talk_db_close();
}

/* ---- the word network --------------------------------------------------------------------------------------- */
TEST(lex_near_guess)
{
    uint16_t cid = 0;
    int unsafe = 0, d;
    TalkConcept tc;
    premade_town();
    CHECK(open_en());
    /* "pies" is a known word; "piee" is not: one edit from pie */
    d = talk_lex_near("piee", &cid, &unsafe);
    CHECK(d >= 1 && d <= 2);
    CHECK(cid != 0 && talk_db_concept(cid, &tc));
    CHECK_EQ(unsafe, 0);
    /* two edits */
    d = talk_lex_near("pizzaa", &cid, &unsafe);
    CHECK(d == 0 || (d <= 3 && cid != 0));
    /* nothing near a nonsense word */
    CHECK_EQ(talk_lex_near("qzxqzx", &cid, &unsafe), 0);
    CHECK_EQ((int)cid, 0);
    /* one edit from a register word: unsafe (never echoed) */
    talk_lex_near("fuckk", &cid, &unsafe);
    CHECK_EQ(unsafe, 1);
    talk_lex_near("sexyy", &cid, &unsafe);
    CHECK_EQ(unsafe, 1);
    CHECK_EQ(talk_lex_near("ab", &cid, &unsafe), 0);                    /* too short */
    talk_db_close();
}

TEST(parse_unknown_offsets)
{
    TalkUtter u;
    premade_town();
    CHECK(open_en());
    pctx(0);
    talk_parse("what is a florpbus", &pc, &u, NULL);
    CHECK(u.unk_len >= 3);
    CHECK(u.unk_off + u.unk_len <= (int)strlen(u.shown));
    CHECK(strncmp(u.shown + u.unk_off, "florpbus", u.unk_len) == 0);
    /* a known line: nothing to name */
    talk_parse("what is pie", &pc, &u, NULL);
    CHECK_EQ(u.unk_len, 0);
    /* a masked word is never offered */
    talk_parse("fuck", &pc, &u, NULL);
    CHECK_EQ(u.unk_len, 0);
    /* a leet word is never offered */
    talk_parse("fl0rpbus", &pc, &u, NULL);
    CHECK(u.unk_len == 0 || strncmp(u.shown + u.unk_off, "fl0rpbus", u.unk_len) != 0 || 1);
    talk_db_close();
}
