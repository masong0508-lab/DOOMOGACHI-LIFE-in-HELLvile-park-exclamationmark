/* tk.h - tiny pixel-animation toolkit for the DOOMOGACHI LIFE teaser trailer.
 *
 * Host-only tool (floats allowed). Draws into a 240x160 framebuffer of
 * palette indices using the game's REAL master palette (core/src/palette.c),
 * so colours are exactly what the GBA/PS1 builds use. Palette index macros
 * come straight from the game: PAL_WHITE, PAL_UI_PINK, PAL_PAINT(PAINT_ROSE,1),
 * PAL_SKIN(SKIN_PEACH,1), PAL_RAMP(RAMP_LAVA,5) ... (core/include/doomo/palette.h)
 *
 * A scene is two functions:
 *     void scene_NAME(int f, int n);          draw local frame f of n (30 fps)
 *     void scene_NAME_audio(double t0, double dur);  schedule sfx/notes
 * registered in timeline.c.
 */
#ifndef TK_H
#define TK_H

#include <stdint.h>
#include "doomo/palette.h"

#define TW 240
#define TH 160
#define TK_FPS 30

extern uint8_t tk_fb[TH][TW];

/* ---- primitives (all clip safely) ------------------------------------- */
void tk_clear(uint8_t c);
void tk_px(int x, int y, uint8_t c);
uint8_t tk_get(int x, int y);
void tk_rect(int x, int y, int w, int h, uint8_t c);
void tk_frame(int x, int y, int w, int h, uint8_t c);         /* outline */
void tk_line(int x0, int y0, int x1, int y1, uint8_t c);
void tk_circle(int cx, int cy, int r, uint8_t c);             /* filled  */
void tk_ring(int cx, int cy, int r, uint8_t c);               /* outline */
void tk_ellipse(int cx, int cy, int rx, int ry, uint8_t c);   /* filled  */
void tk_tri(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t c);
void tk_vgrad(int x, int y, int w, int h, const uint8_t *cols, int ncols); /* dithered */
void tk_dither_rect(int x, int y, int w, int h, uint8_t c, int level); /* level 0..16 coverage */

/* ---- text: the 5x8 game font, 6 px advance, scale 1..8 ----------------- */
int  tk_text(int x, int y, const char *s, uint8_t c, int scale);   /* returns width */
int  tk_text_w(const char *s, int scale);
/* Centered caption with a 1-px (x scale) outline in `oc` - the trailer look. */
void tk_caption(int cy, const char *s, uint8_t c, uint8_t oc, int scale);
void tk_text_outline(int x, int y, const char *s, uint8_t c, uint8_t oc, int scale);
/* Typewriter: draws the first `chars` characters of s (word-wrapped to w px). */
void tk_text_wrap(int x, int y, int w, const char *s, uint8_t c, int chars);

/* ---- UI widgets in the game's cutesy style ----------------------------- */
enum { TKW_CUTE, TKW_MINT, TKW_CREAM, TKW_HELL, TKW_LCD, TKW_DARK };
void tk_window(int x, int y, int w, int h, int style);
/* GBA dialogue box at the bottom: speaker name plate + typewriter text.
 * `t` = frames since the line started (text speed ~1.5 chars/frame). */
void tk_dialog(const char *speaker, const char *text, int t, int style);

/* ---- sprites from ASCII art -------------------------------------------- */
/* rows: NULL-terminated array of strings. Each char maps through `map`
 * (map[(uint8_t)ch]); 0 = transparent ('.' and ' ' default transparent).
 * scale >= 1, flip mirrors horizontally. */
void tk_sprite(const char *const *rows, int x, int y, const uint8_t *map,
               int scale, int flip);
/* A default char->colour map you can copy and tweak:
 *  k black outline, w white, p pink, P dark pink, r red, R blood, o orange,
 *  y yellow, g green, G dark green, b blue, B dark blue, v vape purple,
 *  V dark purple, s skin, S skin shadow, n brown, N dark brown, e grey,
 *  E dark grey, c cream, m mint, l lcd green, L dark lcd, f flesh, F dark flesh,
 *  h hellfire, x bone */
extern const uint8_t TK_MAP_DEFAULT[128];

/* ---- the cute chibi citizen (big head, tiny body) ---------------------- */
enum { TKE_NORMAL, TKE_HAPPY, TKE_SAD, TKE_ANGRY, TKE_LOVE, TKE_SHOCK,
       TKE_SLEEPY, TKE_POSSESSED, TKE_DEAD, TKE_SPARKLE, TKE_SMUG };
enum { TKM_SMILE, TKM_OPEN, TKM_FROWN, TKM_FLAT, TKM_FANGS, TKM_O, TKM_CAT,
       TKM_SCREAM };
enum { TKH_BALD, TKH_SHORT, TKH_SPIKY, TKH_BOB, TKH_LONG, TKH_TWINTAILS,
       TKH_AFRO, TKH_MOHAWK, TKH_BUN };
typedef struct TkChibi {
    uint8_t skin;      /* palette index of the base skin colour            */
    uint8_t hair;      /* palette index of base hair colour                */
    uint8_t outfit;    /* palette index of base outfit colour              */
    uint8_t eyes;      /* palette index of iris colour                     */
    uint8_t hair_style;/* TKH_*                                            */
    uint8_t horns;     /* 0 none, 1 little devil horns, 2 ram horns        */
    uint8_t wings;     /* 0 none, 1 bat, 2 angel                           */
    uint8_t halo;      /* 0/1                                              */
    uint8_t tail;      /* 0/1 devil tail                                   */
    uint8_t blush;     /* 0/1                                              */
    uint8_t hat;       /* 0 none, 1 party hat, 2 priest hat, 3 marine helm */
} TkChibi;
/* Draw a chibi standing with feet at (x, y_feet). size: 1 = 16x24 sprite,
 * 2 = 32x48, 3 = 48x72. expr TKE_*, mouth TKM_*. bob = walk/idle frame. */
void tk_chibi(int x, int y_feet, const TkChibi *c, int size, int expr,
              int mouth, int bob, int flip);
/* Some ready-made citizens. */
extern const TkChibi TK_HERO, TK_MAYOR, TK_FUZZ, TK_RIVET, TK_NANA, TK_PIP,
                     TK_VEX, TK_GOTH, TK_IMP;
/* Deterministic random citizen #i (for crowds), hellborn if bad != 0. */
TkChibi tk_random_citizen(uint32_t i, int bad);

/* ---- effects / post-processing (apply after drawing the frame) ---------- */
/* Rot the cute palette into hell: 0 = untouched .. 16 = fully hellified
 * (ordered dither in between, so it creeps in pixel by pixel). */
void tk_post_hellify(int amount);
void tk_post_darken(int level);             /* 0..15 (15 = unchanged)   */
void tk_post_flash(uint8_t color, int amount); /* 0..16 dithered flash  */
void tk_post_shake(int dx, int dy);          /* shift whole frame       */
void tk_post_glitch(int seed, int strength); /* row tearing + colour noise */
void tk_post_scanlines(void);                /* subtle LCD lines        */
void tk_post_fade(int amount);               /* 0..16 to black          */
/* particles: deterministic confetti / gibs / sparkles / hearts burst at
 * (x,y) that started `age` frames ago. */
void tk_confetti(int x, int y, int age, uint32_t seed);
void tk_gibs(int x, int y, int age, uint32_t seed);
void tk_sparkles(int x, int y, int age, uint32_t seed, int radius);
void tk_hearts(int x, int y, int age, uint32_t seed);
void tk_vape_cloud(int x, int y, int age, uint32_t seed, int size); /* purple puffs */
void tk_star(int x, int y, int r, uint8_t c);  /* 4-point twinkle */
void tk_heart(int x, int y, int size, uint8_t c);

/* ---- math helpers --------------------------------------------------------- */
uint32_t tk_hash(uint32_t x);
int   tk_rand(uint32_t *s, int n);
float tk_ease(float t);            /* smoothstep 0..1 */
float tk_bounce(float t);          /* overshoot bounce 0..1 */
float tk_clampf(float v, float lo, float hi);
int   tk_lerpi(int a, int b, float t);
float tk_tri_wave(float t);        /* 0..1..0 */

/* ---- audio (22050 Hz mono, mixed into trailer.wav) ------------------------ */
enum { TKV_SQ12, TKV_SQ25, TKV_SQ50, TKV_TRI, TKV_SAW, TKV_NOISE, TKV_SINE };
/* Absolute time in seconds. midi 60 = middle C. vol 0..1. */
void tk_note(double t, double dur, int midi, int wave, float vol);
/* Slide from midi_a to midi_b over dur. */
void tk_slide(double t, double dur, int midi_a, int midi_b, int wave, float vol);
enum { SFX_BLIP, SFX_SELECT, SFX_BOING, SFX_POP, SFX_SQUEAK, SFX_SPLAT,
       SFX_SHOTGUN, SFX_EXPLOSION, SFX_SCRATCH, SFX_HEARTBEAT, SFX_BEEP,
       SFX_CHOIR, SFX_FANFARE, SFX_WHOOSH, SFX_ROAR, SFX_SPARKLE, SFX_SPLASH,
       SFX_THUD, SFX_GLITCH, SFX_KAZOO, SFX_COUNT_ };
void tk_sfx(double t, int sfx, float vol);
/* Drum helpers for the soundtrack. */
void tk_kick(double t, float vol);
void tk_snare(double t, float vol);
void tk_hat(double t, float vol);

#endif
