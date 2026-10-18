/* test_exorcism_authored.c - authored possessors (owner: S3; SPEC S3 M1,
 * story_final.md sec.1 and sec.15.11): the table, the override hook the sim
 * reads, the name board (answer, near miss, herring, never offered), the
 * scripted loss, the reverse rite and the practice rows. */
#include "test.h"
#include "../src/sim_int.h"
#include "../src/story_int.h"     /* SF_C3_EXO0 */

static void story_town(void)
{
    town_new(TOWN_STORY, 21, TOWN_POP_PREMADE);
    sim_ensure_init();
}

static int row_of(const char *name, int host)
{
    int i;
    for (i = 0; i < doomo_possess_count; i++)
        if (doomo_possess_table[i].name && strcmp(doomo_possess_table[i].name, name) == 0 &&
            doomo_possess_table[i].host == host)
            return i;
    return -1;
}

TEST(possess_table_valid)
{
    int i, k;
    CHECK(doomo_possess_count >= 12);
    for (i = 0; i < doomo_possess_count; i++) {
        const DoomoPossessor *p = &doomo_possess_table[i];
        int n = 0;
        CHECK_MSG(p->host < DOOMO_MAX_RESIDENTS || p->host >= EXO_HOST_PRACTICE, "row %d host", i);
        CHECK_MSG(p->name_res == EXO_NONE || p->name_res < DOOMO_MAX_RESIDENTS, "row %d name_res", i);
        CHECK(p->skin < EXS_COUNT);
        CHECK(p->near_miss == EXO_NO_PICK || p->near_miss < 4);
        CHECK(p->herring == EXO_NO_PICK || p->herring < 4);
        for (k = 0; k < 4; k++) if (p->offer[k]) n++;
        CHECK_MSG(n == 0 || n == 4, "row %d offers 1-3 names", i);
        if (n == 4 && p->name) {
            int found = 0;
            for (k = 0; k < 4; k++) if (strcmp(p->offer[k], p->name) == 0) found = 1;
            /* the true name is on the board unless the loss is scripted */
            CHECK_MSG(found == !(p->flags & EXF_SCRIPTED_FAIL), "row %d (%s)", i, p->name);
        }
        if (p->name_res != EXO_NONE && p->name && p->name_res > 0) {
            const char *rn = roster_name(p->name_res);
            for (k = 0; p->name[k] && rn[k]; k++)
                if ((rn[k] & ~0x20) != (p->name[k] & ~0x20)) break;
            CHECK_MSG(!p->name[k] && !rn[k], "row %d: %s is not %s", i, p->name, rn);
        }
    }
    /* story_final.md sec.1 */
    CHECK(row_of("TALLYWORN", 2) >= 0);
    CHECK(row_of("WHEEZLE", 81) >= 0);
    CHECK(row_of("HAZE", 71) >= 0);
    CHECK(row_of("SOOTKIN", 88) >= 0);
    CHECK(row_of("FLUFFY", 502) >= 0);
}

TEST(possessor_override_names_tallyworn)
{
    int row = row_of("TALLYWORN", 2), ans, k;
    char buf[32];
    story_town();
    CHECK(story_possessor_override(2) == -1);           /* not possessed */
    exorcism_possess(2, row);
    CHECK_EQ(town_res(2)->status, RES_POSSESSED);
    CHECK_EQ(exorcism_authored_row(2), row);
    CHECK_EQ(story_possessor_override(2), 712);
    CHECK_EQ(sim_possessor_of(2), 712);                  /* the sim's pick follows */
    ans = exorcism_name_answer(2);
    CHECK(ans >= 0 && ans < 4);
    CHECK_STR(exorcism_name_text(2, ans, buf, sizeof(buf)), "TALLYWORN");
    CHECK_STR(exorcism_clue(2, 0, buf, sizeof(buf)), "a chalk stub");
    CHECK_EQ(exorcism_name_pick(2, ans), EXP_RIGHT);
    for (k = 0; k < 4; k++) if (k != ans) CHECK_EQ(exorcism_name_pick(2, k), EXP_WRONG);
    CHECK(exorcism_forced_result(2) < 0);
    exorcism_apply(2, 1);
    CHECK_EQ(town_res(2)->status, RES_ALIVE);
    CHECK_EQ(exorcism_authored_row(2), -1);              /* disarmed */
}

TEST(possessor_survives_a_reload_in_story_mode)
{
    int row = row_of("HAZE", 71);
    story_town();
    exorcism_possess(71, row);
    exorcism_unpossess(71);
    CHECK_EQ(town_res(71)->status, RES_ALIVE);
    CHECK_EQ(exorcism_authored_row(71), -1);
    /* a possession without an armed slot is the sim's own pick, also for a
     * story host in story mode (sim_possession_cap from Ch2): never a guess */
    town_res(71)->status = RES_POSSESSED;
    CHECK_EQ(exorcism_authored_row(71), -1);
    CHECK_EQ(story_possessor_override(71), -1);
    /* a reload inside the story's own window (Hattie, Ch3 nests): the slots
     * are not saved, the story re-arms them from the @armed rows (S2) */
    doomo_town.story.quest = BEAT_C3_NESTS;
    exorcism_slots_clear();
    story_rearm_possessors();
    CHECK_EQ(exorcism_authored_row(71), row);
    CHECK_EQ(story_possessor_override(71), 709);
    /* ... but not once the story is past it, or its flag says it is over */
    doomo_town.story.quest = BEAT_C4_GATE;
    story_rearm_possessors();
    CHECK_EQ(exorcism_authored_row(71), -1);
    doomo_town.story.quest = BEAT_C3_NESTS;
    story_set_flag(SF_C3_EXO0, 1);
    story_rearm_possessors();
    CHECK_EQ(exorcism_authored_row(71), -1);
    story_set_flag(SF_C3_EXO0, 0);
    /* a slot left over from a story session never reaches free play */
    story_rearm_possessors();
    CHECK_EQ(exorcism_authored_row(71), row);
    doomo_town.mode = TOWN_FREEPLAY;
    CHECK_EQ(exorcism_authored_row(71), -1);
    /* and free play never re-arms */
    story_rearm_possessors();
    doomo_town.mode = TOWN_STORY;
    CHECK_EQ(exorcism_authored_row(71), -1);
}

/* every @armed row (S2, the .dsc packs) names the host of its possessor row */
TEST(story_armed_rows_match_the_possessor_table)
{
    int i;
    CHECK(doomo_story_armed_count >= 4);
    for (i = 0; i < doomo_story_armed_count; i++) {
        const StoryArmed *a = &doomo_story_armed[i];
        CHECK_MSG(a->row < doomo_possess_count, "@armed %d: row %d", i, a->row);
        CHECK_MSG(doomo_possess_table[a->row].host == a->host, "@armed %d: row %d is not host %d's", i, a->row, a->host);
        CHECK_MSG(story_possess_row_of(a->host) == a->row, "@armed %d: host %d's first row", i, a->host);
        CHECK(story_episode_order(a->from) >= 0 && story_episode_order(a->to) >= story_episode_order(a->from));
    }
}

TEST(wheezle_is_never_offered_and_lost_by_design)
{
    int row = row_of("WHEEZLE", 81), k;
    char buf[32];
    story_town();
    exorcism_possess(81, row);
    CHECK_EQ(exorcism_name_answer(81), -1);
    for (k = 0; k < 4; k++) {
        CHECK(strcmp(exorcism_name_text(81, k, buf, sizeof(buf)), "WHEEZLE") != 0);
        CHECK_EQ(exorcism_name_pick(81, k), EXP_WRONG);
    }
    CHECK_EQ(exorcism_forced_result(81), 0);
    CHECK_EQ(exorcism_odds_with(81, 0x3F), exorcism_odds_with(81, 0));   /* fixed odds */
    exorcism_apply(81, 0);
    CHECK_EQ(town_res(81)->status, RES_SOUL_LOST);       /* one scripted fail */
    CHECK_EQ(story_possessor_override(81), 707);
}

TEST(sprinklepop_reverse_rite_unfails)
{
    int row = row_of("WHEEZLE", 81), rev = -1, i;
    story_town();
    for (i = 0; i < doomo_possess_count; i++)
        if (doomo_possess_table[i].flags & EXF_UNFAIL) rev = i;
    CHECK(rev >= 0);
    CHECK(doomo_possess_table[rev].flags & EXF_TYPE_NAME);
    exorcism_possess(81, row);
    exorcism_apply(81, 0);
    CHECK_EQ(town_res(81)->status, RES_SOUL_LOST);
    exorcism_possess(81, rev);                           /* the soul stays lost until... */
    CHECK_EQ(town_res(81)->status, RES_SOUL_LOST);
    CHECK_EQ(exorcism_authored_row(81), rev);
    exorcism_apply(81, 0);                               /* a failed reverse rite changes nothing */
    CHECK_EQ(town_res(81)->status, RES_SOUL_LOST);
    exorcism_apply(81, 1);                               /* EXORCISM UNFAILED x1 */
    CHECK_EQ(town_res(81)->status, RES_ALIVE);
    CHECK_EQ(exorcism_fails(81), 0);
}

TEST(smog_near_miss_and_herring)
{
    int row = row_of("FLUFFY", 502);
    char buf[32];
    story_town();
    exorcism_possess(502, row);
    CHECK_STR(exorcism_name_text(502, 0, buf, sizeof(buf)), "VAPULA");
    CHECK(!story_flag(SF_SMOG_NEAR_MISS));
    CHECK_EQ(exorcism_name_pick(502, 0), EXP_NEAR_MISS);
    CHECK(story_flag(SF_SMOG_NEAR_MISS));
    CHECK_EQ(exorcism_name_pick(502, 1), EXP_HERRING);   /* GIGGLESWORTH */
    CHECK_EQ(exorcism_name_pick(502, 2), EXP_RIGHT);     /* FLUFFY */
    CHECK_EQ(exorcism_name_pick(502, 3), EXP_WRONG);
    CHECK_EQ(story_possessor_override(502), -1);         /* FLUFFY is not a citizen */
}

TEST(authored_losses_set_their_flags)
{
    int row = row_of("HAZE", 71), i;
    story_town();
    exorcism_possess(71, row);
    for (i = 0; i < DOOMO_EXORCISM_FAILS_TO_LOSE_SOUL; i++) exorcism_apply(71, 0);
    CHECK_EQ(town_res(71)->status, RES_SOUL_LOST);
    CHECK(story_flag(SF_HATTIE_LOST));
    CHECK(!story_flag(SF_GRAMPY_LOST));
}

TEST(practice_rows_and_stale_slots)
{
    int prac = -1, i;
    story_town();
    for (i = 0; i < doomo_possess_count; i++)
        if (doomo_possess_table[i].host == EXO_HOST_PRACTICE) { prac = i; break; }
    CHECK(prac >= 0);
    exorcism_possess(-1, prac);
    CHECK_EQ(exorcism_authored_row(-1), prac);
    CHECK_EQ(exorcism_authored_row(-5), prac);           /* any practice plush */
    exorcism_unpossess(-1);
    CHECK_EQ(exorcism_authored_row(-1), -1);
    /* a slot armed before a new game is ignored once the host is fine */
    exorcism_possess(2, row_of("TALLYWORN", 2));
    story_town();
    CHECK_EQ(exorcism_authored_row(2), -1);
    CHECK(sim_possessor_of(2) == -1);
}

TEST(free_play_possession_keeps_the_sims_pick)
{
    int p;
    town_new(TOWN_FREEPLAY, 99, TOWN_POP_PREMADE);
    sim_ensure_init();
    town_res(2)->status = RES_POSSESSED;
    CHECK_EQ(story_possessor_override(2), -1);
    p = sim_possessor_of(2);
    CHECK(p >= DOOMO_DISTRICT_SIZE);
    CHECK(exorcism_authored(2) == NULL);
}
