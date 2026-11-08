/* talk_census.h - Talk T7: the census and the quality checks (owner: TK;
 * docs/TALK.md "The census"; design 3.7). Host only: core/tools/talk_census.c
 * is the `talk_census` program (make -C core talk-census) and, compiled
 * with TALK_CENSUS_NO_MAIN, the smoke census of the unit tests
 * (core/tests/test_talk_census.c). It links the core and reads
 * core/build/talk/TALK_en.BIN through the native platform.
 */
#ifndef TALK_CENSUS_H
#define TALK_CENSUS_H

#include <stdint.h>
#include <stdio.h>
#include "doomo/talk.h"

/* why an exchange did not count (design 3.7, Q1-Q6; Q7 is the human read) */
enum {
    QF_NONE = 0,
    QF_Q1_FALLBACK,     /* nothing fitted: a safe line                    */
    QF_Q1_CODE,         /* an unresolved {, a stray }, an unknown code     */
    QF_Q1_SPACE,        /* two spaces, a space before punctuation          */
    QF_Q1_CASE,         /* no capital letter at the start                  */
    QF_Q1_END,          /* no end punctuation                              */
    QF_Q1_ARTICLE,      /* "a apple", "an pie"                             */
    QF_Q1_BUBBLE,       /* more than the bubble's lines                    */
    QF_Q1_SENTENCES,    /* more than 4 sentences                           */
    QF_Q2_ACT,          /* a reply act the adjacency table does not allow  */
    QF_Q2_TOPIC,        /* a topic that is not the player's, a 1-hop one or (CHANGE_TOPIC) a like */
    QF_Q3_VOICE,        /* a voice guard that does not hold for the speaker */
    QF_Q3_EXCL,         /* more exclamation marks than the speaker's budget */
    QF_Q3_TIC,          /* another persona type's tic                       */
    QF_Q4_FACT,         /* a callback that is not true                      */
    QF_Q5_REPEAT,       /* a line twice, or a core template twice in 6 turns (conversations) */
    QF_Q6_LINT,         /* the S3 lint replaced or refuses a line           */
    QF_Q6_KIDROOM,      /* a register word, LOVE face or romance rule in a kid room */
    QF_Q6_MATURE,       /* a Mature variant without the option or the adults */
    QF_COUNT
};
extern const char *const talk_census_qf_names[QF_COUNT];

typedef struct CensusCfg {
    const char *grid;            /* census_<lang>.tsv                                   */
    uint32_t contexts;           /* exchanges to sample (0: the grid's @contexts)        */
    uint32_t conversations;      /* bot conversations of 6 turns (0: none)               */
    uint32_t seed;               /* 0: the grid's @seed                                 */
    FILE    *sample;             /* NULL, or the review sheet's source rows (talk_sample.py) */
    uint32_t sample_every;       /* write every Nth exchange to `sample`                 */
    FILE    *fails;              /* NULL, or every exchange that failed a check, with why  */
    int      quiet;
} CensusCfg;

typedef struct CensusTier { uint64_t distinct, f1, f2; double chao1; } CensusTier;

typedef struct CensusResult {
    uint64_t contexts, exchanges_ok, lines, kid_rooms;
    CensusTier surface, skeleton, meaning;
    uint64_t fail[QF_COUNT];       /* failures by reason (an exchange counts once, for its first) */
    uint64_t lint_hits;            /* talk_safe_lint_hits() during the run                  */
    uint64_t conversations, conv_distinct, conv_q5;
    double   secs, conv_secs;
    uint32_t nres, ntopics, nacts;
} CensusResult;

/* Load the grid and run. Returns 0, or -1 when the grid or the database is
 * missing (err says which). */
int  talk_census_run(const CensusCfg *cfg, CensusResult *res, char *err, int errcap);
/* The report (design 3.7: distinct per tier, uniqueness, Chao1, the failure
 * histogram, the conversation tier). */
void talk_census_print(const CensusResult *r, FILE *f);
/* The design's targets for a full run: 1 if every one holds. */
int  talk_census_targets_met(const CensusResult *r);

/* The quality checks of one exchange (Q1-Q4, Q6), for the census and the
 * unit tests: the first failure (QF_*) or QF_NONE. u is the utterance
 * after the gate. */
int  talk_census_check(const TalkConv *c, const TalkUtter *u, const TalkExchange *x, const TalkTrace *tr);

#endif
