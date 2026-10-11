/* mg.h - private glue of the FUN FAIR minigames (mg_*.c, owner: minigames).
 * Not part of the contract.
 *
 * Every arcade game is a DoomoScene driven by the shared flow in mg.c:
 *
 *   TITLE card (marquee sign, host booth photo + tutorial line, 3 tiers,
 *   best score and medals)  ->  READY? GO!  ->  PLAY (pause on START)  ->
 *   TIME UP / OUCH banner  ->  RESULTS (score count-up, NEW BEST, medal,
 *   H$ reward, unlock toasts; Retry / Leave).
 *
 * A game only supplies an MgGame: start / update / draw hooks plus an
 * autoplay bot (tests, screenshots). Its state lives in the shared arena
 * (one game runs at a time). Juice helpers: particles, score popups,
 * screen shake, flashes and hit-stop; scoring helpers apply the combo and
 * tier multipliers. The Hellfire tier turns the fair into its hellish
 * night version (red sky, embers, brutal splats with GORE_BRUTAL).
 *
 * Scene arg: bits 0-1 preselect a tier (1 Sweet, 2 Spicy, 3 Hellfire),
 * MG_ARG_AUTOSTART (16) skips the title card, MG_ARG_AUTOPLAY (32) lets
 * the bot play, MG_ARG_DEMO (64) moves the premade citizens onto an empty
 * island first (runner scripts: "push MG_CATCH 50" = Spicy, bot, no
 * title; 113 = Sweet, bot, no title, premade cast).
 *
 * Budgets: .bss of the whole module < 3 KB (state + 2 KB arena + fx),
 * no .data, no malloc, no floats.
 */
#ifndef DOOMO_MG_H
#define DOOMO_MG_H

#include "doomo/doomo.h"

enum { MG_CATCH, MG_WHACK, MG_DODGE, MG_GALLERY, MG_DERBY, MG_MEMORY,
       MG_GAME_COUNT };
enum { MG_TIER_SWEET, MG_TIER_SPICY, MG_TIER_HELLFIRE, MG_TIERS };
enum { MG_MEDAL_NONE, MG_MEDAL_BRONZE, MG_MEDAL_SILVER, MG_MEDAL_GOLD };

#define MG_ARG_TIER_MASK 0x03
#define MG_ARG_AUTOSTART 0x10
#define MG_ARG_AUTOPLAY  0x20
#define MG_ARG_DEMO      0x40   /* debug: an empty island gets the premade cast */

/* Unlock ids (bit indices; the minigames own 832..863, the unused part of
 * art's range - the concert hall took 864..895). */
#define MG_UL_BASE        832
#define MG_UL_MEDAL(g, m) (MG_UL_BASE + ((m) - 1) * MG_GAME_COUNT + (g)) /* 832..849 */
#define MG_UL_REGULAR     850   /* play every arcade game              */
#define MG_UL_GOLD_ALL    851   /* gold medal in every game            */
#define MG_UL_JACKPOT     852   /* Wheel of Fate jackpot               */
#define MG_UL_UNTOUCHABLE 853   /* survive Vape Cloud Dodge unhit      */
#define MG_UL_FUZZ_FAN    854   /* Whack-an-Imp: never bonk Father Fuzz */
#define MG_UL_RECALL      855   /* Memory Match with no misses         */
#define MG_UL_PHOTO       856   /* win the Derby by a nose             */
#define MG_UL_SUGAR       857   /* three fevers in one Cupcake Catch   */
#define MG_UL_PACIFIST    858   /* gallery run without hitting a citizen */
#define MG_UL_CURSED      859   /* get cursed by the Wheel of Fate     */
#define MG_UL_HELLFIRE    860   /* a gold medal on the Hellfire tier   */
#define MG_UL_FIRST       832
#define MG_UL_LAST        860

/* Medal scores (bronze, silver, gold) - shared by the games and the
 * automatic unlock rules in unlocks_minigames.c. */
#define MG_CATCH_B    800
#define MG_CATCH_S   2000
#define MG_CATCH_G   4000
#define MG_WHACK_B   1000
#define MG_WHACK_S   3000
#define MG_WHACK_G   6000
#define MG_DODGE_B    600
#define MG_DODGE_S   1500
#define MG_DODGE_G   3000
#define MG_GALLERY_B 1500
#define MG_GALLERY_S 4000
#define MG_GALLERY_G 8000
#define MG_DERBY_B   1000
#define MG_DERBY_S   2000
#define MG_DERBY_G   3000
#define MG_MEMORY_B   700
#define MG_MEMORY_S  1600
#define MG_MEMORY_G  3000

typedef struct MgGame {
    const char *title;          /* "CUPCAKE CATCH" (big title text)      */
    const char *sub;            /* small line under the title            */
    const char *how;            /* tutorial line said by the host        */
    const char *keys;           /* controls line                         */
    uint8_t  id;                /* MG_*                                  */
    uint8_t  prog;              /* PROG_HS_*                             */
    uint8_t  song;              /* SONG_* while playing                  */
    uint8_t  lives;             /* hearts (0 = none)                     */
    uint16_t host;              /* roster id of the host                 */
    uint16_t medal[3];          /* bronze / silver / gold scores         */
    uint16_t pay_div;           /* H$ = score / pay_div (+ medal bonus)  */
    uint16_t time;              /* round length in ticks (0 = none)      */
    uint8_t  pal[3];            /* title card colours: stripe a, b, trim */
    void (*start)(void);        /* reset state for mg.tier               */
    void (*update)(void);       /* one play tick                         */
    void (*draw)(void);         /* the playfield, offset by mg.ox/oy     */
    void (*art)(int cx, int cy, int t);   /* title card mascot          */
    uint32_t (*bot)(void);      /* autoplay: buttons held this tick      */
} MgGame;

enum { MGP_TITLE, MGP_READY, MGP_PLAY, MGP_PAUSE, MGP_END, MGP_RESULTS };

typedef struct MgState {
    const MgGame *g;
    uint8_t  phase, tier, sel, autoplay, autostart, result_sel, pause_sel;
    uint8_t  lives, new_best, medal, prev_medal, flash_c, flash_t;
    uint8_t  freeze, shake, quiet_hud, hell;
    int8_t   ox, oy;            /* screen shake offset this frame         */
    uint16_t t;                 /* ticks in the current phase             */
    uint16_t play_t;            /* ticks played this round                */
    uint16_t time_left;
    uint16_t combo, best_combo;
    uint8_t  score_bump, combo_bump;
    int32_t  score, shown;
    int32_t  reward, reward_shown;
    const char *end_text;
    uint32_t bot_now, bot_prev;
    uint8_t  stat_a, stat_b;    /* per-game flags for trophies            */
} MgState;
extern MgState mg;

/* The shared state arena (one game at a time). */
#define MG_ARENA_BYTES 2048
extern uint32_t mg_arena[MG_ARENA_BYTES / 4];
/* The arena as an opaque pointer (keeps the type punning out of sight of
 * the optimiser; `const`: the pointer never changes). */
void *mg_state(void) __attribute__((const));
#define MG_ARENA(type) ((type *)mg_state())
#define MG_ARENA_FITS(type) \
    DOOMO_STATIC_ASSERT(sizeof(type) <= MG_ARENA_BYTES, type##_fits_arena)

/* ---- scene plumbing --------------------------------------------------- */
void mg_scene_enter(const MgGame *g, int arg);
void mg_scene_update(void);
void mg_scene_draw(void);
void mg_scene_leave(void);
/* Defines SCENE_<sc> for game `g` (one line per game file). */
#define MG_DEFINE_SCENE(sc, name, g)                                       \
    static void sc##_enter(int arg) { mg_scene_enter(&(g), arg); }         \
    const DoomoScene sc = { name, sc##_enter, mg_scene_leave,              \
                            mg_scene_update, mg_scene_draw, 0 }

/* ---- input (real pad, or the bot while autoplaying) -------------------- */
int  mg_down(uint32_t b);
int  mg_pressed(uint32_t b);
/* Local deterministic-per-round randomness (seeded at round start). */
extern DoomoRng mg_rng;
int  mg_rand(int n);                        /* 0..n-1 */
int  mg_between(int lo, int hi);            /* lo..hi */

/* ---- scoring ----------------------------------------------------------- */
/* Adds base * combo multiplier * tier multiplier; pops "+N" at x,y (screen,
 * before shake). Returns the points added. */
int  mg_add_score(int base, int x, int y);
/* Raw points (no multipliers, may be negative), popup at x,y. */
void mg_add_raw(int pts, int x, int y);
int  mg_combo_mult(void);                   /* x1 .. x5                   */
void mg_combo_hit(int x, int y);
void mg_combo_break(void);
void mg_lose_life(int x, int y);
void mg_gain_life(int x, int y);
void mg_end(const char *text);        /* round over: "TIME UP!" ...       */
/* The H$ a final score pays (score / pay_div, at most MG_REWARD_CAP, plus
 * the medal bonus 10 / 25 / 50). */
#define MG_REWARD_CAP 250
int  mg_reward_for(const MgGame *g, int32_t score);
int  mg_medal_for(const MgGame *g, int32_t score);   /* MG_MEDAL_*        */
uint32_t mg_best(const MgGame *g);
/* Records a finished round: best score, money, unlocks. Returns 1 if new
 * best. (Called by the flow; exposed for tests.) */
int  mg_finish_round(const MgGame *g, int32_t score);
/* Trophy unlock without the flow (games call it for MG_UL_* specials). */
void mg_trophy(int id);

/* ---- juice ------------------------------------------------------------- */
enum { MGFX_SPARK, MGFX_CONFETTI, MGFX_HEART, MGFX_STAR, MGFX_PUFF,
       MGFX_CRUMB, MGFX_RING, MGFX_DROP, MGFX_EMBER };
void mg_fx(int kind, int x, int y, int vx16, int vy16, uint8_t color, int life);
void mg_burst(int kind, int x, int y, int n, int speed16, uint8_t color);
void mg_confetti_rain(int n);
/* "+50" (text NULL) or a word ("PERFECT!") rising from x,y. */
void mg_popup(int x, int y, const char *text, int value, uint8_t color);
void mg_shake_add(int amount);
void mg_flash(uint8_t color, int ticks);
void mg_hitstop(int ticks);
void mg_fx_update(void);
void mg_fx_draw(void);
void mg_fx_clear(void);
int  mg_fx_live(void);                      /* live particles (tests)     */
/* A cute splat (GORE_CUTE: confetti + hearts; brutal: red drops). */
void mg_splat(int x, int y, int n);

/* ---- shared drawing ---------------------------------------------------- */
int  mg_w(void);
int  mg_h(void);
int  mg_big(void);                          /* 1 on 320x240-class screens */
/* Vertical candy-stripe tent wall with a scalloped valance. */
void mg_tent(int y0, int y1, uint8_t a, uint8_t b, uint8_t trim, int t);
void mg_bunting(int x0, int x1, int y, int sag, int t);
/* Pastel fair sky from y0 to y1 (gradient, clouds, ferris wheel, tents);
 * the Hellfire tier paints the hellish night version. */
void mg_fair_sky(int y0, int y1, int t);
/* Chasing marquee bulbs around a rect. */
void mg_bulbs(int x, int y, int w, int h, int t);
/* Soft blob shadow. */
void mg_shadow(int cx, int cy, int rx);
/* Treats (falling goodies, card fallbacks, wheel prizes). r ~ radius;
 * sx/sy squash in 1/256. */
enum { TREAT_CUPCAKE, TREAT_DONUT, TREAT_COOKIE, TREAT_CANDY, TREAT_STAR,
       TREAT_BAGEL, TREAT_HEART, TREAT_COUNT };
void mg_treat(int kind, int cx, int cy, int r, int sx256, int sy256, int t);
/* A procedural plush imp (red ball, horns, bat wings). rx/ry = body
 * radii (squash), mood: an expression, optionally | IMP_GOLD. */
enum { IMP_GRIN, IMP_BONK, IMP_ANGRY, IMP_SNEAKY, IMP_GOLD = 8 };
void mg_imp(int cx, int cy, int rx, int ry, int t, int mood);
/* A puffy vape wraith (lilac cloud ghost with a vape pen). */
void mg_wraith(int cx, int cy, int r, int t, int hurt);
/* Father Fuzz, the plushie priest (brown bear, white collar). */
void mg_fuzz(int cx, int cy, int r, int t, int ouch);
/* The fair mascot "Tix" (a plush ticket bunny) for absent hosts. */
void mg_mascot(int cx, int cy, int r, int t);
/* Big wobbly text (per-letter bounce) centred at cx. */
void mg_big_text(int cx, int y, const char *s, int scale, uint8_t c,
                 uint8_t outline, int t);
/* Text with a dark outline (HUD). */
void mg_text(int x, int y, const char *s, uint8_t c);
void mg_textc(int cx, int y, const char *s, uint8_t c);
/* Rows [top, top+rows) of an image as a stack copy with its own uid. */
DoomoImage mg_crop(const DoomoImage *src, int top, int rows);
/* Host portrait framed like a booth photo (or the mascot when the host
 * does not live on this island). */
int  mg_host_alive(int id);
void mg_host_frame(int x, int y, int id, int expr, int t);
const char *mg_host_name(int id);
/* HUD pieces */
void mg_hud(void);
void mg_draw_hearts(int x, int y, int n, int max);
void mg_medal_icon(int cx, int cy, int medal, int t);
/* Small 2-bit sprite blit from packed pixels (4 per byte, MSB first):
 * values 0 clear, 1..3 -> pal[0..2]. sy256 squashes vertically (256 =
 * 1.0), the sprite's bottom row stays at y + h - 1. */
void mg_blit2(int x, int y, const uint8_t *pix, int w, int h,
              const uint8_t pal[3], int sy256);
/* Tier name / colours */
extern const char *const mg_tier_names[MG_TIERS];
uint8_t mg_tier_color(int tier);

/* ---- games -------------------------------------------------------------- */
extern const MgGame mg_game_catch, mg_game_whack, mg_game_dodge,
                    mg_game_gallery, mg_game_derby, mg_game_memory;
extern const MgGame *const mg_games[MG_GAME_COUNT];

/* Wheel of Fate (mg_wheel.c) */
#define MG_WHEEL_WEDGES 10
#define MG_WHEEL_HOST 506                  /* Wagerine, Cacoblob Bookie  */
int  mg_wheel_ready(void);                 /* not spun yet today          */
/* Grant wedge's prize / curse; returns the H$ change. */
int  mg_wheel_apply(int wedge);
extern const char *const mg_wheel_names[MG_WHEEL_WEDGES];
int  mg_wheel_is_curse(int wedge);

/* Derby helpers (mg_derby.c) for tests */
int  mg_derby_pet_speed(void);             /* your racer's speed rating   */

/* Memory helpers (mg_memory.c) for tests: picks `pairs` distinct faces
 * (>= 0 resident ids, < 0 = fallback treat/imp faces). Returns pairs. */
int  mg_memory_pick_faces(int pairs, int16_t *out, uint32_t seed);

#endif
