/* tips.h - first-time tip cards (CL4 / clarity C4; core/src/tips.c).
 *
 * The first time a player reaches a place that needs explaining (the island
 * hub, the care screen, the house, an exorcism...), a card pops up over it:
 * a line in a character's voice (Mayor Giggs, Rivet or Father Fuzz) and one
 * plain instruction line. The card is non-blocking: the scene under it keeps
 * running and animating; only the player's presses go to the card, and the
 * first one after a short beat closes it (that press is not passed on, so it
 * never counts as a menu action). Toasts wait while a card is up.
 *
 * Seen flags (one bit per TIP_*) survive New Game: they are the machine's,
 * kept in the machine config record where the platform has one (bytes 12..15,
 * options_store.h) and carried by the 'OPT2' save chunk where it has none
 * (native, GBA). Options > Game has "Tips: On / Off" (OPT_TIPS) and "Show
 * all tips again" (tips_reset). Every Field Guide page (lib_help.c) backs the
 * tips with the long explanation.
 *
 * Test harnesses (plat_ext.h PLAT_TEST_NO_TIPS) never see a card unless the
 * script asks for them ("tips on"), so scripted journeys keep their press
 * budgets (core/tests/journeys/budgets.tsv).
 */
#ifndef DOOMO_TIPS_H
#define DOOMO_TIPS_H

#include "types.h"

/* Add-only: a tip's id is its bit in the seen flags (at most 32). */
enum {
    TIP_ISLAND,        /* free play hub: the button map                 */
    TIP_TOWN_HALL,     /* Town Hall: the two sides, creating, rent       */
    TIP_APARTMENTS,    /* the window icons, floors                       */
    TIP_CARE,          /* a citizen's care screen: needs and actions     */
    TIP_HOUSE,         /* the dream house: Live / Build / Photo, score   */
    TIP_DEFEND,        /* Hold the Door: repair / wind / comfort / plate */
    TIP_SHOOTER,       /* a shooter level or arena: moving and shooting  */
    TIP_EXORCISM,      /* the three phases                               */
    TIP_PET,           /* the Boreagotchi LCD                            */
    TIP_STUDIO,        /* the Music Studio tabs                          */
    TIP_WORKSHOP,      /* welding stats                                  */
    TIP_FAMILY,        /* the household                                  */
    TIP_LIBRARY,       /* the Grand Library and its Field Guide          */
    TIP_COUNT
};
DOOMO_STATIC_ASSERT(TIP_COUNT <= 32, tips_fit_the_seen_word);

/* Ask for tip `id` from the calling scene's update. It shows when tips are
 * on, it was never seen, no other card, dialogue or modal is up and the
 * caller is the top scene; otherwise nothing happens (call it again later:
 * it costs a bit test once the tip was seen). 1 = shown now. */
int  tip_once(int id);
/* 1 while a card is on screen (input goes to it, toasts wait). */
int  tip_active(void);
/* The card on screen (TIP_*), -1 for none. */
int  tip_current(void);
int  tip_seen(int id);
/* A short lower-case name ("island", "care", ...) for scripts and logs. */
const char *tip_name(int id);
uint32_t tip_seen_bits(void);
/* Merge seen flags read from a save or the config record (never clears). */
void tip_seen_merge(uint32_t bits);
/* Every tip unseen again (Options "Show all tips again"). */
void tips_reset(void);
/* Tips on / off: OPT_TIPS, but a test harness (PLAT_TEST_NO_TIPS) keeps them
 * off unless tips_test_force(1) (the runner's / harness's "tips on"). */
int  tips_enabled(void);
void tips_test_force(int on);
/* Tests and tools: a card's two lines (the plain one for the current control scheme, with the button
 * names of the pad in hand written into out, at most n bytes; TIP_PLAIN_MAX is always enough), the
 * speaker's name; NULL for a bad id. tips_session_reset() forgets everything (no card, nothing seen, not
 * forced, not dirty) without touching the config record. */
const char *tip_voice_text(int id);
#define TIP_PLAIN_MAX 224
const char *tip_plain_text(int id, char *out, int n);
const char *tip_speaker(int id);
void tips_session_reset(void);
/* 1 when seen flags changed since the config record was last written. */
int  tips_dirty(void);
void tips_clean(void);

/* doomo_frame hooks (game.c): tips_input() right after input_update (a
 * press closes the card and is eaten), tips_draw() after the scene. */
void tips_input(void);
void tips_draw(void);

/* Retained frames (docs/ISLAND_SPEED.md I8): nonzero while the card holds
 * perfectly still; the value names the card (it changes when the card goes
 * or another comes). A scene that kept the frame with the card on it calls
 * tip_hold() so the card is not drawn over the kept page again this frame. */
int  tip_steady(void);
void tip_hold(void);

#endif
