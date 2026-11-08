/* test_talk_census.c - T7 / T5a: the smoke census and the generator-level
 * property tests over the English banks (TALK_en.BIN). Owner: TK.
 * docs/TALK.md "The census", "The generator"; design 3.7 and 7.5.
 *
 *   talk_census_smoke     the census at the grid's smoke size (20,000
 *                         contexts, 1,000 conversations): quality failures
 *                         1% or less (Q1-Q6), no safety failure, no S3 lint
 *                         hit, and floors on the three tiers that fail early
 *                         when a bank collapses
 *   talk_gen_property_*   P1 kid rooms, P2 both directions, P3 K-7, P4 the
 *                         pet, P5 Mature, P6 closed vocabulary, P7 no romance
 *                         state, P8 the swear cap: over generated lines, every
 *                         player act including the romance and safety ones.
 *                         DOOMO_TALK_PROPERTY_N=200000 runs the design's size
 *                         (the unit run does 20,000).
 * The census engine is core/tools/talk_census.c, included here as a library.
 */
#define TALK_CENSUS_NO_MAIN
#include "../tools/talk_census.c"
#include "test.h"
#include "doomo/playas.h"

static const char *grid_path(char *buf, int cap)
{
    snprintf(buf, (size_t)cap, "%s/../../data/talk/census_en.tsv", test_golden_dir);
    return buf;
}

TEST(talk_census_smoke)
{
    CensusCfg cfg;
    CensusResult r;
    char path[512], err[256];
    Grid g;
    uint64_t failed, safety;
    memset(&cfg, 0, sizeof cfg);
    cfg.grid = grid_path(path, (int)sizeof path);
    cfg.quiet = 1;
    CHECK(talk_db_open(lang_find("en")) == TALK_DB_OK);
    CHECK(load_grid(cfg.grid, &g, err, (int)sizeof err) == 0);
    talk_db_close();
    cfg.contexts = g.smoke_contexts;
    cfg.conversations = g.smoke_conversations;
    CHECK(cfg.contexts >= 10000 && cfg.conversations >= 500);
    CHECK_MSG(talk_census_run(&cfg, &r, err, (int)sizeof err) == 0, "%s", err);
    talk_census_print(&r, stdout);
    failed = r.contexts - r.exchanges_ok;
    safety = r.fail[QF_Q6_LINT] + r.fail[QF_Q6_KIDROOM] + r.fail[QF_Q6_MATURE];
    CHECK_EQ(r.lint_hits, 0);
    CHECK_EQ(safety, 0);
    CHECK_MSG(failed * 100 <= r.contexts, "quality failures %llu of %llu (over 1%%)", (unsigned long long)failed,
              (unsigned long long)r.contexts);
    CHECK(r.kid_rooms * 4 >= r.contexts);               /* the grid really puts children in rooms */
    /* floors for the starter bank (docs/TALK.md "The census"): a bank that
     * collapses fails here first. The design's smoke bar (90% Surface-unique
     * at 100,000 contexts) is T5b's, with its 6,000 alternatives. */
    CHECK_MSG(r.surface.distinct * 100 >= r.exchanges_ok * 50, "Surface %llu of %llu under 50%% unique",
              (unsigned long long)r.surface.distinct, (unsigned long long)r.exchanges_ok);
    CHECK_MSG(r.skeleton.distinct * 100 >= r.exchanges_ok * 30, "Skeleton %llu of %llu under 30%% unique",
              (unsigned long long)r.skeleton.distinct, (unsigned long long)r.exchanges_ok);
    CHECK_MSG(r.meaning.distinct >= 4000, "Meaning %llu distinct, under 4,000", (unsigned long long)r.meaning.distinct);
    CHECK(r.conv_distinct * 100 >= r.conversations * 99);
    CHECK_MSG(r.conv_q5 * 100 <= r.conversations * 2, "Q5: %llu of %llu conversations repeat a line",
              (unsigned long long)r.conv_q5, (unsigned long long)r.conversations);
}

/* ---- the property tests over generated lines ---------------------------------- */
static uint32_t prs;
static uint32_t prnd(uint32_t n) { prs ^= prs << 13; prs ^= prs >> 17; prs ^= prs << 5; return n ? prs % n : 0; }

static int nadult_g, nminor_g, adults_g[64], minors_g[64];

static void people(void)
{
    int id;
    nadult_g = nminor_g = 0;
    for (id = 0; id < DOOMO_MAX_RESIDENTS; id++) {
        if (town_res(id)->status != RES_ALIVE) continue;
        if (talk_safe_minor(id)) { if (nminor_g < 64 && id % 3 == 0) minors_g[nminor_g++] = id; }
        else if (nadult_g < 64 && id % 11 == 0) adults_g[nadult_g++] = id;
    }
}

static int is_romance_or_refusal(int act)
{
    return TA_IS_SAFETY(act);                     /* REFUSE_*, DEFLECT_ROMANCE, DISTRESS (distress first: K-11) */
}

/* the count of lines a probe reads as holding an everyday swear */
static int swear_lines(const TalkExchange *x)
{
    TalkAudience adults;
    int k, n = 0;
    memset(&adults, 0, sizeof adults);
    for (k = 0; k < x->n; k++) if (talk_safe_lint_probe(x->line[k].text, &adults, 0) == TL_SWEAR_CAP) n++;
    return n;
}

TEST(talk_gen_property_rooms)
{
    static const uint8_t romance[] = { TA_FLIRT, TA_CONFESS, TA_ASK_OUT, TA_KISS };
    int it, nit = 20000, edges_before, p1 = 0, p2 = 0, p3 = 0, p4 = 0, p5 = 0, romances = 0, mature_seen = 0;
    uint32_t h0;
    const char *env = getenv("DOOMO_TALK_PROPERTY_N");
    if (env && atoi(env) > 0) nit = atoi(env);
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
    CHECK(talk_db_open(lang_find("en")) == TALK_DB_OK && talk_gen_ready());
    people();
    CHECK(nadult_g > 10 && nminor_g > 10);
    edges_before = doomo_town.edge_count;
    h0 = talk_safe_lint_hits();
    prs = 0x5EEDu;
    for (it = 0; it < nit; it++) {
        TalkConv c;
        TalkUtter u;
        TalkExchange x;
        TalkTrace tr[TALK_EXCHANGE_MAX];
        int ids[TALK_PARTS_MAX], n = 1 + (int)prnd(3), i, k, avatar, any_minor, act, q;
        uint8_t in_act;
        opt_set(OPT_MATURE, prnd(3) == 0 ? MATURE_ON : MATURE_OFF);
        for (i = 0; i < n; i++) {
            int r = (int)prnd(10);
            ids[i] = r < 4 ? minors_g[prnd((uint32_t)nminor_g)] : r < 9 ? adults_g[prnd((uint32_t)nadult_g)] : TALK_PET_ID;
            for (k = 0; k < i; k++) if (ids[k] == ids[i]) ids[i] = adults_g[(it + k) % nadult_g];
        }
        avatar = prnd(5) == 0 ? minors_g[prnd((uint32_t)nminor_g)] : prnd(3) == 0 ? adults_g[prnd((uint32_t)nadult_g)]
                                                                          : DOOMO_PLAYER_ID;
        talk_gen_begin(&c, avatar, ids, n, 1000u + (uint32_t)it);
        c.consent = (uint8_t)prnd(16);
        any_minor = c.aud.any_minor;
        memset(&u, 0, sizeof u);
        act = prnd(4) == 0 ? romance[prnd(4)] : 1 + (int)prnd(TA_COUNT - 1);
        u.act[0] = (uint8_t)act;
        u.topic[0] = (uint16_t)(1 + prnd((uint32_t)talk_db_nconcepts()));
        u.target = (int8_t)(prnd(5) == 0 ? -1 : (int)prnd((uint32_t)n));
        u.tone = (uint8_t)prnd(TONE_COUNT);
        u.safety = (uint8_t)((prnd(6) == 0 ? TS_SEXUAL : 0) | (prnd(8) == 0 ? TS_MINOR : 0) |
                             (prnd(20) == 0 ? TS_HATE : 0) | (prnd(30) == 0 ? TS_DISTRESS : 0));
        strcpy(u.shown, "zorpquux blorpfizz");
        in_act = u.act[0];
        talk_gen_exchange(&c, &u, &x, tr);
        q = x.n ? talk_census_check(&c, &u, &x, tr) : QF_NONE;
        CHECK_MSG(q != QF_Q6_LINT && q != QF_Q6_KIDROOM && q != QF_Q6_MATURE, "safety check %s: %s",
                  talk_census_qf_names[q], x.n ? x.line[0].text : "");
        for (k = 0; k < x.n; k++) {
            TalkAltInfo ai;
            int j;
            const TalkLine *l = &x.line[k];
            CHECK(!strstr(l->text, "zorp") && !strstr(l->text, "blorp"));   /* never the typed words */
            if (any_minor) {
                /* P1: no register word, no LOVE face, no romance rule or act in a kid room */
                CHECK_MSG(talk_safe_lint_probe(l->text, &c.aud, 1) == TL_OK, "kid room: %s", l->text);
                CHECK(l->face != EXPR_LOVE + 1);
                CHECK_MSG(!TA_IS_ROMANCE(l->act), "a romance act in a kid room: %s", l->text);
                for (j = 0; j < tr[k].nalt; j++) CHECK(talk_gen_alt_info(tr[k].alt[j], &ai) && !(ai.rflags & 4u));
                p1++;
            }
            /* P2: nobody under 18 speaks a romance line, and no romance line is said to one */
            if (TA_IS_ROMANCE(l->act) || l->face == EXPR_LOVE + 1) {
                CHECK(!talk_safe_minor(c.id[l->who]) && !talk_safe_minor(c.avatar) && !any_minor && !c.aud.pet);
                romances++;
            }
            /* P4: the pet */
            if (c.aud.pet) {
                TalkAudience pet;
                memset(&pet, 0, sizeof pet);
                pet.pet = 1;
                CHECK(talk_safe_lint_probe(l->text, &pet, 0) != TL_SEXUAL_PET);
                p4++;
            }
            /* P5: a Mature variant only with the option on and premade adults only */
            for (j = 0; j < tr[k].nalt; j++)
                if (talk_gen_alt_info(tr[k].alt[j], &ai) && (ai.aflags & 4u)) {
                    CHECK(opt_mature() >= MATURE_ON && c.aud.mature_ok);
                    mature_seen++;
                }
            p5++;
        }
        /* the response to a romance or sexual act with a child near is a refusal */
        if ((TA_IS_ROMANCE(in_act) || (u.safety & TS_SEXUAL) || in_act == TA_REFUSE_SEXUAL) && any_minor && x.n) {
            CHECK_MSG(is_romance_or_refusal(x.line[0].act), "act %d after a romance act in a kid room: %s",
                      x.line[0].act, x.line[0].text);
            p2++;
        }
        /* P3: sexual with any child near: the conversation ends, unseen */
        if (x.gate == TG_END_K7) {
            CHECK(c.ended);
            for (k = 0; k < x.n; k++) CHECK(x.line[k].flags & TLF_END);
            CHECK_EQ(u.shown[0], 0);
            p3++;
        }
    }
    opt_set(OPT_MATURE, MATURE_OFF);
    /* P7: Talk changed no romance state */
    CHECK_EQ(sim_romance_sweep(-1), 0);
    CHECK_EQ(doomo_town.edge_count, edges_before);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    printf("      %d exchanges: %d kid-room lines, %d romance/sexual acts near a child refused, %d K-7 endings, "
           "%d pet-room lines, %d romance lines between adults, %d Mature variants\n",
           nit, p1, p2, p3, p4, romances, mature_seen);
    (void)p5;
    talk_db_close();
}

/* P6: typed junk never comes back out; P8: one everyday swear a conversation */
TEST(talk_gen_property_vocabulary_and_swears)
{
    static const char *const junk[] = { "zorp", "fuck", "shit", "asdf", "qwerty", "blorp", "naked", "glorp",
                                        "xyzzy", "wank", "nude", "hitler", "pie", "ghost", "kid", "sexy" };
    static const char *const never_out[] = { "zorp", "fuck", "shit", "asdf", "qwerty", "blorp", "naked", "glorp",
                                             "xyzzy", "wank", "nude", "hitler", "sexy" };
    int it, convs = 0, i, k, blunt[64], nb = 0, id;
    uint32_t h0;
    TalkParseCtx pc;
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
    CHECK(talk_db_open(lang_find("en")) == TALK_DB_OK && talk_gen_ready());
    h0 = talk_safe_lint_hits();
    prs = 4242u;
    for (it = 0; it < 4000; it++) {
        TalkConv c;
        TalkUtter u;
        TalkExchange x;
        char text[96];
        int ids[2] = { 1, 666 }, w = 1 + (int)prnd(5), o = 0;
        for (i = 0; i < w && o < 80; i++)
            o += snprintf(text + o, sizeof text - (size_t)o, "%s%s", i ? " " : "", junk[prnd(16)]);
        memset(&pc, 0, sizeof pc);
        pc.nparts = 2; pc.name[0] = "Nana Puddin"; pc.name[1] = "Vex"; pc.hops = 1;
        talk_parse(text, &pc, &u, NULL);
        talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 2, (uint32_t)it);
        talk_gen_exchange(&c, &u, &x, NULL);
        for (k = 0; k < x.n; k++) {
            char low[TALK_LINE_MAX];
            for (i = 0; x.line[k].text[i]; i++) low[i] = (char)tolower((unsigned char)x.line[k].text[i]);
            low[i] = 0;
            for (i = 0; i < (int)(sizeof never_out / sizeof never_out[0]); i++)
                CHECK_MSG(!strstr(low, never_out[i]), "'%s' came back out: %s", never_out[i], x.line[k].text);
        }
    }
    /* P8: blunt adults trading insults: at most one everyday swear a conversation, no capped word ever */
    for (id = 0; id < DOOMO_MAX_RESIDENTS && nb < 64; id++)
        if (!talk_safe_minor(id) && RES_PERSONA(town_res(id), PERS_SPEECH) >= 10) blunt[nb++] = id;
    CHECK(nb > 4);
    for (it = 0; it < 600; it++) {
        TalkConv c;
        int ids[3] = { blunt[it % nb], blunt[(it + 1) % nb], blunt[(it + 2) % nb] }, t, sw = 0;
        talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, 3, 77u + (uint32_t)it);
        for (t = 0; t < 6; t++) {
            TalkUtter u;
            TalkExchange x;
            memset(&u, 0, sizeof u);
            u.act[0] = (uint8_t)(t & 1 ? TA_INSULT : TA_COMPLAIN);
            u.target = (int8_t)(t % 3);
            talk_gen_exchange(&c, &u, &x, NULL);
            sw += swear_lines(&x);
        }
        CHECK_MSG(sw <= 1 && c.swears <= 1, "conversation %d: %d swears", it, sw);
        convs += sw;
    }
    CHECK_EQ(talk_safe_lint_hits(), h0);
    printf("      4000 typed junk lines: nothing typed came back; 600 blunt conversations, %d with their one swear\n",
           convs);
    talk_db_close();
}
