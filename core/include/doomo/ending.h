/* ending.h - the endings contract: ids, categories, the saved 'ENDS'
 * state, romance routes and the adult gate (owner: E0 contract,
 * LIFESIM_PLAN E0 + section 6 amendments; add-only after E0).
 *
 * Implementation: core/src/ending.c (E1: the rule table, the strong 'ENDS'
 * descriptor, the routes and the strong mature_adult_ok). The weak defaults in
 * core/src/weak_endings.c stay as the safe "nothing seen, nobody
 * qualifies" answer for a build that leaves ending.c out: no ending is
 * recorded and mature_adult_ok() refuses everyone, so every mature slot
 * (E2) plays its fade-to-black fallback (LIFESIM_PLAN E2 note).
 *
 * THE ENDINGS (user answers 18, 19, 21; DECISIONS "ENDINGS"):
 * 14 endings in four categories, plus a 15th, very secret joke ending
 * "Nope." that is a bonus and NOT one of the 14 (the counters say n/14;
 * the bonus shows only once found). Index k is also the unlock id
 * UL_STORY_END(k) (unlock_ids.h, 711..725). Ids are append-only: a saved
 * 'ENDS' chunk and the unlock bits store them. The titles are the content
 * packages' (E3 good + secret, E4 bad + sexy); the comments are working
 * titles only.
 */
#ifndef DOOMO_ENDING_H
#define DOOMO_ENDING_H

#include "types.h"

enum {
    /* good (E3) */
    ENDING_E01,      /* the finale, the default good ending (Ep 4.7)          */
    ENDING_E02,      /* the finale, the better good ending (Ep 4.7)           */
    ENDING_E03,      /* the Feast's good outcome (side_b)                     */
    ENDING_E04,      /* "Family Portrait": partner + HG_ETERNAL house + family */
    /* bad (E4) */
    ENDING_E05,      /* the Feast's permanent bad outcome (side_b)            */
    ENDING_E06,      /* the finale with an empty parade (Ep 4.7)              */
    ENDING_E07,      /* the finale with half a parade (Ep 4.7)                */
    ENDING_E08,      /* the fourth bad ending (E4 names it; "Nope." moved to
                        the bonus slot, user answer 19)                        */
    /* secret (E3) */
    ENDING_E09,      /* Bonejangles' (show canon; trigger provisional, E12)    */
    ENDING_E10,      /* every sparkle + D2                                     */
    ENDING_E11,      /* New Game+ with the bottle and the suitcase             */
    /* sexy (E4; adults only, hidden while Mature is Off, answer 21) */
    ENDING_E12,      /* the Grumbleguts route                                  */
    ENDING_E13,      /* the Vex route                                          */
    ENDING_E14,      /* any premade adult (answer 24): the ROUTE_ANY partner   */
    /* the bonus (E4): the very secret 15th */
    ENDING_NOPE,     /* "Nope." (3 s card, credits stub, rewind; answer 19)    */
    ENDING_COUNT
};
#define ENDING_MAIN_COUNT 14          /* the "n/14" of the card and the list   */
/* Retired slots: E03, E05 and E07 have no design, so they never trigger, never count and are
 * never listed. Their numbers stay reserved (save bits, unlock ids 711+k) so nothing shifts. */
#define ENDING_ACTIVE_COUNT 11        /* the "n/11" of the card and the list   */
#define ENDING_IS_RETIRED(k) ((k) == ENDING_E03 || (k) == ENDING_E05 || (k) == ENDING_E07)
#define ENDING_NONE       0xFF        /* DoomoEnds.last / .pending: none        */
DOOMO_STATIC_ASSERT(ENDING_COUNT == 15, endings_are_14_plus_the_bonus);
DOOMO_STATIC_ASSERT(ENDING_NOPE == ENDING_MAIN_COUNT, the_bonus_follows_the_14);

/* Categories (the card's colour, the Endings list's "??? (category)"). */
enum { ENDCAT_GOOD, ENDCAT_BAD, ENDCAT_SECRET, ENDCAT_SEXY, ENDCAT_BONUS, ENDCAT_COUNT };

/* ---- romance routes (E1 engine, E4 content; R12) ------------------------
 * A route's meter (0..255, story affection) and stage live in 'ENDS'.
 * When a route reaches COMMITTED or MARRIED the route script calls
 * CB_LIFE_FAMILY_ADD (LS5), so the family table (F1) and the endings
 * agree; ending_partner() prefers F1's life_player_partner(). Every route
 * participant passes mature_adult_ok() (R10). */
enum { ROUTE_GRUMBLEGUTS, ROUTE_VEX,
       ROUTE_ANY,            /* any premade adult (answer 24): ENDS.route_partner */
       ROUTE_COUNT };
enum { RSTAGE_NONE, RSTAGE_OFFERED, RSTAGE_DATING, RSTAGE_KISSED, RSTAGE_STAYED,
       RSTAGE_COMMITTED, RSTAGE_MARRIED, RSTAGE_REFUSED, RSTAGE_COUNT };

/* ---- the saved state: the 'ENDS' chunk (E1's strong descriptor) ----------
 * 32 B = save_budget.h SAVE_EXT_MAX_ENDS. `seen` and `times` survive New
 * Game+ (E1 copies them over); routes reset with a new story. */
#define ENDS_VERSION 1
enum { ENDF_FINALE_DONE = 1,      /* Ep 4.7's ending played in this save      */
       ENDF_NGPLUS_KEPT = 2,      /* seen bits carried over from a cleared save */
       ENDF_POSTGAME = 4 };       /* post-game endings may fire               */
typedef struct DoomoEnds {
    uint8_t  version;               /* ENDS_VERSION                            */
    uint8_t  last;                  /* the last ending reached, ENDING_NONE    */
    uint16_t seen;                  /* bit k: ending k reached                 */
    uint8_t  times[ENDING_COUNT];   /* times reached, saturating at 255        */
    uint8_t  flags;                 /* ENDF_*                                  */
    uint8_t  route_meter[ROUTE_COUNT];
    uint8_t  route_stage[ROUTE_COUNT];   /* RSTAGE_*                           */
    uint16_t route_partner;         /* ROUTE_ANY's resident id, 0xFFFF none    */
    uint8_t  pending;               /* an ending queued for its card, or NONE  */
    uint8_t  reserved[3];
} DoomoEnds;
DOOMO_STATIC_ASSERT(sizeof(DoomoEnds) == 32, doomo_ends_is_32_bytes);

/* ---- the API (E1; weak defaults in weak_endings.c) ----------------------- */
/* The live state (E1: the chunk's RAM copy). Weak: NULL. */
DoomoEnds *ending_state(void);
/* 1 if ending k was reached in this save (or carried over by NG+). */
int  ending_seen(int k);
/* Record ending k as reached (seen bit, times, last, unlock UL_STORY_END(k)). */
void ending_mark(int k);
/* How many of the 14 main endings were reached (the bonus never counts). */
int  ending_count_seen(void);
/* The ending Ep 4.7's finale plays (E01/E02/E06/E07, or a secret one that
 * outranks it: secret > bad > good). Weak: ENDING_E01. */
int  ending_resolve_finale(void);
/* The player's partner for the endings: F1's life_player_partner() first,
 * then the furthest route's partner; -1 none. */
int  ending_partner(void);
/* ENDCAT_* of ending k (-1 out of range). Pure table; weak_endings.c holds
 * it and E1 may keep it. */
int  ending_category(int k);
/* 1 if ending k may show in the Endings list and the card counters: the
 * sexy ones only while Mature is not Off (answer 21), the bonus only once
 * seen. Pure rule over ending_category, ending_seen and opt_mature. */
int  ending_visible(int k);

/* ---- E1 additions (strong only: ending.c; no weak default) ----------------
 * Reach ending k: ending_mark(k) and queue its card. Returns 1 if k was new.
 * The card (SCENE_ENDING_CARD, ending_scene.c) is pushed by the story overworld
 * the next time it is idle (overworld.c auto_events), so a trigger never has to
 * be in a state where a scene may be pushed. */
int  ending_reach(int k);
/* The queued ending (ENDING_NONE if none); _take also clears it. */
int  ending_pending(void);
int  ending_take_pending(void);
/* The credits of the story rolled: reach ending_resolve_finale(). Sets
 * ENDF_FINALE_DONE. Called from story_ch34.c where the story completes. */
void ending_finale_reached(void);
/* Reach E04 if a partner, a household and an ETERNAL house room exist. */
void ending_check_family(void);
/* E09 "Bonejangles Bows": once the Feast is over (SF_FEAST_DONE), whatever its outcome, and the
 * machine-heart piece is found (SF_BONE_HEART), reach E09. The Feast scenes call this when they
 * set SF_FEAST_DONE; the finale calls it too. Safe to call any time. */
void ending_check_bonejangles(void);
/* E10 "Every Sparkle": every secret sparkle found and the dream D2 played. The overworld calls this
 * when a sparkle is found; the finale too. The finale itself resolves to E10 / E11 when they
 * hold (secret outranks the rest); E11 needs SF_NGPLUS, SF_BOTTLE_SEEN and SF_SUITCASE_OPEN. */
void ending_check_sparkles(void);

/* ---- the adult gate (R10; E1 strong, weak refuses everyone) -------------
 * 1 only if resident `id` may appear in a mature or romance scene: life
 * stage ADULT (LS0) AND the premade allow-list (a premade id whose name
 * still matches, not on core/data/story/kid_cast.txt, not RF_CUSTOM, not
 * sim-born). Every mature slot, sexy ending, override-art lookup and route
 * participant calls it. */
int  mature_adult_ok(int id);
/* 1 if premade citizen `id` is on the kid-coded cast (the compiled copy of
 * core/data/story/kid_cast.txt; E1 strong, no weak default: the gate above is the caller). */
int  ending_kid_cast(int id);

/* ---- the Mature option (U0 owns the strong definition, town.h) ----------
 * OPT_MATURE's value: MATURE_OFF / CENSORED / ON / PACK. The VM's `jmat`
 * reads it. Until U0 lands, the weak default in weak_endings.c returns
 * MATURE_OFF (every guarded moment takes its cutaway). */
enum { MATURE_OFF, MATURE_CENSORED, MATURE_ON, MATURE_PACK, MATURE_LEVELS };
int  opt_mature(void);


/* ---- E4 additions (strong only: ending.c) ---------------------------------
 * Route state. ending_route_add: meter += amount (40 offers, 120 starts dating); returns the
 * meter. ending_route_set: move a route forward to RSTAGE_* (or REFUSED); needs mature_adult_ok
 * and PA9's two-way gate for the partner (ROUTE_ANY takes `partner`, the others ignore it);
 * returns 1 if it changed. ending_route_partner: the resident id a route is with, -1 none.
 * COMMITTED / MARRIED plus Mature not Off makes the finale E12 / E13 / E14. */
int  ending_route_stage(int route);
int  ending_route_meter(int route);
int  ending_route_partner(int route);
int  ending_route_add(int route, int amount);
int  ending_route_set(int route, int stage, int partner);

#endif
