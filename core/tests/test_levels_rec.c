/* test_levels_rec.c - LV0 of the levels expansion (owner: levels; plan
 * sections 2.5, 7.1, 8.5, 9): the registry of 31 levels, the platform
 * manifest (the GBA lists seven of the 24 new ones), the unlock rules (by
 * level, chapter, episode and New Game+), the 'BRCH' save chunk (round
 * trip, the GBA's smaller chunk, bad chunks, absent chunk), the joint
 * save budget and the level select's pages at both screen sizes.
 *
 * The 24 new levels have no map yet (LV12 and later build them), so the
 * select tests re-point them at the Vape Cellar (lv_info is writable on the
 * host builds) and put every entry back. */
#include "test.h"
#include "doomo/save_budget.h"
#include "doomo/story.h"
#include "doomo/home.h"
#include "../src/home_int.h"
#include "../src/levels_int.h"

static uint8_t image[256 * 1024];

/* every test starts as the host with every level listed (a failed test must not leave the GBA's list on) */
static void reset(void) { lv_list_req = 0; test_reset(); }

static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }

static void shot(const char *name)
{
    char path[600];
    snprintf(path, sizeof(path), "%s/levels_%s.png", test_tmp_dir, name);
    nat_png_write(path, nat_fb, nat_w, nat_h, 1);
}

/* the seven new levels of the GBA (plan 7.1: HB8, HB9, HB13, HB15, N1, N7, N14) */
static const int gba_new[7] = { LV_HB8, LV_HB9, LV_HB13, LV_HB15, LV_N1, LV_N7, LV_N14 };

static int on_gba_list(int lv)
{
    int i;
    if (lv < LV_OLD_COUNT) return 1;
    for (i = 0; i < 7; i++) if (gba_new[i] == lv) return 1;
    return 0;
}

TEST(levels_registry_ids_pages_and_manifest)
{
    int i, n = 0, per_page[LVP_COUNT] = { 0, 0, 0 };
    lv_list_req = 0;
    /* the ids saves use never move */
    CHECK_EQ(LV_COUNT, 31);
    CHECK_EQ(LV_UNDERBELLY, 6);
    CHECK_EQ(LV_HB8, 7);
    CHECK_EQ(LV_HB16, 15);
    CHECK_EQ(LV_N1, 16);
    CHECK_EQ(LV_N15, 30);
    CHECK_EQ(LV2_COUNT, 24);
    for (i = 0; i < LV_COUNT; i++) {
        const LevelInfo *L = &lv_info[i];
        CHECK_MSG(L->title && L->title[0] && L->tag && L->tag[0] && L->blurb && L->blurb[0], "level %d text", i);
        CHECK_MSG(L->dev > 30 && L->dev < L->par, "level %d times", i);
        CHECK_MSG(strlen(L->blurb) < 70, "level %d blurb", i);
        {   /* each of the two lines fits the select's text box at 240 wide */
            const char *nl = strchr(L->blurb, '\n');
            char l1[80];
            CHECK_MSG(nl != NULL, "level %d blurb has two lines", i);
            snprintf(l1, sizeof(l1), "%.*s", (int)(nl - L->blurb), L->blurb);
            CHECK_MSG(ui_text_width(l1) <= 224 && ui_text_width(nl + 1) <= 224, "level %d blurb too wide", i);
        }
        CHECK_MSG(ui_text_width(L->title) < 240 - 20 && ui_text_width(L->tag) < 100, "level %d title does not fit", i);
        CHECK_MSG(lv_reg[i].reason < LVT_COUNT && lv_reg[i].page < LVP_COUNT, "level %d registry", i);
        per_page[lv_page_of(i)]++;
        CHECK_EQ(lv_listed(i), 1);                       /* the host lists them all */
        /* built only when it has a map: the 7 old ones today */
        CHECK_EQ(lv_built(i), i < LV_OLD_COUNT);
        CHECK_EQ(lv_built(i), lv_info[i].map != LV_NO_MAP);
        n += (lv_reg[i].flags & LVG_GBA) != 0;
    }
    CHECK_EQ(per_page[LVP_BREACH], 7);
    CHECK_EQ(per_page[LVP_HELL], 9);
    CHECK_EQ(per_page[LVP_ARREARS], 15);
    CHECK_EQ(n, 14);                                     /* 7 + the GBA's seven */
    CHECK_EQ(lv_listed_count(), 31);
    CHECK_EQ(lv_page_of(-1), LVP_BREACH);
    CHECK(!lv_listed(-1) && !lv_listed(LV_COUNT) && !lv_built(99));
    /* the manifest is the plan's seven; the GBA lists exactly those */
    lv_list_req = LVG_GBA;
    CHECK_EQ(lv_listed_count(), 14);
    for (i = 0; i < LV_COUNT; i++) CHECK_MSG(lv_listed(i) == on_gba_list(i), "level %d on the GBA list", i);
    /* its records are numbered in id order, 0..6 */
    for (i = 0; i < 7; i++) CHECK_EQ(lv_rec_slot(gba_new[i]), i);
    CHECK_EQ(lv_rec_slot(LV_HB10), -1);
    CHECK_EQ(lv_rec_slot(LV_UNDERBELLY), -1);            /* the old seven have no record here */
    lv_list_req = 0;
    CHECK_EQ(lv_rec_slot(LV_HB8), 0);
    CHECK_EQ(lv_rec_slot(LV_N15), 23);
    CHECK_EQ(lv_rec_slot(LV_N15 + 1), -1);
    /* the names of the three pages */
    CHECK_STR(lv_page_names[LVP_BREACH], "Breach 1-7");
    CHECK_STR(lv_page_names[LVP_HELL], "Hell Breach 8-16");
    CHECK_STR(lv_page_names[LVP_ARREARS], "Arrears");
}

TEST(levels_records_only_improve_and_leave_the_old_ranks_alone)
{
    int lv;
    unsigned packed;
    reset();
    packed = progress_get(PROG_BREACH_RANKS);
    CHECK(lv_set_rank(LV_HB10, RANK_B));
    CHECK_EQ(lv_rank(LV_HB10), RANK_B);
    CHECK(!lv_set_rank(LV_HB10, RANK_C));                /* only improves */
    CHECK(lv_set_rank(LV_HB10, RANK_S));
    CHECK(!lv_set_rank(LV_HB10, RANK_S));
    CHECK_EQ(progress_get(PROG_BREACH_RANKS), packed);   /* the new levels never touch the old counter */
    CHECK_EQ(lv_rank(LV_HB9), RANK_NONE);
    CHECK_EQ(lv_rank(LV_N15 + 1), RANK_NONE);
    CHECK(!lv_set_rank(LV_N15 + 1, RANK_S));
    /* gold, feats, cleared, secrets, collectibles */
    CHECK(!lv_gold(LV_N3));
    lv_rec_set_gold(LV_N3);
    CHECK(lv_gold(LV_N3) && !lv_gold(LV_N2));
    CHECK(!unlock_has(LVU_GOLD0 + LV_SEWERS));
    lv_rec_add_feats(LV_N3, 5);
    lv_rec_add_feats(LV_N3, 2);
    CHECK_EQ(lv_rec_feats(LV_N3), 7);
    lv_rec_add_feats(LV_N3, 0xFF);
    CHECK_EQ(lv_rec_feats(LV_N3), 7);                    /* 3 bits */
    CHECK(!lv_rec_cleared(LV_N3));
    lv_rec_set_cleared(LV_N3);
    CHECK(lv_rec_cleared(LV_N3) && lv_rank(LV_N3) == RANK_NONE);
    lv_rec_set_secrets(LV_N3, 4);
    lv_rec_set_secrets(LV_N3, 2);
    CHECK_EQ(lv_rec_secrets(LV_N3), 4);
    lv_rec_set_secrets(LV_N3, 900);
    CHECK_EQ(lv_rec_secrets(LV_N3), 255);
    lv_rec_add_collect(LV_N3, 0x8001);
    lv_rec_add_collect(LV_N3, 0x0100);
    CHECK_EQ(lv_rec_collect(LV_N3), 0x8101u);
    /* one record never leaks into its neighbours */
    CHECK(lv_rank(LV_N2) == RANK_NONE && lv_rec_feats(LV_N4) == 0 && lv_rec_collect(LV_N2) == 0 && lv_rec_secrets(LV_N4) == 0);
    /* ranks up to S on every new level, the old seven still in their counter */
    for (lv = 0; lv < LV_COUNT; lv++) CHECK(lv_set_rank(lv, RANK_A) || lv_rank(lv) == RANK_S);
    for (lv = 0; lv < LV_COUNT; lv++) CHECK(lv_rank(lv) >= RANK_A);
    CHECK_EQ(lv_cleared_count(), LV_OLD_COUNT);          /* the trophies still count the old seven */
}

TEST(levels_arc_state)
{
    reset();
    CHECK_EQ(lv_arc_act(), LVA_NONE);
    CHECK(!lv_arc_finished());
    lv_arc_set_act(LVA_MIDPOINT);
    CHECK_EQ(lv_arc_act(), LVA_MIDPOINT);
    lv_arc_set_act(99);
    CHECK_EQ(lv_arc_act(), LVA_DONE);
    CHECK(lv_arc_finished());
    lv_arc_set_flag(0, 1); lv_arc_set_flag(15, 1); lv_arc_set_flag(16, 1);
    CHECK(lv_arc_flag(0) && lv_arc_flag(15) && !lv_arc_flag(16) && !lv_arc_flag(7));
    lv_arc_set_flag(0, 0);
    CHECK(!lv_arc_flag(0) && lv_arc_flag(15));
    lv_arc_set_buyback(0xA5C3);
    CHECK_EQ(lv_arc_buyback(), 0xA5C3u);
    lv_arc_set_choice(0, 200); lv_arc_set_choice(1, 7); lv_arc_set_choice(2, 9);
    CHECK_EQ(lv_arc_choice(0), 200);
    CHECK_EQ(lv_arc_choice(1), 7);
    CHECK_EQ(lv_arc_choice(2), 0);
    CHECK_EQ(lv_arc_last_ending(), 255);                 /* none */
    lv_arc_set_last_ending(0);
    CHECK_EQ(lv_arc_last_ending(), 0);                   /* E01 is a real ending */
    lv_arc_set_last_ending(7);
    CHECK_EQ(lv_arc_last_ending(), 7);
    lv_arc_set_line4(3);
    CHECK_EQ(lv_arc_line4(), 3);
    /* the held residents: a set of at most 16, order kept, a freed place refilled */
    CHECK_EQ(lv_arc_held(0), (unsigned)LV_ARC_NOBODY);
    CHECK(lv_arc_hold(100) && lv_arc_hold(729) && lv_arc_hold(100));
    CHECK_EQ(lv_arc_held_count(), 2);
    CHECK(lv_arc_held(0) == 100u && lv_arc_held(1) == 729u);
    lv_arc_release(100);
    CHECK(lv_arc_held_count() == 1 && lv_arc_held(0) == 729u && lv_arc_held(1) == LV_ARC_NOBODY);
    lv_arc_release(5);                                   /* not held: nothing */
    CHECK_EQ(lv_arc_held_count(), 1);
    {
        unsigned r;
        for (r = 1; r < 40; r++) lv_arc_hold(r);
        CHECK_EQ(lv_arc_held_count(), LV_ARC_HELD);
        CHECK(!lv_arc_hold(500));                        /* full */
        CHECK(!lv_arc_hold(LV_ARC_NOBODY));
    }
}

/* ---- the chunk ------------------------------------------------------------------------ */
static void fill_records(void)
{
    lv_set_rank(LV_HB8, RANK_C);
    lv_set_rank(LV_HB13, RANK_S);
    lv_rec_set_gold(LV_HB13);
    lv_rec_add_feats(LV_HB13, 6);
    lv_rec_set_cleared(LV_HB13);
    lv_rec_set_secrets(LV_HB13, 9);
    lv_rec_add_collect(LV_HB13, 0xBEEF);
    lv_set_rank(LV_N1, RANK_A);
    lv_set_rank(LV_N7, RANK_B);
    lv_rec_add_collect(LV_N7, 3);
    lv_set_rank(LV_N14, RANK_S);                         /* on the GBA's list */
    lv_set_rank(LV_N15, RANK_A);                         /* not on it */
    lv_set_rank(LV_HB10, RANK_B);                        /* not on it */
    lv_arc_set_act(LVA_ACT3);
    lv_arc_set_flag(3, 1); lv_arc_set_flag(14, 1);
    lv_arc_set_buyback(0x0F0F);
    lv_arc_set_choice(0, 2); lv_arc_set_choice(1, 1);
    lv_arc_hold(12); lv_arc_hold(729);
    lv_arc_set_last_ending(5);
    lv_arc_set_line4(2);
}

static void check_records(int all)
{
    CHECK_EQ(lv_rank(LV_HB8), RANK_C);
    CHECK_EQ(lv_rank(LV_HB13), RANK_S);
    CHECK(lv_gold(LV_HB13) && !lv_gold(LV_HB8));
    CHECK_EQ(lv_rec_feats(LV_HB13), 6);
    CHECK(lv_rec_cleared(LV_HB13));
    CHECK_EQ(lv_rec_secrets(LV_HB13), 9);
    CHECK_EQ(lv_rec_collect(LV_HB13), 0xBEEFu);
    CHECK_EQ(lv_rank(LV_N1), RANK_A);
    CHECK_EQ(lv_rank(LV_N7), RANK_B);
    CHECK_EQ(lv_rec_collect(LV_N7), 3u);
    CHECK_EQ(lv_rank(LV_N14), RANK_S);
    CHECK_EQ(lv_rank(LV_N15), all ? RANK_A : RANK_NONE);
    CHECK_EQ(lv_rank(LV_HB10), all ? RANK_B : RANK_NONE);
    CHECK_EQ(lv_rank(LV_HB9), RANK_NONE);
    CHECK_EQ(lv_arc_act(), LVA_ACT3);
    CHECK(lv_arc_flag(3) && lv_arc_flag(14) && !lv_arc_flag(4));
    CHECK_EQ(lv_arc_buyback(), 0x0F0Fu);
    CHECK(lv_arc_choice(0) == 2 && lv_arc_choice(1) == 1);
    CHECK(lv_arc_held_count() == 2 && lv_arc_held(0) == 12u && lv_arc_held(1) == 729u);
    CHECK_EQ(lv_arc_last_ending(), 5);
    CHECK_EQ(lv_arc_line4(), 2);
}

TEST(levels_brch_round_trip_through_a_save)
{
    uint32_t n;
    const DoomoSaveExt *e = doomo_save_ext_breach;
    reset();
    CHECK(e != NULL && e->tag == SAVE_EXT_TAG_BRCH);
    CHECK_EQ_U(SAVE_EXT_TAG_BRCH, SAVE_EXT_TAG('B', 'R', 'C', 'H'));
    CHECK_EQ(e->size(), 0u);                             /* nothing recorded: an empty chunk */
    fill_records();
    CHECK_EQ(e->size(), (uint32_t)LV_BRCH_MAX);          /* 4 + 48 + 24 x 4 */
    CHECK_EQ(LV_BRCH_MAX, 148);
    CHECK(e->max_size >= e->size() && e->max_size == SAVE_EXT_MAX_BRCH);
    CHECK_EQ(save_budget_declared_max(SAVE_EXT_TAG_BRCH), 160u);
    n = save_serialize(image, sizeof(image));
    CHECK(n > 0);
    reset();                                        /* a fresh island: nothing is recorded */
    CHECK_EQ(lv_rank(LV_HB13), RANK_NONE);
    CHECK_EQ(lv_arc_act(), LVA_NONE);
    CHECK(save_deserialize(image, n));
    check_records(1);
    /* saved again, byte for byte the same chunk */
    {
        uint8_t a[SAVE_EXT_MAX_BRCH], b[SAVE_EXT_MAX_BRCH];
        CHECK_EQ(e->write(a, sizeof(a)), (uint32_t)LV_BRCH_MAX);
        n = save_serialize(image, sizeof(image));
        CHECK(n > 0 && save_deserialize(image, n));
        CHECK_EQ(e->write(b, sizeof(b)), (uint32_t)LV_BRCH_MAX);
        CHECK(memcmp(a, b, LV_BRCH_MAX) == 0);
        CHECK_EQ(a[0], LV_BRCH_VERSION);
        CHECK_EQ(a[1] | (a[2] << 8) | (a[3] << 16), 0xFFFFFF);   /* a record for every new level */
    }
    /* a fresh island writes the empty chunk and loads clean */
    reset();
    n = save_serialize(image, sizeof(image));
    CHECK(n > 0);
    fill_records();
    CHECK(save_deserialize(image, n));
    CHECK_EQ(lv_rank(LV_HB13), RANK_NONE);
    CHECK_EQ(lv_arc_held_count(), 0);
    CHECK_EQ(e->size(), 0u);
}

TEST(levels_brch_on_the_gba_is_smaller_and_cross_loads)
{
    uint8_t all[SAVE_EXT_MAX_BRCH], gba[SAVE_EXT_MAX_BRCH];
    const DoomoSaveExt *e = doomo_save_ext_breach;
    uint32_t na, ng;
    reset();
    fill_records();
    na = e->write(all, sizeof(all));
    CHECK_EQ(na, (uint32_t)LV_BRCH_MAX);
    /* the GBA's list: the same records of its seven only, 4 + 48 + 28 = 80 B */
    lv_list_req = LVG_GBA;
    CHECK(e->read(all, na, LV_BRCH_VERSION));            /* a save from another platform loads: foreign records skipped */
    check_records(0);
    ng = e->write(gba, sizeof(gba));
    CHECK_EQ(ng, 4u + LV_BRCH_ARC + 7u * 4u);
    CHECK_EQ(ng, 80u);
    /* the mask: bit = level - 7 of HB8, HB9, HB13, HB15, N1, N7, N14 */
    CHECK_EQ(gba[1] | (gba[2] << 8) | (gba[3] << 16), (1 << 0) | (1 << 1) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 15) | (1 << 22));
    lv_list_req = 0;
    /* ...and a GBA save loads on a platform that lists them all, each record on its level */
    CHECK(e->read(gba, ng, LV_BRCH_VERSION));
    check_records(0);
    CHECK_EQ(lv_rank(LV_HB10), RANK_NONE);
    CHECK_EQ(e->size(), (uint32_t)LV_BRCH_MAX);          /* every new level has a place again */
}

TEST(levels_brch_rejects_what_it_cannot_read)
{
    uint8_t buf[SAVE_EXT_MAX_BRCH];
    const DoomoSaveExt *e = doomo_save_ext_breach;
    uint32_t n;
    reset();
    fill_records();
    n = e->write(buf, sizeof(buf));
    CHECK_EQ(n, (uint32_t)LV_BRCH_MAX);
    /* each bad chunk reads 0 and leaves nothing behind */
#define BAD(expr) do { reset(); fill_records(); CHECK(!(expr)); CHECK_EQ(e->size(), 0u); } while (0)
    BAD(e->read(buf, n, 2));                             /* a version this build does not know */
    BAD(e->read(buf, n, 0));
    BAD(e->read(buf, n - 1, LV_BRCH_VERSION));           /* cut short */
    BAD(e->read(buf, 0, LV_BRCH_VERSION));               /* empty */
    BAD(e->read(buf, LV_BRCH_HDR + LV_BRCH_ARC - 1, LV_BRCH_VERSION));
    {
        uint8_t bad[SAVE_EXT_MAX_BRCH];
        memcpy(bad, buf, n);
        bad[LV_BRCH_HDR] = LVA_DONE + 1;                 /* an act that does not exist */
        BAD(e->read(bad, n, LV_BRCH_VERSION));
        memcpy(bad, buf, n);
        bad[LV_BRCH_HDR + 9] = LV_ARC_HELD + 1;          /* more held residents than fit */
        BAD(e->read(bad, n, LV_BRCH_VERSION));
        memcpy(bad, buf, n);
        bad[1] = 0;                                      /* the mask says fewer records than the bytes hold */
        BAD(e->read(bad, n, LV_BRCH_VERSION));
    }
#undef BAD
    /* a write into too small a room writes nothing */
    reset();
    fill_records();
    CHECK_EQ(e->write(buf, 10), 0u);
}

TEST(levels_brch_budget_holds_in_both_slots)
{
    /* the joint worst case with BRCH on top of K0's five and TALK's (the other
     * tests hold K0 and TALK; here all three together) */
    uint32_t home = (HOME_CHUNK_WORST * 129u + 127u) / 128u + SAVE_BUDGET_CHUNK_HDR;
    uint32_t story = 19342u + home + SAVE_BUDGET_K0_WORST + SAVE_BUDGET_TALK_WORST + SAVE_BUDGET_BRCH_WORST;
    uint32_t fp = SAVE_BUDGET_TOWN_BOUND_FP + home + SAVE_BUDGET_K0_WORST + SAVE_BUDGET_TALK_WORST + SAVE_BUDGET_BRCH_WORST;
    CHECK_EQ(SAVE_EXT_SLOTS, 14);
    CHECK_EQ(SAVE_EXT_MAX_BRCH, 160);
    CHECK(LV_BRCH_MAX <= SAVE_EXT_MAX_BRCH);
    CHECK_EQ(SAVE_BUDGET_BRCH_WORST, SAVE_BUDGET_LZ_WORST(160u) + SAVE_BUDGET_CHUNK_HDR);
    printf("      joint worst with TALK + BRCH: story %u B, free play %u B\n", (unsigned)story, (unsigned)fp);
    CHECK(story + SAVE_BUDGET_MARGIN <= SAVE_SLOT_STORY_CAP_CFG - 4096u);
    CHECK(fp + SAVE_BUDGET_MARGIN <= SAVE_SLOT_FP_CAP);
    /* a registered chunk is counted: its worst case lands in the live budget's bound */
    CHECK_EQ(save_ext_at(save_ext_count() - 1) == doomo_save_ext_breach, 1);
}

/* ---- the unlock rules ------------------------------------------------------------------ */
TEST(levels_unlock_rules)
{
    char buf[64];
    int i;
    reset();
    /* the old seven: as before LV0 */
    CHECK(lv_unlocked(LV_VAPE_CELLAR) && lv_unlocked(LV_CLOUD_NINE) == 0 && lv_unlocked(LV_SEWERS) == 0);
    CHECK_EQ(lv_prereq_of(LV_FACTORY), LV_SEWERS);
    CHECK_EQ(lv_prereq_of(LV_VAPE_CELLAR), -1);
    CHECK_EQ(lv_prereq_of(LV_HB8), -1);                  /* a story rule, not a level */
    CHECK_EQ(lv_prereq_of(LV_N8), LV_N7);
    lv_locked_reason(LV_CLOUD_NINE, buf, sizeof(buf));
    CHECK_STR(buf, "Clear The Vape Cellar first");
    lv_locked_reason(LV_VAPE_CELLAR, buf, sizeof(buf));
    CHECK_STR(buf, "");
    lv_set_rank(LV_VAPE_CELLAR, RANK_C);
    CHECK(lv_unlocked(LV_CLOUD_NINE) && lv_unlocked(LV_SEWERS) && !lv_unlocked(LV_FACTORY));
    /* the rule kinds */
    CHECK(lv_rule_eval(LVK_ALWAYS, 0));
    CHECK(!lv_rule_eval(LVK_AFTER_LEVEL, LV_HB9) && !lv_rule_eval(LVK_AFTER_LEVEL, 99) && !lv_rule_eval(LVK_AFTER_LEVEL, -1));
    CHECK(lv_rule_eval(LVK_AFTER_LEVEL, LV_VAPE_CELLAR));
    CHECK(!lv_rule_eval(LVK_CHAPTER, STORY_CH2));
    doomo_town.story.chapter = STORY_CH2;
    CHECK(lv_rule_eval(LVK_CHAPTER, STORY_CH2) && !lv_rule_eval(LVK_CHAPTER, STORY_CH3));
    CHECK(!lv_rule_eval(LVK_FLAG, SF_NGPLUS) && !lv_rule_eval(LVK_NGPLUS, 0));
    story_set_flag(SF_NGPLUS, 1);
    CHECK(lv_rule_eval(LVK_FLAG, SF_NGPLUS) && lv_rule_eval(LVK_NGPLUS, 0));
    story_set_flag(SF_NGPLUS, 0);
    CHECK(!lv_rule_eval(LVK_NGPLUS, 0));
    lv_arc_set_act(LVA_DONE);                            /* the arc was finished: New Game+ stays open */
    CHECK(lv_rule_eval(LVK_NGPLUS, 0));
    lv_arc_set_act(LVA_NONE);
    /* by episode: the done bit, or a later chapter (an old save further on) */
    reset();
    CHECK(!lv_rule_eval(LVK_BEAT, BEAT_EP2_6));
    story_episode_set_done(BEAT_EP2_6, 1);
    CHECK(lv_rule_eval(LVK_BEAT, BEAT_EP2_6) && !lv_rule_eval(LVK_BEAT, BEAT_EP3_4));
    story_episode_set_done(BEAT_EP2_6, 0);
    CHECK(!lv_rule_eval(LVK_BEAT, BEAT_EP2_6));
    doomo_town.story.chapter = STORY_CH2;
    CHECK(!lv_rule_eval(LVK_BEAT, BEAT_EP2_6));          /* the same chapter is not enough */
    doomo_town.story.chapter = STORY_CH3;
    CHECK(lv_rule_eval(LVK_BEAT, BEAT_EP2_6) && !lv_rule_eval(LVK_BEAT, BEAT_EP3_4) && !lv_rule_eval(LVK_BEAT, BEAT_EP4_1));
    doomo_town.story.chapter = STORY_CH_DONE;
    CHECK(lv_rule_eval(LVK_BEAT, BEAT_EP4_5));
    /* the table: every rule of a new level is evaluable, no id out of range, every reason has text */
    for (i = LV_HB8; i < LV_COUNT; i++) {
        const LevelReg *r = &lv_reg[i];
        CHECK_MSG(lv_reason_text[r->reason] != NULL && (r->reason == LVT_NONE) == (r->rule == LVK_ALWAYS), "level %d", i);
        CHECK_MSG(r->rule != LVK_AFTER_LEVEL || r->arg < i, "level %d waits for a later one", i);
        CHECK_MSG(r->rule != LVK_AFTER_LEVEL || lv_reg[r->arg].page == r->page, "level %d waits across pages", i);
        CHECK_MSG(r->rule != LVK_BEAT || (r->arg >= BEAT_EP1_1 && r->arg <= BEAT_EP4_7), "level %d beat", i);
    }
    /* the Notices need New Game+ on top of their own rule; the main game's levels do not */
    reset();
    doomo_town.story.chapter = STORY_CH_DONE;
    for (i = LV_N2; i <= LV_N6; i++) CHECK_MSG(!lv_rule_met(i), "notice %d without New Game+", i - LV_N1 + 1);
    CHECK(lv_rule_met(LV_HB16));
    story_set_flag(SF_NGPLUS, 1);
    CHECK(lv_rule_met(LV_N1) && lv_rule_met(LV_N2) && lv_rule_met(LV_N6));
    CHECK(!lv_rule_met(LV_N7));                          /* after Notice 6 is cleared */
    lv_set_rank(LV_N6, RANK_C);
    CHECK(lv_rule_met(LV_N7) && !lv_rule_met(LV_N8));
    /* New Game+ off again: the page closes */
    story_set_flag(SF_NGPLUS, 0);
    CHECK(!lv_rule_met(LV_N7));
    /* the text says why: its package builds the map first */
    lv_locked_reason(LV_N7, buf, sizeof(buf));
    CHECK_STR(buf, "Not built yet");
    CHECK(!lv_unlocked(LV_N7) && !lv_unlocked(LV_HB16));     /* no map: never open, never startable */
    CHECK_EQ(lv_startable(LV_N7), LV_UNDERBELLY);
    CHECK_EQ(lv_startable(LV_CATHEDRAL), LV_CATHEDRAL);
    CHECK_EQ(lv_startable(200), LV_UNDERBELLY);
}

TEST(levels_unlock_rules_on_the_gba_follow_the_chain)
{
    reset();
    lv_list_req = LVG_GBA;
    doomo_town.story.chapter = STORY_CH_DONE;
    story_set_flag(SF_NGPLUS, 1);
    /* Notice 6 is not on the cartridge: Notice 7 opens by its chain (the chapter rule of 6) */
    CHECK(lv_rule_met(LV_N7));
    CHECK(!lv_rule_met(LV_N14));                         /* Notice 13..8 stand in: Notice 7 first */
    lv_set_rank(LV_N7, RANK_C);
    CHECK(lv_rule_met(LV_N14));
    /* the chain stops at New Game+: without it nothing of the Arrears page opens */
    story_set_flag(SF_NGPLUS, 0);
    CHECK(!lv_rule_met(LV_N7) && !lv_rule_met(LV_N14) && !lv_rule_met(LV_N1));
    CHECK_EQ(lv_listed_count(), 14);
    lv_list_req = 0;
    CHECK(!lv_rule_met(LV_N14));                         /* the same chain on a console that has them all: Notice 13 first */
}

/* ---- the select's pages ------------------------------------------------------------------ */
static LevelInfo saved_info[LV_COUNT];

static void built_all(int on)
{
    int i;
    if (on) {
        memcpy(saved_info, lv_info, sizeof(saved_info));
        for (i = LV_OLD_COUNT; i < LV_COUNT; i++) lv_info[i].map = MAPID_VAPE_CELLAR;
    } else {
        memcpy(lv_info, saved_info, sizeof(saved_info));
    }
}

static int started_level(void) { return lv_run.level; }

/* press A on the select, read which level started, come back */
static int press_a(void)
{
    int lv;
    frame(BTN_A); frame(0);
    if (scene_top() != &SCENE_BREACH) return -1;
    lv = started_level();
    scene_pop();
    frame(0); frame(0);
    return lv;
}

TEST(levels_select_shows_only_built_levels_on_one_page)
{
    int i;
    reset();
    CHECK_EQ(lv_listed_count(), 31);
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_LEVEL_SELECT, LVM_NORMAL);
    frame(0); frame(0);
    CHECK(scene_top() == &SCENE_LEVEL_SELECT);
    /* nothing new is built: UP / DOWN find no other page, LEFT / RIGHT walk the seven and wrap */
    frame(BTN_DOWN); frame(0);
    frame(BTN_UP); frame(0);
    CHECK_EQ(press_a(), LV_VAPE_CELLAR);                 /* the cellar is the only open one: still it */
    for (i = 0; i < 7; i++) { frame(BTN_RIGHT); frame(0); }
    CHECK_EQ(press_a(), LV_VAPE_CELLAR);                 /* seven steps: round once */
    frame(BTN_LEFT); frame(0);
    frame(BTN_A); frame(0);                              /* the underbelly: locked */
    CHECK(scene_top() == &SCENE_LEVEL_SELECT);
    scene_set(&SCENE_TITLE, 0);
}

TEST(levels_select_pages_both_sizes)
{
    int res, i, mask = 0;
    reset();
    built_all(1);
    for (i = 0; i < LV_COUNT; i++) lv_set_rank(i, RANK_C);   /* every level is open */
    for (res = 0; res < 2; res++) {
        nat_w = res ? 320 : 240;
        nat_h = res ? 240 : 160;
        scene_set(&SCENE_TITLE, 0);
        scene_push(&SCENE_LEVEL_SELECT, LVM_NORMAL | ((LV_HB8 + 1) << 4));   /* focus a level of page 2 */
        frame(0); frame(0);
        CHECK(scene_top() == &SCENE_LEVEL_SELECT);
        CHECK_EQ(press_a(), LV_HB8);
        for (i = 0; i < 12; i++) frame(0);
        shot(res ? "page_hell_320x240" : "page_hell_240x160");
        /* LEFT / RIGHT stay on the page and wrap: nine levels, HB8..HB16 */
        for (i = 0; i < 9; i++) { frame(BTN_RIGHT); frame(0); }
        CHECK_EQ(press_a(), LV_HB8);
        frame(BTN_LEFT); frame(0);
        CHECK_EQ(press_a(), LV_HB16);
        /* DOWN: the Arrears page, its first level that has not been played; here all are, so Notice 1 */
        frame(BTN_DOWN); frame(0);
        CHECK_EQ(press_a(), LV_N1);
        for (i = 0; i < 12; i++) frame(0);
        shot(res ? "page_arrears_320x240" : "page_arrears_240x160");
        for (i = 0; i < 15; i++) { frame(BTN_RIGHT); frame(0); }
        CHECK_EQ(press_a(), LV_N1);
        frame(BTN_LEFT); frame(0);
        CHECK_EQ(press_a(), LV_N15);
        /* DOWN again: round to the first page; UP goes back */
        frame(BTN_DOWN); frame(0);
        CHECK(press_a() < LV_OLD_COUNT);
        frame(BTN_UP); frame(0);
        CHECK_EQ(press_a(), LV_N1);
        frame(BTN_UP); frame(0);
        CHECK_EQ(press_a(), LV_HB8);
        for (i = 0; i < 12; i++) frame(0);
        /* the hint line with the page keys fits the screen */
        {
            char hint[96];
            snprintf(hint, sizeof(hint), "%s", res ? STR_ICON_ARROW_L STR_ICON_ARROW_R " Level  " STR_ICON_ARROW_U STR_ICON_ARROW_D
                                                      " Page  " STR_ICON_A " Play  " STR_ICON_B " Back"
                                                    : STR_ICON_ARROW_U STR_ICON_ARROW_D " Page  " STR_ICON_A " Play  " STR_ICON_B " Back");
            CHECK(ui_text_width(hint) + 12 <= nat_w);
        }
        mask |= 1 << res;
    }
    CHECK_EQ(mask, 3);
    nat_w = 240;
    nat_h = 160;
    scene_set(&SCENE_TITLE, 0);
    built_all(0);
}

TEST(levels_select_focus_a_level_that_is_not_built_falls_back)
{
    reset();
    lv_set_rank(LV_VAPE_CELLAR, RANK_C);
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_LEVEL_SELECT, LVM_NORMAL | ((LV_N5 + 1) << 4));   /* not built: the default focus */
    frame(0); frame(0);
    CHECK(scene_top() == &SCENE_LEVEL_SELECT);
    CHECK_EQ(press_a(), LV_SEWERS);                      /* the first open level not played */
    scene_set(&SCENE_TITLE, 0);
}
