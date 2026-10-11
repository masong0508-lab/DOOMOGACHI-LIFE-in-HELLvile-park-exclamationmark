/* mg.h - private glue of the FUN FAIR minigames (mg_*.c, owner: minigames).
 * Not part of the contract.
 *
 * Every arcade game is a DoomoScene driven by the shared flow in mg.c:
 *
 *   TITLE card (host portrait + tutorial line, 3 difficulty tiers, best
 *   score and medals)  ->  READY? GO!  ->  PLAY (pause on START)  ->
 *   TIME UP / OUCH banner  ->  RESULTS (score count-up, NEW BEST, medal,
 *   H$ reward, unlock toasts; Retry / Leave).
 *
 * A game only supplies an MgGame: start / update / draw hooks plus an
 * autoplay bot (tests, screenshots, attract mode). Its state lives in the
 * shared arena (one game runs at a time). Juice helpers: particles, score
 * popups, screen shake, flashes and hit-stop; scoring helpers apply the
 * combo and tier multipliers.
 *
 * Scene arg: bits 0-1 preselect a tier (1 Sweet, 2 Spicy, 3 Hellfire),
 * MG_ARG_AUTOSTART skips the title card, MG_ARG_AUTOPLAY lets the bot
 * play (both used by runner scripts: "push MG_CATCH 50").
 */
#ifndef DOOMO_MG_H
#define DOOMO_MG_H

#include "doomo/doomo.h"

enum { MG_CATCH, MG_WHACK, MG_DODGE, MG_GALLERY, MG_DERBY, MG_MEMORY,
       MG_GAME_COUNT };
enum { MG_TIER_SWEET, MG_TIER_SPICY, MG_TIER_HELLFIRE, MG_TIERS };

#define MG_ARG_AUTOSTART 0x10
#define MG_ARG_AUTOPLAY  0x20

/* Unlock ids (bit indices; the minigames own 864..895). */
#define MG_UL_BASE        864
#define MG_UL_MEDAL(g, m) (MG_UL_BASE + (g) * 3 + (m))   /* 864..881 */
#define MG_UL_REGULAR     882   /* play every arcade game              */
#define MG_UL_GOLD_ALL    883   /* gold medal in every game            */
#define MG_UL_JACKPOT     884   /* Wheel of Fate jackpot               */
#define MG_UL_UNTOUCHABLE 885   /* survive Vape Cloud Dodge unhit      */
#define MG_UL_FUZZ_FAN    886   /* never bonk Father Fuzz in a run     */
#define MG_UL_RECALL      887   /* Memory Match with no misses         */
#define MG_UL_PHOTO       888   /* win the Derby by a nose             */
#define MG_UL_SUGAR       889   /* three fevers in one Cupcake Catch   */
#define MG_UL_PACIFIST    890   /* gallery run without hitting a citizen */
#define MG_UL_CURSED      891   /* get cursed by the Wheel of Fate     */
#define MG_UL_LAST        891

typedef struct MgGame {
    const char *title;          /* "CUPCAKE CATCH" (big title text)      */
    const char *how;            /* tutorial line said by the host        */
    const char *keys;           /* controls line                         */
    uint8_t  id;                /* MG_*                                  */
    uint8_t  prog;              /* PROG_HS_*                             */
    uint8_t  song;              /* SONG_*                                */
    uint8_t  icon;              /* ICON_* glyph                          */
    uint16_t host;              /* roster id of the host                 */
    uint16_t medal[3];          /* bronze / silver / gold scores         */
    uint16_t pay_div;           /* H$ = score / pay_div (+ medal bonus)  */
    uint16_t time;              /* round length in ticks (0 = none)      */
    uint8_t  lives;             /* hearts (0 = none)                     */
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
    uint8_t  phase, tier, sel, autoplay, autostart, over, result_sel;
    uint8_t  lives, new_best, medal, prev_medal, flash_c, flash_t;
    uint8_t  freeze, shake, quiet_hud;
    int8_t   ox, oy;            /* screen shake offset this frame         */
    uint16_t t;                 /* ticks in the current phase             */
    uint16_t time_left;
    uint16_t combo, best_combo;
    uint16_t score_bump;
    int32_t  score, shown;
    int32_t  reward;
    const char *end_text;
    uint32_t bot_now, bot_prev;
    uint32_t play_ticks;
    uint8_t  stat_a, stat_b;    /* per-game flags for trophies            */
} MgState;
extern MgState mg;

/* The shared state arena (one game at a time). */
#define MG_ARENA_BYTES 2048
extern uint32_t mg_arena[MG_ARENA_BYTES / 4];
#define MG_ARENA(type) ((type *)(void *)mg_arena)
#define MG_ARENA_FITS(type) \
    DOOMO_STATIC_ASSERT(sizeof(type) <= MG_ARENA_BYTES, type##_fits_arena)

/* ---- scene plumbing --------------------------------------------------- */
void mg_scene_enter(const MgGame *g, int arg);
void mg_scene_update(void);
void mg_scene_draw(void);
void mg_scene_leave(void);

/* ---- input (real pad, or the bot while autoplaying) -------------------- */
int  mg_down(uint32_t b);
int  mg_pressed(uint32_t b);
int  mg_repeat(uint32_t b);

/* ---- scoring ----------------------------------------------------------- */
/* Adds base * combo multiplier * tier multiplier; pops "+N" at x,y (screen,
 * before shake). Returns the points added. */
int  mg_add_score(int base, int x, int y);
/* Raw points (no multipliers, may be negative), popup text optional. */
void mg_add_raw(int pts, int x, int y);
int  mg_combo_mult(void);
void mg_combo_hit(int x, int y);
void mg_combo_break(void);
void mg_lose_life(int x, int y);
void mg_end(const char *text);        /* round over: "TIME UP!" ...       */
/* The H$ a final score pays (with the medal bonus). */
int  mg_reward_for(const MgGame *g, int32_t score);
int  mg_medal_for(const MgGame *g, int32_t score);   /* 0 none .. 3 gold */
uint32_t mg_best(const MgGame *g);
/* Records a finished round: best score, money, unlocks. Returns 1 if new
 * best. (Called by the flow; exposed for tests.) */
int  mg_finish_round(const MgGame *g, int32_t score);

/* ---- juice ------------------------------------------------------------- */
enum { MGFX_SPARK, MGFX_CONFETTI, MGFX_HEART, MGFX_STAR, MGFX_PUFF,
       MGFX_CRUMB, MGFX_RING, MGFX_DROP };
void mg_fx(int kind, int x, int y, int vx16, int vy16, uint8_t color, int life);
void mg_burst(int kind, int x, int y, int n, int speed16, uint8_t color);
void mg_confetti_rain(int n);
void mg_popup(int x, int y, const char *text, int value, uint8_t color);
void mg_shake_add(int amount);
void mg_flash(uint8_t color, int ticks);
void mg_hitstop(int ticks);
void mg_fx_update(void);
void mg_fx_draw(void);
void mg_fx_clear(void);

/* ---- shared drawing ---------------------------------------------------- */
int  mg_w(void);
int  mg_h(void);
/* Vertical candy-stripe tent wall with a scalloped valance and bunting. */
void mg_tent(int y0, int y1, uint8_t a, uint8_t b, uint8_t trim, int t);
void mg_bunting(int x0, int x1, int y, int sag, int t);
/* Pastel fair sky: gradient, clouds, a turning ferris wheel, tents. */
void mg_fair_sky(int y0, int y1, int t);
/* Chasing marquee bulbs around a rect. */
void mg_bulbs(int x, int y, int w, int h, int t);
/* Soft blob shadow. */
void mg_shadow(int cx, int cy, int rx);
/* Treats: cupcake, donut, cookie, candy, star cake (also used as fallback
 * card faces and wheel prizes). size ~ radius. */
enum { TREAT_CUPCAKE, TREAT_DONUT, TREAT_COOKIE, TREAT_CANDY, TREAT_STAR,
       TREAT_BAGEL, TREAT_HEART, TREAT_COUNT };
void mg_treat(int kind, int cx, int cy, int sx256, int sy256, int t);
/* A tiny procedural plush imp (cute red ball with horns, bat wings). */
void mg_imp(int cx, int cy, int r, int t, int mood);
/* Big wobbly text (per-letter bounce) centred at cx. */
void mg_big_text(int cx, int y, const char *s, int scale, uint8_t c,
                 uint8_t outline, int t);
/* Text with a dark outline (HUD). */
void mg_text(int x, int y, const char *s, uint8_t c);
void mg_textc(int cx, int y, const char *s, uint8_t c);
/* Top rows of an image (holes, cards): a stack copy with h = rows. */
DoomoImage mg_crop(const DoomoImage *src, int top, int rows);
/* Draw `img` horizontally squashed to w256/256 of its width by skipping
 * columns (card flips): drawn as vertical 1-px slices, cheap for small
 * images. */
void mg_image_squash_x(int cx, int y, const DoomoImage *img, int w256);
/* Host portrait framed like a booth photo (or the mascot when the host
 * does not live on this island). */
int  mg_host_alive(int id);
void mg_host_frame(int x, int y, int id, int expr, int t);
/* HUD pieces */
void mg_hud(void);
void mg_draw_hearts(int x, int y, int n, int max);
/* Small 2-bit sprite blit: pix values 0 clear, 1..3 -> pal[0..2]. The
 * buffer is w*h bytes, scaled by `scale` (1..2). */
void mg_blit2(int x, int y, const uint8_t *pix, int w, int h, const uint8_t pal[3],
              int scale, int flip);
/* Fixed-point helpers */
int  mg_isqrt(int v);

/* ---- games -------------------------------------------------------------- */
extern const MgGame mg_game_catch, mg_game_whack, mg_game_dodge,
                    mg_game_gallery, mg_game_derby, mg_game_memory;
extern const MgGame *const mg_games[MG_GAME_COUNT];
extern const char *const mg_tier_names[MG_TIERS];

/* Wheel of Fate (mg_wheel.c) */
int  mg_wheel_ready(void);                 /* not spun yet today          */
int  mg_wheel_apply(int wedge);            /* grant a prize / curse       */
#define MG_WHEEL_WEDGES 10
extern const char *const mg_wheel_names[MG_WHEEL_WEDGES];

/* Derby helpers (mg_derby.c) for tests */
int  mg_derby_pet_speed(void);             /* your racer's speed rating   */

/* Memory helpers (mg_memory.c) for tests */
int  mg_memory_pick_faces(int pairs, int16_t *out, uint32_t seed);

#endif
