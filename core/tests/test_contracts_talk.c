/* test_contracts_talk.c - what T0 fixes for Talk (docs/TALK.md 9, T0):
 * the act ids, TalkUtter, the PLATF_TALK bit, the room view's talk
 * framing fields and plat_room_project, the 'TALK' save chunk's tag and
 * budget, the NT_TALK_* news. Owner: TK.
 *
 * Every check holds before and after the later packages land: an id stays
 * where T0 put it, and a hook that is no longer the weak default is left to
 * its owner's tests.
 */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/plat_ext.h"
#include "doomo/talk.h"
#include "doomo/save_budget.h"
#include "doomo/home.h"
#include "../src/home_int.h"
#include "../src/sim_int.h"

TEST(talk_contract_acts_and_utter)
{
    /* the act ids are saved in talk memory: these never move */
    CHECK_EQ(TA_NONE, 0);
    CHECK_EQ(TA_GREET, 1);
    CHECK_EQ(TA_ASK_ABOUT, 6);
    CHECK_EQ(TA_TELL, 14);
    CHECK_EQ(TA_COMPLIMENT, 22);
    CHECK_EQ(TA_INSULT, 32);
    CHECK_EQ(TA_GOSSIP, 40);
    CHECK_EQ(TA_CHANGE_TOPIC, 45);
    CHECK_EQ(TA_UNKNOWN, 48);
    CHECK_EQ(TA_FLIRT, 49);
    CHECK_EQ(TA_KISS, 52);
    CHECK_EQ(TA_WAVE, 53);
    CHECK_EQ(TA_HIGH_FIVE, 59);
    CHECK_EQ(TA_REFUSE_SEXUAL, 60);
    CHECK_EQ(TA_DEFLECT_ROMANCE, 64);
    CHECK_EQ(TA_COUNT, 65);
    CHECK(TA_COUNT <= 255);                          /* TalkUtter.act is a byte */
    /* the groups the gate tests by range */
    CHECK(TA_IS_ROMANCE(TA_FLIRT) && TA_IS_ROMANCE(TA_CONFESS) && TA_IS_ROMANCE(TA_ASK_OUT) &&
          TA_IS_ROMANCE(TA_KISS));
    CHECK(!TA_IS_ROMANCE(TA_HUG) && !TA_IS_ROMANCE(TA_COMPLIMENT) && !TA_IS_ROMANCE(TA_UNKNOWN));
    CHECK(TA_IS_GESTURE(TA_HUG) && TA_IS_GESTURE(TA_HIGH_FIVE) && !TA_IS_GESTURE(TA_KISS));
    CHECK(TA_IS_SAFETY(TA_REFUSE_MINOR) && TA_IS_SAFETY(TA_DISTRESS) && !TA_IS_SAFETY(TA_NONE));
    CHECK_EQ(TONE_FLIRTY, 4);
    CHECK_EQ(TONE_COUNT, 5);
    /* TS_* are distinct single bits of a byte */
    {
        static const unsigned ts[] = { TS_SEXUAL, TS_MINOR, TS_HATE, TS_DISTRESS, TS_REALWORLD,
                                       TS_STRONG, TS_CRUDE, TS_MASKED };
        unsigned all = 0;
        int i;
        for (i = 0; i < DOOMO_COUNTOF(ts); i++) {
            CHECK(ts[i] && !(ts[i] & (ts[i] - 1)) && !(all & ts[i]));
            all |= ts[i];
        }
        CHECK_EQ_U(all, 0xFFu);
    }
    CHECK_EQ((int)sizeof(TalkUtter), 106);
    CHECK_EQ((int)offsetof(TalkUtter, topic), 2);
    CHECK_EQ((int)offsetof(TalkUtter, shown), 10);
    CHECK_EQ(TALK_PARTS_PS1, 3);                    /* = home_draw.c HOME_HOUSEMATES (F3) */
    CHECK(TALK_PARTS_MAX >= TALK_PARTS_PS1);
}

TEST(talk_contract_feature_bit_and_hooks)
{
    uint8_t buf[8];
    CHECK_EQ_U(PLATF_TALK, 1u << 9);
    CHECK(PLATF_ALL_KNOWN & PLATF_TALK);
    CHECK_EQ_U(PLATF_ALL_KNOWN, 0x3FFu);
    CHECK_EQ_U(plat_features() & ~PLATF_ALL_KNOWN, 0u);
    /* no language "zz": nothing to load, nothing read, buffer untouched */
    CHECK_EQ_U(plat_talk_load("zz"), 0u);
    memset(buf, 0xA5, sizeof(buf));
    /* the weak house hook: Talk is not available until T8 */
    CHECK_EQ(home_talk_enter(0), 0);
    CHECK_EQ(buf[0], 0xA5);
}

/* zoom 2 + focus_* are add-only fields read only by renderers that know
 * them; the reference projection (plat_room_project's weak default) is the
 * anchor room_ref_draw() puts an actor's feet on. */
TEST(talk_contract_room_projection)
{
    DoomoRoomView v, w;
    int16_t x0, y0, x1, y1, x2, y2;
    CHECK((int)offsetof(DoomoRoomView, focus_x) >= (int)offsetof(DoomoRoomView, plat_cache_cap));
    memset(&v, 0, sizeof(v));
    v.w = 6; v.d = 4; v.room_kind = ROOMK_WALLS; v.vx = 0; v.vy = 12; v.vw = 320; v.vh = 172;
    v.punch_q8 = 256; v.daylight = 15;
    /* yaw 0: ox = vx + vw / 2 - (W - D) * 8, oy below; the formula of room_ref */
    room_ref_project(&v, 0, 0, 0, &x0, &y0);
    CHECK_EQ(x0, 160 - (6 - 4) * 8);
    {
        int total = 48 + (6 + 4) * 8;                /* vh 172 < 200: wall_h 48 */
        CHECK_EQ(y0, total + 4 <= 172 ? 12 + (172 - total) / 2 + (total - (6 + 4) * 8)
                                      : 12 + 172 - (6 + 4) * 8 - 4);
    }
    /* one tile east: +16 x, +8 y; one tile south: -16 x, +8 y; z lifts */
    room_ref_project(&v, INT2FIX(1), 0, 0, &x1, &y1);
    CHECK_EQ(x1 - x0, 16); CHECK_EQ(y1 - y0, 8);
    room_ref_project(&v, 0, INT2FIX(1), 0, &x1, &y1);
    CHECK_EQ(x1 - x0, -16); CHECK_EQ(y1 - y0, 8);
    room_ref_project(&v, INT2FIX(2), INT2FIX(3), 20, &x1, &y1);
    room_ref_project(&v, INT2FIX(2), INT2FIX(3), 0, &x2, &y2);
    CHECK_EQ(x1, x2); CHECK_EQ(y2 - y1, 20);
    /* a quarter turn is the same as the turned room seen at yaw 0 */
    w = v;
    v.yaw = 1024;
    w.w = v.d; w.d = v.w;
    room_ref_project(&v, INT2FIX(2), INT2FIX(1), 0, &x1, &y1);
    room_ref_project(&w, INT2FIX(4 - 1), INT2FIX(2), 0, &x2, &y2);
    CHECK_EQ(x1, x2); CHECK_EQ(y1, y2);
    /* pan moves it; the reference renderer ignores zoom and focus */
    v.pan_x = 5; v.pan_y = -3;
    room_ref_project(&v, INT2FIX(2), INT2FIX(1), 0, &x2, &y2);
    CHECK_EQ(x2 - x1, 5); CHECK_EQ(y2 - y1, -3);
    v.zoom = 2; v.focus_x = INT2FIX(3); v.focus_y = INT2FIX(2); v.focus_r = INT2FIX(2);
    room_ref_project(&v, INT2FIX(2), INT2FIX(1), 0, &x1, &y1);
    CHECK_EQ(x1, x2); CHECK_EQ(y1, y2);
    /* the platform hook: the native runner has no projection of its own */
    plat_room_project(&v, INT2FIX(2), INT2FIX(1), 0, &x1, &y1);
    CHECK_EQ(x1, x2); CHECK_EQ(y1, y2);
    room_ref_project(NULL, 0, 0, 0, &x1, &y1);
    CHECK_EQ(x1, 0); CHECK_EQ(y1, 0);
}

static uint32_t lz(uint32_t raw) { return (raw * 129u + 127u) / 128u; }

/* The 'TALK' chunk (docs/TALK.md 6.8): its tag, its declared worst case, and
 * the joint worst case with it on top of K0's (test_save_budget.c). */
TEST(talk_contract_save_chunk)
{
    const DoomoSaveExt *e = doomo_save_ext_talk;
    int i;
    CHECK_EQ_U(SAVE_EXT_TAG_TALK, SAVE_EXT_TAG('T', 'A', 'L', 'K'));
    CHECK_EQ(SAVE_EXT_MAX_TALK, 320);
    CHECK_EQ(TALK_MEM_WORST, 292);
    CHECK(TALK_MEM_WORST <= SAVE_EXT_MAX_TALK);
    CHECK_EQ(save_budget_declared_max(SAVE_EXT_TAG_TALK), SAVE_EXT_MAX_TALK);
    CHECK_EQ(SAVE_BUDGET_TALK_WORST, lz(320) + SAVE_BUDGET_CHUNK_HDR);
    /* no K0 tag is TALK */
    CHECK(SAVE_EXT_TAG_TALK != SAVE_EXT_TAG_LIFE && SAVE_EXT_TAG_TALK != SAVE_EXT_TAG_PROF);
    if (e) {                                         /* T6 landed */
        CHECK_EQ_U(e->tag, SAVE_EXT_TAG_TALK);
        CHECK(e->max_size > 0 && e->max_size <= SAVE_EXT_MAX_TALK);
    } else {
        /* weak: never in the registry, nothing written */
        for (i = 0; i < save_ext_count(); i++) CHECK(save_ext_at(i)->tag != SAVE_EXT_TAG_TALK);
    }
    /* the joint worst case: a post-game story town (test_save_budget.c's
     * 19,342 B), HOME, K0's five and TALK, all at their worst, fit the
     * story slot at 36 KB with PA10's 4 KB to spare, and the free play slot */
    {
        uint32_t home = lz(HOME_CHUNK_WORST) + SAVE_BUDGET_CHUNK_HDR;
        uint32_t story = 19342u + home + SAVE_BUDGET_K0_WORST + SAVE_BUDGET_TALK_WORST;
        uint32_t fp = SAVE_BUDGET_TOWN_BOUND_FP + home + SAVE_BUDGET_K0_WORST + SAVE_BUDGET_TALK_WORST;
        printf("      joint worst with TALK: story %u B, free play %u B\n", (unsigned)story, (unsigned)fp);
        CHECK(story + SAVE_BUDGET_MARGIN <= SAVE_SLOT_STORY_CAP_CFG - 4096u);
        CHECK(fp + SAVE_BUDGET_MARGIN <= SAVE_SLOT_FP_CAP);
    }
}

/* NT_TALK_*: one headline per conversation (T6); they format, name both
 * residents, and fit the feed line. */
TEST(talk_contract_news)
{
    static const int t[] = { NT_TALK_ROW, NT_TALK_MADE_UP, NT_TALK_GREAT_NIGHT, NT_TALK_WALKOUT };
    DoomoNewsRec r;
    char buf[NEWS_LEN];
    int i;
    CHECK(NT_TALK_ROW > NT_WHEEL_SPOOKED);           /* appended: saved records keep their ids */
    CHECK(NT_COUNT <= 255);
    town_new(TOWN_FREEPLAY, 5, TOWN_POP_PREMADE);
    sim_init();
    /* the longest names (11 characters): nothing is cut off */
    memcpy(town_res(3)->name, "Abcdefghijk", 12);
    memcpy(town_res(4)->name, "Lmnopqrstuv", 12);
    for (i = 0; i < DOOMO_COUNTOF(t); i++) {
        memset(&r, 0, sizeof(r));
        r.tmpl = (uint8_t)t[i];
        r.v = NEWS_IDS(3, 4, NEWS_NONE);
        sim_news_format(&r, buf, sizeof buf);
        CHECK_MSG(buf[0] && !strstr(buf, "%s") && strstr(buf, sim_name(3)), "NT %d: '%s'", t[i], buf);
        CHECK_MSG(buf[strlen(buf) - 1] == '.', "NT %d cut off: '%s'", t[i], buf);
        if (t[i] != NT_TALK_WALKOUT) CHECK(strstr(buf, sim_name(4)) != NULL);
    }
}
