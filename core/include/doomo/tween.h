/* tween.h - comic-timing curves, keyframe tables and springs.
 *
 * CONTRACT (docs/design/story/SPEC.md sec.4): written by S0, maintained by S1
 * (add-only after S0; changing a declaration needs an RFC to I9).
 * Implementation: core/src/tween.c + tween_tables.c (S1). Until those land
 * every symbol below is a weak stub in core/src/stubs.c (S0 block) that logs
 * "STUB:" once and returns a neutral value.
 *
 * Units: fix16 time (0..FIX_ONE), Q8.8 scales (256 = 1.0), Q4 spring
 * positions (16 = 1 px), Q14 spring coefficients. Integer only, no floats.
 * Source of truth: comic_timing.md sec.7.1 + story_final.md sec.15.4; the tables
 * are the trailer's (CT Appendix A), sampled 1:1 at 30 Hz (tk.h TK_FPS 30).
 */
#ifndef DOOMO_TWEEN_H
#define DOOMO_TWEEN_H

#include "types.h"

fix16 tw_t(int age, int dur);              /* clamp(age/dur) as fix16            */
fix16 tw_smooth(fix16 t);                  /* t*t*(3-2t)                (tk_ease) */
fix16 tw_in2(fix16 t);                     /* t^2: falls, flings, sneaks          */
fix16 tw_out2(fix16 t);                    /* 1-(1-t)^2: crown drop, cloud spread */
fix16 tw_back(fix16 t, fix16 s);           /* UI slides only; s = 0x13333 -> ~5%  */
fix16 tw_bounce(fix16 t);                  /* 33-entry LUT of tk_bounce + lerp,
                                              clamps to 1.0 at i >= 30            */
int   tw_lerp(int a, int b, fix16 t);

/* Keyframe tables: the trailer's real workhorse. One entry per frame.
 * Clamps: age < 0 -> v[0], age >= n -> v[n-1]. */
typedef struct TwTab { const int16_t *v; uint8_t n; } TwTab;
int tw_tab(const TwTab *t, int age);

/* Squash = scale pair + y offset, anchored at the feet / contact point.
 * amount256 = OPT_UI_BOUNCE scaling (0, 128, 256): lerps every scale toward
 * 256 and every offset toward 0. Amplitude only, never timing (CT sec.9). */
typedef struct TwSquash { int16_t sx, sy; int8_t dy; } TwSquash;
TwSquash tw_squash(const TwTab *sx, const TwTab *sy, const TwTab *dy, int age, int amount256);

/* The shared library of trailer tables (CT Appendix A + SF sec.15.4), in ROM. */
extern const TwTab TW_POP_BUBBLE, TW_POP_LETTER, TW_POP_SOUL, TW_POP_LOCKUP,
                   TW_POP_EYE, TW_POP_MENU, TW_POP_BLOB, TW_POP_BURST,
                   TW_SLAM_STAMP, TW_SLAM_COUNTER, TW_SLAM_STICKER, TW_SLAM_STICK2,
                   TW_DROP_TITLE_SX, TW_DROP_TITLE_SY, TW_DROP_TITLE_DY,
                   TW_DROP_LET_SX, TW_DROP_LET_SY, TW_DROP_LET_DY,
                   TW_DROP_SOFT_SX, TW_DROP_SOFT_SY, TW_DROP_SOFT_DY,   /* s14 handheld */
                   TW_HEART_LAND_SQ, TW_DEVICE_DY, TW_SIGN_DY, TW_CLUNK, TW_CLAP,
                   TW_WINDOW_BOING_DY, TW_BRAKE_DX, TW_ZOOM_WOBBLE_DY,
                   TW_HOP1, TW_HOP2, TW_TAG_DY, TW_DEADPAN_DY;

/* Table ids, for data that names a table by number (BOP_POP's b, the .dsc
 * compiler's [POP TW_x]). tw_table(id) returns NULL for an unknown id.
 * Append only (ids are stored in beat scripts). */
enum {
    TWT_POP_BUBBLE, TWT_POP_LETTER, TWT_POP_SOUL, TWT_POP_LOCKUP, TWT_POP_EYE,
    TWT_POP_MENU, TWT_POP_BLOB, TWT_POP_BURST,
    TWT_SLAM_STAMP, TWT_SLAM_COUNTER, TWT_SLAM_STICKER, TWT_SLAM_STICK2,
    TWT_DROP_TITLE_SX, TWT_DROP_TITLE_SY, TWT_DROP_TITLE_DY,
    TWT_DROP_LET_SX, TWT_DROP_LET_SY, TWT_DROP_LET_DY,
    TWT_DROP_SOFT_SX, TWT_DROP_SOFT_SY, TWT_DROP_SOFT_DY,
    TWT_HEART_LAND_SQ, TWT_DEVICE_DY, TWT_SIGN_DY, TWT_CLUNK, TWT_CLAP,
    TWT_WINDOW_BOING_DY, TWT_BRAKE_DX, TWT_ZOOM_WOBBLE_DY,
    TWT_HOP1, TWT_HOP2, TWT_TAG_DY, TWT_DEADPAN_DY,
    TWT_COUNT
};
const TwTab *tw_table(int id);

/* Springs: exact sampled damped oscillator, 2 multiplies per tick.
 * 16 B of RAM per live spring (the camera uses two, the held furniture
 * one, the boss belly one). */
enum { TW_JELLY, TW_SLAM4, TW_SLAM7, TW_SLAM8, TW_BRAKE, TW_BOING, TW_WOBBLE,
       TW_FOLLOW, TW_SPRING_COUNT };
typedef struct TwSpring { int32_t e, ep, target; int16_t a1, a2; } TwSpring;
void    tw_spring_init(TwSpring *s, int preset, int32_t pos_q4);
void    tw_spring_target(TwSpring *s, int32_t target_q4); /* keeps velocity            */
void    tw_spring_punch(TwSpring *s, int32_t d_q4);       /* offset that snaps back    */
void    tw_spring_shove(TwSpring *s, int32_t dv_q4);      /* velocity impulse          */
int32_t tw_spring_step(TwSpring *s);                      /* returns position, Q4      */

/* ---- S1 additions (add-only) ---- */
/* OPT_UI_BOUNCE as tw_squash's amount256: off 0, subtle 128, boingy 256. */
int tw_bounce_amount(void);
/* 1 once the spring has settled exactly on its target (e == ep == 0). */
int tw_spring_resting(const TwSpring *s);

#endif
