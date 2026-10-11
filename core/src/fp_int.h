/* fp_int.h - private glue between the free play screens (owner: freeplay).
 *
 *   fp_common.c   shared helpers: the living island (sim_tick + autosave),
 *                 HUD, chibis on a render budget, portraits, meters, bubbles,
 *                 particle bursts, status / mood helpers, the island's own
 *                 save chunk ('FPLY': island name, rent day, grave flowers),
 *                 the citizen picker API and the strong activity_launch()
 *   fp_title.c    SCENE_TITLE (+ the plush parade)
 *   fp_newisland.c SCENE_NEW_ISLAND
 *   fp_island.c   SCENE_ISLAND: the hub map, cursor, location menus
 *   fp_island_gfx.c the illustrated island (terrain, buildings, walkers)
 *   fp_apartments.c SCENE_APARTMENTS
 *   fp_roster.c   SCENE_ROSTER + SCENE_PICKER (the same browser, two states)
 *   fp_resident.c SCENE_RESIDENT (care, profile, friends, activities)
 *   fp_shop.c     SCENE_SHOP (shop, cafe counter, Hellvile black market)
 *   fp_graveyard.c SCENE_GRAVEYARD
 *   fp_news.c     SCENE_NEWS
 *   fp_options.c  SCENE_OPTIONS
 *   fp_saveload.c SCENE_SAVELOAD
 *   fp_unlocks.c  SCENE_UNLOCKS (gallery of every unlock + progress)
 *   unlocks_freeplay.c  the free play unlockables (ids 736..831)
 */
#ifndef DOOMO_FP_INT_H
#define DOOMO_FP_INT_H

#include "doomo/doomo.h"
#include "doomo/save_ext.h"

/* ---- scene args --------------------------------------------------------- */
enum { FP_ISLAND_ARG_RETURN = 0, FP_ISLAND_ARG_WELCOME = 1 };
enum { FP_ROSTER_ARG_BROWSE = 0 };
enum { FP_SHOP_ARG_SHOP = 0, FP_SHOP_ARG_BLACK_MARKET = 1, FP_SHOP_ARG_CAFE = 2 };
enum { FP_SL_ARG_MANAGE = 0, FP_SL_ARG_LOAD = 1 };

/* ---- palette shorthands ------------------------------------------------- */
#define RP(r, s) PAL_RAMP(r, s)
#define PP(p, s) PAL_PAINT(p, s)

/* ---- the living island -------------------------------------------------- */
/* Call once per tick from every free play scene's update: runs sim_tick()
 * in free play (the island lives while you browse), the autosave option,
 * and the particle system. */
void fp_live(void);
/* 1 if this is a free play island (not a story town). */
int  fp_is_freeplay(void);
/* Background music for the island hub at this hour. */
int  fp_island_song(void);

/* ---- island save chunk ('FPLY') ------------------------------------------ */
#define FP_ISLAND_NAME_LEN 16
const char *fp_island_name(void);             /* "" if unnamed           */
void fp_set_island_name(const char *s);
const char *fp_island_title(char *buf, int cap); /* "Mochi Isle" / "Hellvile Park" */
uint32_t fp_rent_day(void);                   /* day + 1 of the last rent  */
void fp_set_rent_day(uint32_t d);
int  fp_flowers(int id);                       /* flowers on a grave 0..3  */
void fp_set_flowers(int id, int n);

/* ---- small helpers ------------------------------------------------------- */
int  fp_t(void);                               /* doomo_frame_count()      */
int  fp_big(void);                             /* 320x240 class screen     */
int  fp_sw(void);
int  fp_sh(void);
int  fp_lower(int c);
int  fp_strcasestr(const char *hay, const char *needle);
void fp_strcpy(char *dst, const char *src, int cap);
/* Resident record or NULL (any id incl. empty slots). */
DoomoResident *fp_res(int id);
int  fp_alive(int id);                         /* ALIVE / POSSESSED / GHOST / SOUL_LOST */
int  fp_living_status(int status);
const char *fp_status_name(int status);
uint8_t fp_status_color(int status);
uint8_t fp_status_icon(int status);
int  fp_mood_expr(const DoomoResident *r);
uint8_t fp_mood_icon(int mood);
uint8_t fp_mood_color(int mood);
uint8_t fp_rel_icon(int kind);
uint8_t fp_rel_color(int kind);
const char *fp_job(int id);                    /* roster job / "Freelancer" */
const char *fp_bio(int id);
int  fp_floor(int id);                         /* 1..50 */
int  fp_room(int id);                          /* 1..10 */
const char *fp_district_name(int d);           /* "Hillvile" / "Hellvile"  */
const char *fp_home_name(int d);               /* "Hillvile Towers" / "Hellvile Pits" */
/* A premade roster citizen's look for hosts (works on an empty island). */
const DoomoLook *fp_host_look(int roster_id);
int  fp_host_in_town(int roster_id);           /* living in this town      */
/* Map a sim activity (ACT_*) to a short place name. */
const char *fp_activity_name(int act);
uint8_t fp_activity_icon(int act);
int  fp_item_icon(int item);

/* ---- drawing -------------------------------------------------------------- */
/* Top HUD: day, weekday, clock (sun / moon), money. Returns its height. */
int  fp_hud(int style);
/* Money pill at x (right-aligned at right edge if right != 0). */
void fp_money_pill(int x, int y, int right_align);
/* Small icon glyph drawn in a colour with a soft shadow. */
void fp_icon(int x, int y, int icon, uint8_t color);
/* Thought bubble with an icon (problem markers); phase animates it. */
void fp_bubble(int x, int y, int icon, uint8_t color, int phase);
/* Label + meter row: icon, name, bar. */
void fp_meter(int x, int y, int w, int icon, const char *label, int value, uint8_t fill,
              uint8_t text);
uint8_t fp_stat_color(int stat, int value);
uint8_t fp_stat_icon(int stat);
extern const char *const fp_stat_names[STAT_COUNT];
/* Chibi (bodyspr) with a per-tick render budget: draws a soft placeholder
 * until there is budget to render it. flags: IMG_FLIPX etc. Returns 1 if
 * the real chibi was drawn. */
int  fp_chibi(int id, int x, int y, int dir, int frame, int flags);
/* Bust portrait in a frame (w = 48*scale + 6). Expression from mood when
 * expr < 0. Throttles new renders like fp_chibi (shows a placeholder). */
void fp_portrait(int x, int y, int id, int scale, int expr, int style);
/* Full-body figure of a look (hosts, title parade). */
void fp_figure(int x, int y, const DoomoLook *l, int status, int expr);
/* Soft shaded ball (placeholders, decorations). */
void fp_ball(int cx, int cy, int r, uint8_t base);
/* Rounded pill (buttons, tags, plates) and a soft drop shadow under a
 * rect that is drawn right afterwards. */
void fp_pill(int x, int y, int w, int h, uint8_t fill, uint8_t outline, uint8_t hi);
void fp_shadow(int x, int y, int w, int h);
/* Rounded panel with a header tab. */
void fp_panel(int x, int y, int w, int h, int style, const char *title);
/* Pill with text, colours given. */
void fp_tag(int x, int y, const char *s, uint8_t fill, uint8_t text);
/* Marquee text clipped to w pixels (scrolls when too long). */
void fp_marquee(int x, int y, int w, const char *s, uint8_t color, int t);
/* News ticker strip at y (h = 12). */
void fp_ticker(int y, int style);
/* Wavy "loading" dots. */
void fp_dots(int x, int y, uint8_t c);

/* ---- the graveyard rite -------------------------------------------------- */
/* Why the ghost rite can't happen now ("" = it can): night, Hymn Book +
 * Sage Bundle. fp_ghost_rite() performs it (dialogue + achievement). */
const char *fp_ghost_rite_blocker(int id);
int  fp_ghost_rite(int id);

/* ---- particles ------------------------------------------------------------ */
enum { FX_HEART, FX_STAR, FX_SPARK, FX_NOTE, FX_STINK, FX_TEAR, FX_CONFETTI,
       FX_PETAL, FX_SKULL, FX_BUBBLE, FX_COIN };
void fp_fx_burst(int x, int y, int kind, int n);
void fp_fx_clear(void);
void fp_fx_tick(void);          /* step every particle (fp_live does it) */
void fp_fx_draw(void);           /* draw (and step) every live particle */

/* ---- citizen picker ---------------------------------------------------- */
enum { PICK_ANY,          /* any non-empty slot                          */
       PICK_LIVING,       /* alive, possessed, soul lost, ghost          */
       PICK_AWAKE,        /* living and not asleep                       */
       PICK_POSSESSED,    /* possessed or soul lost (church)             */
       PICK_SICK,         /* health < 60 / sick (hospital)               */
       PICK_DEAD,         /* in the graveyard                            */
       PICK_HERE,         /* activity == pick_act                        */
       PICK_SLEEPING,     /* asleep (dreams)                             */
       PICK_KIND_COUNT };
/* Opens SCENE_PICKER; done(id) runs after it closed (id -1 = cancelled).
 * exclude: an id that cannot be picked (-1 none). */
void fp_pick(const char *title, int kind, int exclude, void (*done)(int id));
void fp_pick_act(const char *title, int act, void (*done)(int id));
int  fp_pick_match(int kind, int id);
/* Launch an activity for a known resident. */
void fp_launch_with(const DoomoActivity *a, int id);
/* Is a's location one of ours / can it be shown in free play at all. */
int  fp_activity_visible(const DoomoActivity *a);
/* Why an activity isn't available right now ("" if it is). */
const char *fp_activity_reason(const DoomoActivity *a);

/* ---- pending results ------------------------------------------------------ */
/* Scenes that push SCENE_CREATOR / SCENE_EXORCISM etc. mark what they are
 * waiting for and check it when they are on top again. */
enum { FP_WAIT_NONE, FP_WAIT_CREATOR, FP_WAIT_EXORCISM, FP_WAIT_OTHER };

/* ---- sorting --------------------------------------------------------------- */
void fp_sort_u16(uint16_t *a, int n, int (*cmp)(int a, int b));

/* ---- shared scene entry helpers ---------------------------------------- */
/* Autosave into slot 1 only once this island owns it (saved / loaded). */
void fp_autosave_bind(int on);
/* Start a fresh free play island (town_new + sim_init + chunk reset). */
void fp_new_island(int population, uint32_t seed);
/* Go to the hub (scene_set). */
void fp_goto_island(int arg);
/* Open the resident screen. */
void fp_open_resident(int id);

/* ---- the resident screen's actions (fp_resident.c) ----------------------- */
enum { FPA_HELP, FPA_FEED, FPA_GIFT, FPA_CLEAN, FPA_PLAY, FPA_TALK, FPA_HEAL, FPA_COMFORT,
       FPA_INTRODUCE, FPA_ADVICE, FPA_EDIT, FPA_FAV, FPA_EXORCISE, FPA_FLOWERS, FPA_GHOST,
       FPA_GRAVE, FPA_HOME, FPA_REST, FPA_CREATE, FPA_COUNT };
/* test hooks: the actions offered for the open resident */
int  fp_resident_action_count(void);
int  fp_resident_has_action(int a);
int  fp_resident_select(int a);           /* move the cursor to it (Care tab) */
int  fp_resident_id(void);
int  fp_resident_in_farewell(void);

/* ---- free play unlock ids (unlocks_freeplay.c, range 736..831) ---------- */
enum {
    UL_FP_FIRST_ACH = 740,      /* code-granted achievements (UL_CAT_MISC) */
    UL_FP_LANDLORD = UL_FP_FIRST_ACH, UL_FP_NAMER, UL_FP_FIRST_CITIZEN, UL_FP_PICNIC,
    UL_FP_SHOPAHOLIC, UL_FP_BLACK_MARKET, UL_FP_GARDENER, UL_FP_NIGHT_OWL, UL_FP_BOOKWORM,
    UL_FP_FAV_FOOD, UL_FP_HEALER, UL_FP_MATCHMAKER, UL_FP_FULL_HOUSE_HILL, UL_FP_FULL_HOUSE_HELL,
    UL_FP_GHOST_RITE, UL_FP_HELPER, UL_FP_HELPER_BIG, UL_FP_COLLECTOR, UL_FP_EMPTY_START,
    UL_FP_PREMADE_START, UL_FP_RANDOM_START, UL_FP_FAVOURITES, UL_FP_SEARCHER,
    UL_FP_HATED_FOOD, UL_FP_HOUSEWARMING,
    UL_FP_LAST_ACH
};
DOOMO_STATIC_ASSERT(UL_FP_LAST_ACH <= 770, fp_achievements_fit);
/* Grant an achievement (toast) and the creator part that comes with it. */
void fp_achieve(int ul);
/* Island-state achievements (full districts, favourites, hoards...). */
void fp_check_island_achievements(void);

/* Extra scenes (also declared in scenes.h). */
extern const DoomoScene SCENE_UNLOCKS, SCENE_PICKER;

/* Shared static scratch for building one-off strings. */
extern char fp_tmp[128];
/* Id lists of the roster / picker and the unlocks gallery live in the
 * shared scene scratch (scene.h): each re-claims and rebuilds its list
 * when it comes back on top. */
#define FP_IDS_MAX 1024

#endif
