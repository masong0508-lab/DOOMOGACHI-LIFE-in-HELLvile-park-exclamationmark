/* beat.c - the frame-exact beat scheduler (beat.h; owner: S1,
 * docs/design/story/SPEC.md sec.7 S1, C6; comic_timing.md sec.7.2;
 * story_final.md sec.15.3).
 *
 * A BeatScript is a ROM array of 8-byte events. Each event hangs off an
 * anchor label and fires `at` frames after that label fired, so re-timing
 * one beat drags its sound, shake and captions along (no FR(n) constants).
 * Labels fire on a schedule (BOP_LABEL), after an unskippable hold
 * (BOP_HOLD) or on an A press (BOP_WAIT_A): the player's pacing only ever
 * moves anchors, never the gaps inside a punch.
 *
 * Scripts are sorted by (anchor, at) - the .dsc compiler emits them that
 * way and beat_script_valid() checks it - so a label's events are one
 * contiguous run found by binary search. Each fired label gets a cursor
 * into its run; a tick only looks at live cursors, so the cost is
 * O(events fired + live cursors), about 50 cycles on a quiet frame.
 *
 * Resident on the PS1 (SPEC R4), so it never calls overlay code (C6):
 * camera effects, audio hooks, actor clocks and story flags run here; the
 * host-kind events (SAY, CAPTION, POP, EXPR, ACT, IRIS, CUT, INSERT,
 * TWITCH, NPC, CAM, TINT, SHOT, HEADTURN) are handed back with BT_EXT and
 * the calling scene handles them in its own code, then calls beat_tick()
 * again in the same frame.
 *
 * RAM: 132 B of .bss on the consoles (the host adds a 4 B work counter).
 */
#include "doomo/doomo.h"
#include "doomo/beat.h"
#include "doomo/camfx.h"
#include "doomo/audio.h"

#define NEVER     0xFFFFu
#define CURSORS   8         /* labels whose events are still pending */
#define TIMERS    4         /* pending BOP_HOLD labels               */
#define HITSTOP_MAX 8       /* CT sec.3.15: never above 8 frames      */

#define DEAD      0xFFu     /* a free cursor / hold slot              */

/* The work counter only exists on the host (the cost test); the consoles
 * keep the .bss inside the SPEC's ledger (C5: S1 208 B on the PS1). */
#if defined(DOOMO_PLATFORM_GBA) || defined(DOOMO_PLATFORM_PS1)
#define WORK() ((void)0)
#else
#define BEAT_WORK 1
#define WORK() (bt.work++)
#endif

static struct {
    const BeatEv *ev;
    uint16_t n, now, ext_idx;
    uint16_t lab[BEAT_LABEL_MAX];       /* frame each label fired, NEVER = not yet */
    uint16_t cur_idx[CURSORS];          /* next event of each live label's run     */
    uint16_t hold_due[TIMERS];          /* pending BOP_HOLD: when its label fires  */
    uint16_t clk[BEAT_ACTORS];
    uint16_t wa_ok;                     /* WAIT_A: first frame A is accepted       */
    uint16_t wa_due;                    /* WAIT_A: fires by itself then (NEVER)    */
    uint16_t mode;                      /* BCLK_* per actor, 2 bits each           */
    uint8_t  cur_lab[CURSORS];          /* label id, DEAD = free                   */
    uint8_t  hold_lab[TIMERS];          /* label id, DEAD = free                   */
    uint8_t  wa_label, hitstop;
    uint8_t  flags;                     /* F_* below                               */
#ifdef BEAT_WORK
    uint32_t work;                      /* events examined (cost test)             */
#endif
} bt;

enum { F_RUNNING = 1, F_IN_FRAME = 2, F_STARTED = 4, F_WA_LIVE = 8, F_A_LATCH = 16 };
#define HAS(f)   ((bt.flags & (f)) != 0)
#define SET(f)   (bt.flags = (uint8_t)(bt.flags | (f)))
#define CLR(f)   (bt.flags = (uint8_t)(bt.flags & ~(f)))
#define MODE(i)  ((bt.mode >> ((i) * 2)) & 3)

int beat_script_valid(const BeatScript *s)
{
    uint16_t i;
    if (!s || !s->ev) return 0;
    for (i = 0; i < s->n; i++) {
        if (s->ev[i].anchor >= BEAT_LABEL_MAX || s->ev[i].op >= BOP_COUNT) return 0;
        if (i && (s->ev[i].anchor < s->ev[i - 1].anchor ||
                  (s->ev[i].anchor == s->ev[i - 1].anchor && s->ev[i].at < s->ev[i - 1].at)))
            return 0;
    }
    return 1;
}

/* first event of label l's run (binary search on the sorted anchors) */
static uint16_t run_start(int l)
{
    uint16_t lo = 0, hi = bt.n;
    while (lo < hi) {
        uint16_t mid = (uint16_t)((lo + hi) >> 1);
        WORK();
        if (bt.ev[mid].anchor < l) lo = (uint16_t)(mid + 1);
        else hi = mid;
    }
    return lo;
}

static void label_fire(int l)
{
    int i, free_slot = -1;
    uint16_t at;
    if (l < 0 || l >= BEAT_LABEL_MAX) return;
    bt.lab[l] = bt.now;
    at = run_start(l);
    for (i = 0; i < CURSORS; i++) {
        if (bt.cur_lab[i] == l) { bt.cur_idx[i] = at; return; }   /* re-fire */
        if (bt.cur_lab[i] == DEAD && free_slot < 0) free_slot = i;
    }
    if (at >= bt.n || bt.ev[at].anchor != l) return;          /* no events hang off it */
    if (free_slot < 0) { plat_log("beat: too many live labels"); return; }
    bt.cur_idx[free_slot] = at;
    bt.cur_lab[free_slot] = (uint8_t)l;
}

void beat_start(const BeatScript *s)
{
    int i;
    memset(&bt, 0, sizeof(bt));
    for (i = 0; i < BEAT_LABEL_MAX; i++) bt.lab[i] = NEVER;
    memset(bt.cur_lab, DEAD, sizeof(bt.cur_lab));
    memset(bt.hold_lab, DEAD, sizeof(bt.hold_lab));
    if (!s || !s->ev || !s->n) return;
    if (!beat_script_valid(s)) plat_log("beat: script not sorted by (anchor, at)");
    bt.ev = s->ev;
    bt.n = s->n;
    SET(F_RUNNING);
    label_fire(0);                                            /* label 0 = script start */
}

int beat_running(void) { return HAS(F_RUNNING); }
int beat_now(void) { return bt.now; }
#ifdef BEAT_WORK
uint32_t beat_debug_work(void) { return bt.work; }
#else
uint32_t beat_debug_work(void) { return 0; }
#endif

int beat_age(int label)
{
    if (label < 0 || label >= BEAT_LABEL_MAX || bt.lab[label] == NEVER) return -1;
    return (uint16_t)(bt.now - bt.lab[label]);
}

static void set_mode(int actor, int mode)
{
    int i;
    for (i = 0; i < BEAT_ACTORS; i++)
        if (actor < 0 || actor == i)
            bt.mode = (uint16_t)((bt.mode & ~(3u << (i * 2))) | ((unsigned)(mode & 3) << (i * 2)));
}

void beat_set_actor_clock(int actor, int mode) { set_mode(actor, mode); }

int beat_clock(int actor)
{
    if (actor < 0 || actor >= BEAT_ACTORS) return bt.now;
    return MODE(actor) == BCLK_STILL ? 0 : bt.clk[actor];
}

static void advance_clocks(void)
{
    int i;
    if (bt.hitstop) { bt.hitstop--; return; }                 /* hit-stop freezes actors only */
    for (i = 0; i < BEAT_ACTORS; i++) {
        if (MODE(i) == BCLK_NORMAL) bt.clk[i]++;
        else if (MODE(i) == BCLK_DOUBLE) bt.clk[i] = (uint16_t)(bt.clk[i] + 2);
    }
}

static int holding(void)
{
    int i;
    for (i = 0; i < TIMERS; i++) if (bt.hold_lab[i] != DEAD) return 1;
    return 0;
}

void beat_press_a(void) { if (HAS(F_RUNNING)) SET(F_A_LATCH); }

static void set_flag(int f, int on)
{
    if (f < 0 || f >= DOOMO_STORY_FLAGS) return;
    if (on) doomo_town.story.flags[f >> 3] |= (uint8_t)(1 << (f & 7));
    else    doomo_town.story.flags[f >> 3] &= (uint8_t)~(1 << (f & 7));
}

/* 1 = handled here, 0 = a host event (BT_EXT), -1 = END */
static int exec(const BeatEv *e)
{
    int i;
    switch (e->op) {
    case BOP_END: return -1;
    case BOP_LABEL: label_fire(e->a); return 1;
    case BOP_WAIT_A: {
        unsigned b = (uint16_t)e->b, au = (b >> 7) & 511u;
        bt.wa_label = (uint8_t)e->a;
        bt.wa_ok = (uint16_t)(bt.now + (b & 127u));
        bt.wa_due = au ? (uint16_t)(bt.now + au) : NEVER;
        SET(F_WA_LIVE);
        return 1; }
    case BOP_HOLD:
        for (i = 0; i < TIMERS; i++)
            if (bt.hold_lab[i] == DEAD) {
                bt.hold_lab[i] = (uint8_t)e->a;
                bt.hold_due[i] = (uint16_t)(bt.now + (e->b > 0 ? e->b : 0));
                return 1;
            }
        plat_log("beat: too many holds");
        label_fire(e->a);
        return 1;
    case BOP_FREEZE: set_mode(e->a, BCLK_FROZEN); return 1;
    case BOP_THAW:   set_mode(e->a, BCLK_NORMAL); return 1;
    case BOP_PANIC:  set_mode(e->a, BCLK_DOUBLE); return 1;
    case BOP_STILL:  set_mode(e->a, BCLK_STILL); return 1;
    case BOP_HITSTOP: bt.hitstop = (uint8_t)DOOMO_CLAMP(e->a, 0, HITSTOP_MAX); return 1;
    case BOP_SHAKE: camfx_shake(e->a); return 1;
    case BOP_PUNCH: camfx_punch(e->a, e->b); return 1;
    case BOP_ZOOM:  camfx_zoom_snap(e->a, e->b); return 1;
    case BOP_FLASH: camfx_flash((uint8_t)e->a, e->b & 0xFF, (e->b >> 8) & 0xFF); return 1;
    case BOP_FADE:  camfx_fade_to(e->b & 0xFF, (e->b >> 8) & 0xFF); return 1;
    case BOP_PAL_LERP: camfx_pal_lerp_to(e->a, e->b & 0xFF, (e->b >> 8) & 0xFF); return 1;
    case BOP_SFX:
        if (e->a >= SFX_SYNTH_FIRST) audio_sfx_synth(e->a);
        else audio_sfx(e->a);
        return 1;
    case BOP_MUSIC: audio_music(e->a); return 1;
    case BOP_MUSIC_MUTE: audio_music_mute(e->a); return 1;
    case BOP_MUSIC_DUCK: audio_music_duck(e->b, e->a); return 1;   /* a frames, b gain Q8 */
    case BOP_TAPESTOP: audio_tapestop(e->a); return 1;
    case BOP_FLAG: set_flag(e->a, e->b != 0); return 1;
    case BOP_NOBUTTON: return 1;                              /* a marker for the lint */
    case BOP_INSERT:
        if (e->b > 0) audio_music_mute(e->b);                 /* whiplash: mutes itself */
        return 0;
    default: return 0;                                        /* host kinds */
    }
}

/* the frame's start: clocks, due holds, an accepted A press */
static void frame_begin(void)
{
    int i;
    if (HAS(F_STARTED)) { bt.now++; advance_clocks(); }  /* frame 0: every clock is 0 */
    SET(F_STARTED);
    for (i = 0; i < TIMERS; i++)
        if (bt.hold_lab[i] != DEAD && (int16_t)(bt.now - bt.hold_due[i]) >= 0) {
            int l = bt.hold_lab[i];
            bt.hold_lab[i] = DEAD;
            label_fire(l);
        }
    if (HAS(F_A_LATCH)) {
        CLR(F_A_LATCH);
        if (HAS(F_WA_LIVE) && !holding() && (int16_t)(bt.now - bt.wa_ok) >= 0) {
            CLR(F_WA_LIVE);
            label_fire(bt.wa_label);
        }
    }
    if (HAS(F_WA_LIVE) && bt.wa_due != NEVER && (int16_t)(bt.now - bt.wa_due) >= 0) {
        CLR(F_WA_LIVE);                                   /* nobody pressed: on we go */
        label_fire(bt.wa_label);
    }
}

int beat_tick(void)
{
    int i, progress;
    if (!HAS(F_RUNNING)) { advance_clocks(); bt.now++; return BT_DONE; }
    if (!HAS(F_IN_FRAME)) { frame_begin(); SET(F_IN_FRAME); }
    do {
        progress = 0;
        for (i = 0; i < CURSORS; i++) {
            while (bt.cur_lab[i] != DEAD) {
                const BeatEv *e;
                int l = bt.cur_lab[i];
                uint16_t idx = bt.cur_idx[i];
                WORK();
                if (idx >= bt.n || bt.ev[idx].anchor != l) { bt.cur_lab[i] = DEAD; break; }
                e = &bt.ev[idx];
                if ((uint32_t)bt.lab[l] + e->at > bt.now) break;
                bt.cur_idx[i] = (uint16_t)(idx + 1);
                progress = 1;
                switch (exec(e)) {
                case -1:
                    CLR(F_RUNNING | F_IN_FRAME);
                    return BT_DONE;
                case 0:
                    bt.ext_idx = (uint16_t)(e - bt.ev);
                    return BT_EXT;                            /* resume here next call */
                default: break;
                }
            }
        }
    } while (progress);
    CLR(F_IN_FRAME);
    return BT_RUN;
}

const BeatEv *beat_ext_event(void)
{
    return bt.ev && bt.ext_idx < bt.n && HAS(F_IN_FRAME) ? &bt.ev[bt.ext_idx] : NULL;
}

/* has event i fired already? */
static int fired(uint16_t i)
{
    int k, l = bt.ev[i].anchor;
    if (bt.lab[l] == NEVER) return 0;
    for (k = 0; k < CURSORS; k++)
        if (bt.cur_lab[k] == l) return i < bt.cur_idx[k];
    return 1;
}

/* START: jump to the end. Only the persistent state (story flags) is
 * applied; sounds, shakes and host events are dropped. */
void beat_skip(void)
{
    uint16_t i;
    if (!HAS(F_RUNNING)) return;
    for (i = 0; i < bt.n; i++)
        if (bt.ev[i].op == BOP_FLAG && !fired(i)) set_flag(bt.ev[i].a, bt.ev[i].b != 0);
    CLR(F_RUNNING | F_IN_FRAME | F_WA_LIVE);
    memset(bt.hold_lab, DEAD, sizeof(bt.hold_lab));
    memset(bt.cur_lab, DEAD, sizeof(bt.cur_lab));
}
