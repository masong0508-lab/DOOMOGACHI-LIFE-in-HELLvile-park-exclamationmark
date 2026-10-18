/* beat.h - frame-exact scripts for cutscenes, dialogue beats and overworld
 * gags. Every event hangs off a named anchor (label), so re-timing one beat
 * drags its sound, shake and captions with it (the trailer's music.c FR(n)
 * problem, solved: cues hang off beat labels, never frame constants).
 *
 * CONTRACT (docs/design/story/SPEC.md sec.4): written by S0, maintained by S1
 * (add-only). Implementation: core/src/beat.c (S1, resident on the PS1).
 * Until it lands every function is a weak stub in core/src/stubs.c.
 *
 * Source: comic_timing.md sec.7.2 with SPEC R5's renames (the ladder constants
 * are RUNG_*, because story.h owns BEAT_*), plus the SF sec.15.3 additions.
 */
#ifndef DOOMO_BEAT_H
#define DOOMO_BEAT_H

#include "types.h"

/* The beat ladder, in 30 Hz frames (CT sec.2). Gaps sit on these rungs (+-1);
 * the lint test (S2, test_story_lint.c) checks >= 75% of gaps. */
enum { RUNG_TICK = 1, RUNG_R2 = 2, RUNG_R3 = 3, RUNG_R5 = 5, RUNG_R8 = 8,
       RUNG_R13 = 13, RUNG_R21 = 21, RUNG_R26 = 26 };

enum {                     /* op            a                b                    */
    BOP_END,
    BOP_LABEL,             /* label id      -                                     */
    BOP_WAIT_A,            /* label id      min frames      (fires on A press)    */
    BOP_HOLD,              /* label id      frames          (A can't skip)        */
    BOP_SAY, BOP_CAPTION,  /* speaker/style text id (txt.h)                       */
    BOP_POP,               /* object id     TWT_* table id (tween.h)              */
    BOP_EXPR,              /* actor         EXPR_* | mouth<<8                     */
    BOP_ACT,               /* actor         action id (hop, wave, flee, turn)     */
    BOP_FREEZE, BOP_THAW, BOP_PANIC,  /* actor (-1 = all): clamp / resume / 2x clock */
    BOP_HITSTOP,           /* frames        -                                     */
    BOP_SHAKE,             /* SHK_* profile -                                     */
    BOP_PUNCH,             /* dx            dy  (camera spring kick)              */
    BOP_ZOOM,              /* scale Q8.8    frames                                */
    BOP_FLASH, BOP_FADE,   /* colour        level16 | frames<<8                   */
    BOP_IRIS_OUT, BOP_IRIS_IN, /* centre actor/prop id, hold | heart<<6 | (radius/2)<<7 */
    BOP_CUT,               /* shot id       flags (BCUT_COVER)                    */
    BOP_INSERT,            /* insert id     frames (3-4; whiplash, auto-mutes)    */
    BOP_SFX,               /* SFX_*         volume                                */
    BOP_MUSIC, BOP_MUSIC_MUTE, BOP_MUSIC_DUCK, BOP_TAPESTOP, /* song | frames | gain */
    BOP_FLAG,              /* story flag    value                                 */
    /* ---- SF sec.15.3 / SPEC sec.4 additions ---- */
    BOP_STILL,             /* actor (-1 = all): idle clock 0 (deadpan, horror)     */
    BOP_TWITCH,            /* actor         possessed ear twitch on/off            */
    BOP_NPC,               /* actor         act | arg<<8 (overworld NPC ops; host) */
    BOP_CAM,               /* CAM_* mode    arg (pan/zoom/creep/brake; host)       */
    BOP_TINT,              /* TINT_*        level16                                */
    BOP_PAL_LERP,          /* PAL_SEPIA..   level16 | frames<<8 (camfx.h)          */
    BOP_NOBUTTON,          /* -             -   (a marker the lint reads)          */
    BOP_SHOT,              /* shot id       -   (stage: backdrop, actors, camera; host) */
    BOP_HEADTURN,          /* actor         angle (brad, 256 = 1/16 turn) | frames<<8 (host) */
    BOP_COUNT
};
enum { BCUT_COVER = 1 };   /* BOP_CUT b: the cut hides behind a cover sprite */

/* Reserved label ids the lint reads (SPEC sec.4). L_PUNCH marks the punch; a
 * button prompt (L_BUTTON) follows it 12-25 frames later, or BOP_NOBUTTON. */
enum { BEAT_LABEL_MAX = 32, L_PUNCH = 30, L_BUTTON = 31 };

typedef struct BeatEv {
    uint16_t at;           /* frames after the anchor fired                 */
    uint8_t  anchor;       /* label id (0 = script start)                   */
    uint8_t  op;           /* BOP_*                                         */
    int16_t  a, b;
} BeatEv;                  /* 8 bytes, ROM / pack; a 60 s cutscene ~ 150 ev */
typedef struct BeatScript { const BeatEv *ev; uint16_t n; uint8_t nlabels; } BeatScript;

/* beat_tick() results. BT_EXT: the event in beat_ext_event() is a host kind
 * (BOP_NPC, BOP_CAM, BOP_SHOT, BOP_HEADTURN, and SAY/CAPTION drawing) that
 * the calling scene handles in its own code (SPEC C6: resident code never
 * calls overlay code); call beat_tick() again the same frame until it
 * returns BT_RUN or BT_DONE. */
enum { BT_DONE = 0, BT_RUN = 1, BT_EXT = 2 };

void beat_start(const BeatScript *s);
int  beat_tick(void);              /* BT_*; call once per 30 Hz tick (and again after BT_EXT) */
const BeatEv *beat_ext_event(void);/* the host event of the last BT_EXT, else NULL       */
int  beat_running(void);           /* 1 between beat_start and BOP_END / beat_skip       */
int  beat_age(int label);          /* frames since the label fired, -1 if not yet:       */
                                   /*   draw code does tw_tab(&TW_SLAM_STAMP, beat_age(L_FAIL1)) */
int  beat_clock(int actor);        /* per-actor animation clock: frozen / 2x / normal    */
void beat_skip(void);              /* START: jump to BOP_END, applying flags only        */
void beat_press_a(void);           /* A pressed: fires a pending WAIT_A anchor           */

/* Platform draw code may override an actor's clock (e.g. the GBA OBJ path
 * holding a squash frame). mode: BCLK_NORMAL, BCLK_FROZEN, BCLK_DOUBLE,
 * BCLK_STILL. actor -1 = all. */
enum { BCLK_NORMAL, BCLK_FROZEN, BCLK_DOUBLE, BCLK_STILL };
#define BEAT_ACTORS 8
void beat_set_actor_clock(int actor, int mode);

/* ---- S1 additions (add-only) ----
 * Scripts must be sorted by (anchor, at): beat_script_valid() checks it
 * (and anchors < BEAT_LABEL_MAX, ops < BOP_COUNT); the .dsc compiler and
 * test_beat.c call it. beat_now(): frames since beat_start. A hit-stop
 * (BOP_HITSTOP) freezes the actor clocks only, never the script clock,
 * the camera effects or audio (CT sec.3.15). beat_debug_work(): events
 * and cursors examined since beat_start (the O(events fired) cost test). */
int beat_script_valid(const BeatScript *s);
int beat_now(void);
uint32_t beat_debug_work(void);

#endif
