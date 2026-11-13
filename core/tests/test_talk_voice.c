/* test_talk_voice.c - T5B-VOICE: the cast's own voice banks
 * (core/data/talk/grammar/en/voice_*.tlk) over the English banks
 * (TALK_en.BIN). Owner: TK. docs/TALK.md "The starter bank";
 * docs/design/story/SCRIPT_STYLE.md section 7 "Voices".
 *
 *   talk_voice_recognisable   every cast member with a voice bank says a
 *                             line of their own, recognisable without the
 *                             name (a phrase only that voice uses), and no
 *                             other cast member ever says it
 *   talk_voice_kid_and_guard  Pip's and Wisp's banks stay kid-safe in a kid
 *                             room (the S3 lint, no LOVE face) and still
 *                             sound like themselves, and none of
 *                             Grumbleguts' adults-only purr lines can come
 *                             out with a child in the room
 */
#include "test.h"
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/lang.h"
#include "doomo/portrait.h"

/* the roster ids of talkc.py CAST, and a phrase only this voice says */
typedef struct {
    int id;
    const char *who;
    const char *mark;
} VoiceCase;

static const VoiceCase cases[] = {
    {   0, "Mayor Giggs", "Gobble gobble"     },
    {   1, "Nana Puddin", "kneading"          },
    {   5, "Bonejangles", "Clack"             },
    {  13, "Rivet",       "the red thing"     },
    {  42, "Pip",         "Patrol checkpoint" },
    {  71, "Hattie Haha", "Tough crowd"       },
    {  77, "Father Fuzz", "sermon"            },
    { 106, "Mascotty",    "stand still"       },
    { 111, "Dr. Bloom",   "Pulse"             },
    { 563, "Wisp",        "floaty"            },
    { 566, "Charlotte",   "envelope"          },
    { 595, "Anvil",       "the forge"         },
    { 666, "Vex",         "- V"               },
    { 729, "Grumbleguts", "belly"             },
    { 753, "Clinker",     "chalk"             },
    { 778, "Mother Nox",  "Sit. Soup"         },
    { 999, "Lord Vapula", "drizzle"           }
};
#define NVOICE ((int)(sizeof cases / sizeof cases[0]))

/* the player acts a voice bank answers */
static const uint8_t acts[] = { TA_GREET,       TA_FAREWELL,    TA_THANK,      TA_APOLOGIZE, TA_ASK_ABOUT,
                                TA_ASK_OPINION, TA_ASK_FEELING, TA_ASK_ADVICE, TA_TELL,      TA_JOKE,
                                TA_COMPLIMENT,  TA_COMFORT,     TA_OFFER,      TA_INVITE,    TA_HIGH_FIVE,
                                TA_HUG,         TA_WAVE,        TA_GIVE };
#define NACT  ((int)(sizeof acts / sizeof acts[0]))
#define NSEED 24

/* a kid room's tally: the lines a child may not hear (CHECK returns void,
 * so a helper counts and the test reads the count) */
static int kid_bad;
static char kid_bad_line[TALK_LINE_MAX];

/* every act of the bank, NSEED seeds each, the core part alone (c.plain:
 * the voice's own words, without the opener, callback or tag). Sets hit[v]
 * for every voice mark this speaker said; `mark`, when given, is counted in
 * the return value instead. */
static int sweep(int id, int mate, int kid_room, int *hit, const char *mark)
{
    int a, it, v, found = 0;
    for (a = 0; a < NACT; a++) {
        for (it = 0; it < NSEED; it++) {
            TalkConv c;
            TalkLine l;
            int ids[2], n = 0;
            ids[n++] = id;
            if (mate >= 0) ids[n++] = mate;
            talk_gen_begin(&c, DOOMO_PLAYER_ID, ids, n, 700u + (uint32_t)(a * 101 + it));
            c.plain = 1;
            talk_gen_line(&c, 0, -1, acts[a], TC_NONE, &l, NULL);
            if (kid_room && (!c.aud.any_minor || talk_safe_lint_probe(l.text, &c.aud, 1) != TL_OK ||
                             l.face == EXPR_LOVE + 1)) {
                kid_bad++;
                memcpy(kid_bad_line, l.text, sizeof l.text);
            }
            if (mark && strstr(l.text, mark)) found = 1;
            if (hit)
                for (v = 0; v < NVOICE; v++)
                    if (strstr(l.text, cases[v].mark)) hit[v] = 1;
        }
    }
    return found;
}

/* every voice is reachable and sounds like itself, and like nobody else */
TEST(talk_voice_recognisable)
{
    int hit[NVOICE][NVOICE], i, j;
    uint32_t h0;
    memset(hit, 0, sizeof hit);
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
    CHECK(talk_db_open(lang_find("en")) == TALK_DB_OK && talk_gen_ready());
    h0 = talk_safe_lint_hits();
    for (i = 0; i < NVOICE; i++) sweep(cases[i].id, -1, 0, hit[i], NULL);
    for (i = 0; i < NVOICE; i++) {
        CHECK_MSG(hit[i][i], "%s never says \"%s\" in %d lines", cases[i].who, cases[i].mark, NACT * NSEED);
        for (j = 0; j < NVOICE; j++) {
            if (i == j) continue;
            CHECK_MSG(!hit[j][i], "%s says %s's \"%s\"", cases[j].who, cases[i].who, cases[i].mark);
        }
    }
    printf("      %d voices, %d lines each: every mark said by its own speaker only\n", NVOICE, NACT * NSEED);
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}

/* the kids' banks, and Grumbleguts' adults-only guard */
TEST(talk_voice_kid_and_guard)
{
    uint32_t h0;
    kid_bad = 0;
    kid_bad_line[0] = 0;
    test_reset();
    town_new(TOWN_FREEPLAY, 7, TOWN_POP_PREMADE);
    CHECK(talk_db_open(lang_find("en")) == TALK_DB_OK && talk_gen_ready());
    h0 = talk_safe_lint_hits();
    /* Pip and Wisp in each other's room: kid-safe, and still themselves */
    CHECK(sweep(42, 563, 1, NULL, "Patrol"));
    CHECK(sweep(563, 42, 1, NULL, "sorry"));
    /* Grumbleguts with a child in the room: the grumpy uncle, never one of
     * the five adults_only&!kid_present purr lines */
    CHECK(sweep(729, 42, 1, NULL, "wash your hands"));
    CHECK_MSG(!sweep(729, 42, 1, NULL, "oven dragged in"), "a purr line with a child in the room");
    CHECK_MSG(!sweep(729, 42, 1, NULL, "good side of the belly"), "a purr line with a child in the room");
    CHECK_MSG(!sweep(729, 563, 1, NULL, "bark and a little bit of gravy"), "a purr line with a child in the room");
    CHECK_MSG(!sweep(729, 563, 1, NULL, "belly's blushing"), "a purr line with a child in the room");
    CHECK_MSG(!sweep(729, 42, 1, NULL, "this fabulous"), "a purr line with a child in the room");
    CHECK_MSG(kid_bad == 0, "%d kid-room lines a child may not hear, e.g. %s", kid_bad, kid_bad_line);
    /* with an adult in the room the purr is his again */
    CHECK(sweep(729, 1, 0, NULL, "darlin'"));
    CHECK_EQ(talk_safe_lint_hits(), h0);
    talk_db_close();
}
