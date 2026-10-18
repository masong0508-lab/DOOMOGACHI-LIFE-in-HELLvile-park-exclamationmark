/* story_feast.c - CUT_FEAST_POP: the "What an Ass" outcome of the post-game
 * Feast of Rebuilding (owner: story). Called by cutscene.c (SCENE_CUTSCENE):
 *   feast_tick(panel, pt)        sound effects, once per tick
 *   feast_draw(panel, pt, ct)    the picture (cutscene.c draws the caption)
 *
 * The pictures follow the storyboard (docs/design/story/concept/
 * gg_storyboard.png, gg_closeup.png): Grumbleguts is the concept-sheet
 * figure (grumbleguts_gfx.c), staged in the festival hall with its lantern
 * bunting, the bagel table and a crowd of real residents. Slapstick,
 * cartoon-modest: cheeks only, a cartoon X pucker at most, the hose goes
 * into the crease and the rest is a squeak and his face. CUTE gore
 * setting: confetti, plush stuffing, hearts, a halo. BRUTAL: a red shower.
 * Panels (cutscene.c; every panel's length is timed exactly):
 *   0 the feast (butt to camera)   1 every eye on him   2 "Look who's full
 *   of himself, now?" (record scratch, eyebrow wiggle)   3 the hose: a wide
 *   shot, the REVERSE ROPE CLIMB rear camera (push_shot: hands on, hose in
 *   at the cleft, "surely that's enough?"), the REACTION CUT (face_shot:
 *   eyes bulge one at a time, blush), the yank (rear_shot: SHLORP, the
 *   board's CENSOR STICKER), then the SLAP with its hand print   4 burp
 *   (false pop 1, a gross-out close-up)   5 fart (false pop 2, the front
 *   shot)   6 both at once   7 the valve wins: the X-RAY CUTAWAY, the camera
 *   angles, the belly close-up, from behind the crowd   8 POP (cute:
 *   confetti / stuffing / hearts / halo, lone cheeks drifting with a
 *   raspberry; brutal: red shower, the belly lands last)   9 "...that's hot."
 *   and the mob   10 the candlelit afterglow   11 the slow look into the
 *   lens   12-13 the bike pump   14-19 the long night (the leak, the burp
 *   and poke, the little black book, the queue, the pump passed down the
 *   line, dawn), with its rump-cam / profile cut-ins   20 title.
 * Comic timing: holds, smash cuts (white flash, record scratch), squash and
 * stretch on the belly, a stateless damped-spring jiggle (sp_*) so every
 * frame is a pure function of the panel tick on every port.
 */
#include "story_int.h"
#include "grumbleguts_gfx.h"

#define F_CUTE  (opt_get(OPT_GORE) != GORE_BRUTAL)
#define F_CULPRIT ID_HATTIE          /* the possessed guest (comedian)   */

static const int16_t crowd_ids[6] = { ID_MAYOR, ID_NANA, ID_FUZZ, ID_BLOOM, ID_MOTHER_NOX, ID_ANVIL };

/* ---- the wide shot's geometry: everything composes above the caption ---------- */
typedef struct Stage { int sw, sh, s, cap, yf, floor, cx; } Stage;

static void stage(Stage *g)
{
    g->sw = plat_screen_w();
    g->sh = plat_screen_h();
    g->s = g->sh >= 200 ? 2 : 1;
    g->cap = cutscene_caption_y();
    g->yf = DOOMO_MIN(g->cap + 2 * g->s, g->sh - 8 * g->s);
    g->floor = g->yf - 12 * g->s;
    g->cx = g->sw / 2 - 12 * g->s;
}

/* how swollen he is, per panel and tick (0..512, 256 = the concept's FULL) */
static int swell_at(int panel, uint32_t pt)
{
    int t = (int)pt, v;
    switch (panel) {
    case 0: return t * 6 / 25;
    case 1: return 36 + t * 6 / 20;
    case 2: case 3: return 84;
    case 4: v = t < 30 ? t * 12 / 30 : 12 - (t - 30) / 10; return 84 + DOOMO_MAX(v, 0) * 6;
    case 5: v = t < 40 ? t * 22 / 40 : 22 - (t - 40) / 6; return 96 + DOOMO_MAX(v, 0) * 6;
    case 6: if (t < 50) return 84 + t * 180 / 50;                /* the big one: swells ...   */
            return 264 - DOOMO_MIN((t - 50) * 24, 240);          /* ... and PFFT, deflates    */
    case 7: return 36 + t * 480 / 150;
    default: return 0;
    }
}

/* ---- the long night's spring physics ----------------------------------------------
 * Jiggle is a stateless closed-form damped spring, so every frame is a pure function of
 * the panel tick (no per-frame state, replays and rewinds identically on every port):
 *     offset(t) = sum over the last few kicks of  amp * e^(-dt/tau) * cos(2*pi*dt/period)
 * Kicks land on a per-panel cadence (a slap, a pump stroke, a hiss pulse); belly, cheeks,
 * chest and tail ring on their own periods and lag the kick by a few ticks, so the body
 * ripples instead of bobbing. Integer maths only. */
typedef struct { uint8_t panel; uint16_t a, b; uint8_t view; } GgCut;
static const GgCut gg_cuts[] = {
    { 14,  30,  84, GGV_BACK }, { 14, 118, 160, GGV_SIDE }, { 14, 176, 210, GGV_BACK_LOOK },
    { 15,  22,  70, GGV_SIDE }, { 15, 104, 168, GGV_BACK },
    { 16,  34,  92, GGV_BACK }, { 16, 146, 202, GGV_BACK_LOOK },
    { 17,  22,  84, GGV_BACK }, { 17, 118, 160, GGV_SIDE }, { 17, 172, 236, GGV_BACK_LOOK },
    { 18,  18,  78, GGV_BACK }, { 18, 108, 150, GGV_SIDE }, { 18, 160, 236, GGV_BACK },
    { 19,  14,  76, GGV_BACK_LOOK }
};

static const GgCut *cut_at(int panel, uint32_t pt)
{
    unsigned i;
    if (panel < 14 || panel > 19) return 0;
    for (i = 0; i < sizeof gg_cuts / sizeof gg_cuts[0]; i++)
        if (gg_cuts[i].panel == panel && pt >= gg_cuts[i].a && pt < gg_cuts[i].b) return &gg_cuts[i];
    return 0;
}

/* ticks between kicks: the panels get quicker as the night goes on */
static int sp_every(int panel)
{
    static const uint8_t ev[6] = { 16, 14, 18, 12, 10, 28 };
    return (panel >= 14 && panel <= 19) ? ev[panel - 14] : 16;
}

/* one kick, dt ticks after it landed */
static int sp_ring(int dt, int amp, int period, int tau)
{
    int k, d = 4096, f;
    if (dt < 0 || dt > tau * 4 || amp == 0) return 0;
    f = 4096 - 4096 / tau;                                   /* e^(-1/tau) in 12 bits, one division */
    for (k = 0; k < dt; k++) d = (d * f) >> 12;
    return (ui_wave(dt + period / 4, period, amp) * d) >> 12;
}

/* a train of kicks every `every` ticks starting at `phase`; the last three still ring.
 * parity 0/1 = the kicks that land on this side hit full strength, the others only
 * ring a quarter (a slap on one cheek barely shakes the other); -1 = all full. */
static int sp_spring(int t, int every, int phase, int amp, int period, int tau, int parity)
{
    int n, j, sum = 0, kt = t - phase;
    if (kt < 0 || every <= 0) return 0;
    n = kt / every;
    for (j = 0; j < 3 && n - j >= 0; j++) {
        int idx = n - j, a = amp * (6 + (idx * 5) % 5) / 8;           /* every kick a little different */
        if (parity >= 0 && (idx & 1) != parity) a /= 4;
        sum += sp_ring(kt - idx * every, a, period, tau);
    }
    return sum;
}

/* the springs of the figure for this tick: every = kick cadence, phase = first kick,
 * amp = strength in px. cut = 0: a front shot, both cheeks rock together a beat apart */
static void sp_motion(GgParams *m, int every, int phase, int amp, int t, uint32_t ct, int s, int cut)
{
    int k = (int)ct;
    m->bob     = sp_spring(t, every, phase, amp / 2 + 1, 12, 10, -1) + ui_wave(k * 3, 24, 1);
    m->chest   = sp_spring(t, every, phase + 1, amp / 2 + 1, 14, 9, -1);
    m->tail    = sp_spring(t, every, phase + 3, amp * 3, 18, 14, -1) + ui_wave(k * 5, 16, s * 2);
    m->flap    = ui_wave(k * 4, 14, 3 * s) + sp_spring(t, every, phase, amp * 2, 10, 8, -1);
    m->belly_x = sp_spring(t, every, phase + 2, amp, 16, 14, -1);
    m->belly_y = sp_spring(t, every, phase + 3, amp * 3 / 2, 14, 12, -1);
    m->lx = sp_spring(t, every, phase + 1, amp, 12, 8, 0);
    m->ly = sp_spring(t, every, phase + 2, amp * 2, 16, 10, 0);
    m->rx = sp_spring(t, every, phase + 1, amp, 12, 8, 1);
    m->ry = sp_spring(t, every, phase + 2, amp * 2, 16, 10, 1);
    if (!cut) {
        m->ly = sp_spring(t, every, 0, amp * 2, 15, 10, -1);
        m->ry = sp_spring(t, every, 2, amp * 2, 17, 10, -1);
        m->lx = m->ly / 3;
        m->rx = -m->ry / 3;
    }
}

/* the idle: breathing, a wing beat, the tail swaying */
static void sp_idle(GgParams *m, uint32_t ct, int s)
{
    int k = (int)ct;
    m->blink = (ct % 97u) < 4u;
    m->bob = ui_wave(k * 3, 24, 1);
    m->flap = ui_wave(k * 4, 14, 3 * s);
    m->tail = ui_wave(k * 5, 16, 2 * s);
    m->belly_y = ui_wave(k * 3, 24, s);
    m->chest = ui_wave(k * 3 + 4, 24, 1);
}

/* ---- gross-out close-ups: smash cut + hold + record scratch ------------------ */
#define CU_BURP_A 30u
#define CU_BURP_B 64u      /* panel 4 */
#define CU_GAS_A  62u
#define CU_GAS_B  94u      /* panel 5 */
#define CU_BELLY_A 90u
#define CU_BELLY_B 126u    /* panel 7 */
#define CU_PUSH_A  40u
#define CU_PUSH_B  112u    /* panel 3: rear camera, the hose pushed further and further in */
#define CU_FACE_A  112u
#define CU_FACE_B  138u    /* panel 3: the reaction cut */
#define CU_REAR_A  138u
#define CU_REAR_B  188u    /* panel 3: the rear shot, the yank */
#define CU_XRAY_B  20u     /* panel 7: the cutaway first */

/* ---- camera angle shots on the swelling belly --------------------------------------- */
enum { AN_LOW, AN_PUSH, AN_TOP, AN_BEHIND, AN_REACT, AN_FRONT };
typedef struct { uint8_t panel, a, b, kind; const char *name; } AngleShot;
static const AngleShot shots[] = {
    { 5, 40,  62, AN_FRONT,  "CAM 6  FRONT" },
    { 6, 50,  70, AN_FRONT,  "CAM 6  FRONT" },
    { 6, 70, 104, AN_REACT,  "CAM 3  REACTION" },
    { 7, 20,  44, AN_LOW,    "CAM 2  LOW ANGLE" },
    { 7, 44,  68, AN_PUSH,   "CAM 1  PUSH IN" },
    { 7, 68,  90, AN_TOP,    "CAM 4  OVERHEAD" },
    { 7, 126, 150, AN_BEHIND, "CAM 5  BEHIND THE CROWD" }
};
#define NSHOTS ((int)(sizeof shots / sizeof shots[0]))

static int angle_start(int panel, uint32_t pt)     /* a cut happens this tick */
{
    int i;
    for (i = 0; i < NSHOTS; i++)
        if (shots[i].panel == panel && (pt == shots[i].a || pt == shots[i].b)) return 1;
    return 0;
}

/* 1 while a full-screen shot is up: cutscene.c hides the caption */
int feast_front_shot(int panel, uint32_t pt)
{
    int i;
    if (cut_at(panel, pt)) return 1;
    if (panel == 3 && pt >= CU_PUSH_A && pt < CU_REAR_B) return 1;
    if (panel == 4 && pt >= CU_BURP_A && pt < CU_BURP_B) return 1;
    if (panel == 5 && pt >= CU_GAS_A && pt < CU_GAS_B) return 1;
    if (panel == 7 && pt >= CU_BELLY_A && pt < CU_BELLY_B) return 1;
    if (panel == 7 && pt < CU_XRAY_B) return 1;
    if (panel == 11) return pt < 130;
    for (i = 0; i < NSHOTS; i++)
        if (shots[i].panel == panel && pt >= shots[i].a && pt < shots[i].b) return 1;
    return 0;
}

void feast_tick(int panel, uint32_t pt)
{
    if (angle_start(panel, pt)) audio_sfx(SFX_BLIP);                  /* a cut */
    {   const GgCut *gc = cut_at(panel, pt);                          /* the night's cut-ins */
        if (gc && pt == gc->a) audio_sfx(SFX_BLIP);
        if (gc && panel != 14 && pt >= (uint32_t)gc->a + 4 && ((pt - gc->a - 4) % (uint32_t)sp_every(panel)) == 0) {
            audio_sfx(SFX_SPLAT);                                      /* the smack... */
            audio_sfx(SFX_SQUEAK);                                     /* ...and the wobble that follows */
        }
        if (gc && panel == 14 && ((pt - gc->a) % (uint32_t)sp_every(panel)) == 6) audio_sfx(SFX_POOP);   /* the pulses */
    }
    switch (panel) {
    case 0: if ((pt % 24) == 6) audio_sfx(SFX_EAT);
            if (pt == 50 || pt == 110) audio_sfx(SFX_SPARKLE);         /* the wink       */
            break;
    case 1: if (pt == 40) audio_sfx(SFX_LOVE); break;                  /* wolf whistle   */
    case 2: if (pt == 40) audio_sfx(SFX_ERROR); break;                 /* record scratch */
    case 3:
        if (pt == 20) audio_sfx(SFX_SQUEAK);
        if (pt == CU_PUSH_A) audio_sfx(SFX_BLIP);                       /* cut to the rear camera */
        if (pt >= CU_PUSH_A && pt < CU_PUSH_B) {                        /* one squeak per shove, the music drops to a pluck */
            uint32_t pt0 = pt - CU_PUSH_A;
            if ((pt0 < 24u && (pt0 % 6u) == 0) || (pt0 >= 36u && ((pt0 - 36u) % 4u) == 0)) audio_sfx(SFX_SQUEAK);
            if (pt0 == 36u) audio_sfx(SFX_BLIP);                        /* "...nope." */
        }
        if (pt == CU_FACE_A) audio_sfx(SFX_ERROR);                      /* record scratch */
        if (pt == CU_REAR_A) audio_sfx(SFX_BLIP);
        if (pt == CU_REAR_A + 12 || pt == CU_REAR_A + 28 || pt == CU_REAR_A + 34) audio_sfx(SFX_SQUEAK);   /* the heaves */
        if (pt == CU_REAR_A + 40) { audio_sfx(SFX_POOP); audio_sfx(SFX_SPLASH); }                         /* SHLORP */
        if (pt == CU_REAR_B + 4) audio_sfx(SFX_SPLAT);                  /* the SLAP       */
        break;
    case 4: if (pt == CU_BURP_A) { audio_sfx(SFX_ERROR); audio_sfx(SFX_POOP); } break;   /* burp           */
    case 5: if (pt == 40) audio_sfx(SFX_POOP);
            if (pt == CU_GAS_A) audio_sfx(SFX_ERROR);
            break;
    case 6: if (pt == 50) { audio_sfx(SFX_POOP); audio_sfx(SFX_SPLASH); } break;
    case 7: if ((pt % 30) == 0) audio_sfx(SFX_SQUEAK);
            if (pt == CU_BELLY_A) audio_sfx(SFX_ERROR);
            if (pt > CU_BELLY_A && pt < CU_BELLY_B && (pt % 6) == 0) audio_sfx(SFX_SQUEAK);
            if (pt == CU_BELLY_A + 26) audio_sfx(SFX_PICKUP);          /* PING           */
            break;
    case 8: if (pt == 4) { audio_sfx(SFX_EXPLODE); audio_sfx(F_CUTE ? SFX_CONFETTI : SFX_SPLAT); }
            if (pt == 14 || pt == 34 || pt == 54 || pt == 74) audio_sfx(SFX_POOP);     /* balloon flight */
            if (pt == 24 || pt == 44 || pt == 64) audio_sfx(SFX_SQUEAK);
            if (pt == 90) audio_sfx(SFX_SPLAT);                        /* flop           */
            if (pt == 100) audio_sfx(SFX_EAT);                         /* last gurgle    */
            break;
    case 9: if (pt == 24) audio_sfx(SFX_ERROR); if (pt == 30) audio_sfx(SFX_SPLAT); break;
    case 10: if (pt == 20 || pt == 100) audio_sfx(SFX_LOVE);
             if (pt == 60) audio_sfx(SFX_SPARKLE);
             break;
    case 11: if (pt == 30) audio_sfx(SFX_LOVE);
             if (pt == 66) audio_sfx(SFX_SPARKLE);                       /* the slow wink  */
             break;
    case 12: if ((pt % 20) == 10) audio_sfx(SFX_SQUEAK);                  /* a stroke of the pump */
             break;
    case 13: if ((pt % 24) == 12) audio_sfx(SFX_SQUEAK);
             if (pt == 108 || pt == 156) audio_sfx(SFX_ERROR);            /* the skin complains */
             if (pt == 160) audio_sfx(SFX_LOVE);
             break;
    case 14: if ((pt % 24) == 6) audio_sfx(SFX_SQUEAK);                  /* the harness */
             if (pt == 20 || pt == 70 || pt == 118) audio_sfx(SFX_POOP);    /* the long hiss */
             if (pt == 40 || pt == 90) audio_sfx(SFX_BUBBLE);               /* gurgle */
             if (pt == 180) audio_sfx(SFX_SPLAT);                           /* BLORP */
             break;
    case 15: if (pt == 8) audio_sfx(SFX_EAT);                               /* the burp */
             if ((pt % 40) == 4) audio_sfx(SFX_SQUEAK);                     /* poke, squelch */
             if ((pt % 40) == 6) audio_sfx(SFX_SPLASH);
             if (pt == 100 || pt == 170) audio_sfx(SFX_LOVE);
             break;
    case 16: if ((pt % 28) == 10) audio_sfx(SFX_BLIP);                      /* a page turns */
             if (pt == 60 || pt == 130) audio_sfx(SFX_SPARKLE);
             if (pt == 160) audio_sfx(SFX_LOVE);
             break;
    case 17: if ((pt % 70) == 6) audio_sfx(SFX_BELL);                       /* the ticket machine */
             if (pt == 40 || pt == 110 || pt == 180) audio_sfx(SFX_LOVE);
             if ((pt % 35) == 20) audio_sfx(SFX_SQUEAK);                    /* the queue shuffles */
             break;
    case 18: if ((pt % 20) == 10) audio_sfx(SFX_SQUEAK);                    /* a stroke of the pump */
             if ((pt % 60) == 30) audio_sfx(SFX_LOVE);
             if (pt == 150 || pt == 200) audio_sfx(SFX_ERROR);              /* the skin complains */
             break;
    case 19: if (pt == 10) audio_sfx(SFX_SPARKLE);
             if (pt == 70) audio_sfx(SFX_EAT);                              /* a long, satisfied sigh */
             if (pt == 150) audio_sfx(SFX_HAPPY);
             break;
    case 20: if (pt == 4) audio_sfx(SFX_BELL); break;
    default: break;
    }
}

/* ====================================================================== */
/* staging helpers                                                          */

/* the lantern string: ONE lantern is cached (a few hundred bytes) and blitted
 * along the string, swinging */
static void lantern_fn(void *ctx) { gg_lantern(0, 0, *(int *)ctx); }

static void bunting(int y, uint32_t ct)
{
    const DoomoImage *im;
    int ox, oy, r = plat_screen_h() >= 200 ? 5 : 4;
    im = gg_cache_image(0, 0x4C414E00u | (uint32_t)r, lantern_fn, &r, &ox, &oy);   /* slot 0: survives the figure */
    if (im) gg_lantern_string(y, r, ct, im, ox, oy);
    else gg_lanterns(y, ct);
}

/* a resident, feet at (x, y), seen from dir (0 faces the camera, 3 shows the back) */
static void citizen(int id, int x, int y, int s, int dir, int frame, int shadow)
{
    const DoomoImage *b = bodyspr_image(id, dir, frame);
    if (!b) return;
    if (shadow) gg_ellipse(x, y - 1, b->w * s / 3 + 2, 2, PAL_RAMP(RAMP_STONE, 1));
    plat_image(x - b->w * s / 2, y - b->h * s + 2 * s, b, s, 0, 0);
}

static void candles(int x, int y, uint32_t ct)
{
    int i;
    for (i = 0; i < 3; i++) {
        int cx = x + i * 12, h = 8 + (i & 1) * 4, fl = ui_wave((int)ct * 3 + i * 5, 9, 1);
        plat_checker(cx - 6, y - h - 16, 16, 16, PAL_RAMP(RAMP_LAVA, 1));          /* glow */
        plat_rect(cx, y - h, 4, h, PAL_RAMP(RAMP_BONE, 6));
        plat_rect(cx + 3, y - h, 1, h, PAL_RAMP(RAMP_BONE, 4));
        gg_ellipse(cx + 2 + fl, y - h - 3, 2, 3, PAL_UI_GOLD);
        gg_ellipse(cx + 2 + fl, y - h - 2, 1, 1, PAL_WHITE);
    }
}

static void smoke(int x, int y, int rise, uint32_t ct)
{
    int i;
    for (i = 0; i < 6; i++) {
        int yy = y - (int)((ct * 2 + (uint32_t)i * 14) % (uint32_t)rise);
        gg_ellipse(x + ui_wave((int)ct + i * 9, 24, 5 + i), yy, 2 + i / 2, 2 + i / 2, PAL_RAMP(RAMP_STONE, 5 + (i & 1)));
    }
}

/* the hall, the lanterns, the table, the crowd behind him */
static void hall_wide(const Stage *g, int mood, int bagels, int crowd_mood, int crowd_dir, uint32_t ct)
{
    int s = g->s, i;
    gg_hall(g->floor, mood, ct);
    bunting(3, ct);
    gg_table(6, g->floor - 7 * s, 62 * s, bagels, ct);
    if (mood == GG_HALL_NIGHT) candles(14, g->floor - 7 * s, ct);
    for (i = 0; i < 6; i++) {
        int16_t id = crowd_ids[i];
        gg_crowd(&id, 1, g->sw / 2 - 64 * s + i * 26 * s + (i & 1) * 4 * s, g->floor + (i & 1 ? 2 : 6) * s, 0, crowd_dir, s, crowd_mood, ct + (uint32_t)i * 7);
    }
}

/* the FULLNESS meter in the corner (storyboard frame 1) */
static void fullness(const Stage *g, int v)
{
    int x = g->sw - 70 * g->s / 2 - 10, y = 16 + g->s * 4, w = 60 * g->s / 2 + 10;
    ui_round_rect(x - 2, y - 2, w + 4, 11, 3, PAL_RAMP(RAMP_STONE, 1));
    plat_rect(x, y, w, 7, PAL_RAMP(RAMP_BLOOD, 1));
    plat_rect(x, y, DOOMO_CLAMP(v, 0, 512) * w / 512, 7, PAL_UI_RED);
    plat_rect(x, y, DOOMO_CLAMP(v, 0, 512) * w / 512, 1, PAL_RAMP(RAMP_LAVA, 6));
    ui_text_outline(x, y + 9, "FULLNESS", PAL_UI_CREAM, PAL_BLACK);
}

/* hearts drifting up from (x, y) */
static void hearts(int x, int y, int n, uint32_t ct)
{
    int i;
    for (i = 0; i < n; i++)
        ui_heart(x + (i - n / 2) * 14 + ui_wave((int)ct + i * 9, 20, 3), y - (int)((ct * 2 + (uint32_t)i * 17) % 40u), 2 + (i & 1), PAL_UI_PINK);
}

/* a hose from (x0, y0) to (x1, y1): fat, ribbed, minty */
static void hose(int x0, int y0, int x1, int y1, int s, int ribs, uint32_t ct)
{
    int k, i, mx = (x0 + x1) / 2, my = DOOMO_MAX(y0, y1) + 10 * s;
    for (k = -s; k <= s + 1; k++) {
        uint8_t c = k == -s ? PAL_RAMP(RAMP_MINT, 6) : k == s + 1 ? PAL_RAMP(RAMP_MINT, 2) : PAL_RAMP(RAMP_MINT, 4);
        plat_line(x0, y0 + k, mx, my + k, c);
        plat_line(mx, my + k, x1, y1 + k, c);
    }
    for (i = 0; i < ribs; i++) {
        int u = (i * 100 / ribs + (int)(ct * 2) % 100) % 100, px, py;
        if (u < 50) { px = x0 + (mx - x0) * u / 50; py = y0 + (my - y0) * u / 50; }
        else { px = mx + (x1 - mx) * (u - 50) / 50; py = my + (y1 - my) * (u - 50) / 50; }
        plat_rect(px, py - s, 2, 2 * s + 2, PAL_RAMP(RAMP_MINT, 2));
    }
}

/* the brass inflation valve */
static void valve(int x, int y, int s, int open)
{
    plat_rect(x - 3 * s - 1, y - 2 * s - 1, 6 * s + 2, 5 * s + 2, PAL_BLACK);
    plat_rect(x - 3 * s, y - 2 * s, 6 * s, 5 * s, PAL_UI_GOLD);
    plat_rect(x - 3 * s, y - 2 * s, 6 * s, 1, PAL_RAMP(RAMP_CANDY, 6));
    plat_rect(x - s, y - s, 2 * s, 2 * s, open ? PAL_BLACK : PAL_RAMP(RAMP_STEEL, 2));
}

/* a bead of sweat sliding down from (x, y) */
static void sweat(int x, int y, uint32_t ct, int phase)
{
    int d = (int)((ct * 2 + (uint32_t)phase) % 16u);
    gg_ellipse(x, y + d, 2, 3, PAL_RAMP(RAMP_SKY, 7));
    plat_rect(x - 1, y + d - 1, 1, 2, PAL_WHITE);
}

/* ====================================================================== */
/* panel 3's close shots                                                    */

/* the REVERSE ROPE CLIMB rear camera: the culprit works the hose in like a rope
 * climb, hand over hand, one palm braced on a cheek; cheeks only, the hose goes
 * in at the cleft. Three beats (rule of three): four pulls, a pause with a sweaty
 * "surely that's enough?", then it keeps going, faster, past any sensible
 * length. t0 runs 0 .. CU_PUSH_B - CU_PUSH_A - 1. */
static int push_pulls(int t)
{
    return t < 24 ? t / 6 : t < 36 ? 4 : 4 + (t - 36) / 4;
}

static void push_shot(uint32_t t0)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), cx = sw / 2, cy = sh / 2, i, k, t = (int)t0, s = sh >= 200 ? 2 : 1;
    int r = sh * 2 / 5, cyc = cy + 4 * s, pause = t >= 24 && t < 36;
    int len = t < 24 ? 6 : 4, ph = t < 24 ? t % 6 : (t >= 36 ? (t - 36) % 4 : 0), pulls = push_pulls(t);
    int sq = pause ? 0 : DOOMO_MAX(3 - ph, 0) * s, jig = pause ? 0 : ui_wave(t * 7, 8, 1) * s;
    int fx = cx + sw / 4 + 10 * s, fy = sh - 30 * s, off, s0, s1, f0, f1;
    uint8_t sk = PAL_RAMP(RAMP_PEACH, 6);
    const DoomoImage *face;

    gg_hall(sh - 10, GG_HALL_DUSK, t0);
    gg_cheeks_closeup(cx, cyc, r, sq, sq, jig, 1);
    /* one palm braced flat on the near cheek for leverage, creases under it */
    gg_hand(cx - r * 3 / 4 + sq, cyc - r / 4, s, 0, sk);
    for (k = 0; k < 3; k++) plat_line(cx - r - 2 * s + k * 10 * s + sq, cyc + 2 * s, cx - r - 6 * s + k * 12 * s + sq, cyc + 14 * s, PAL_RAMP(RAMP_BLOOD, 3));
    /* the hose from the fists into the crease */
    for (k = 0; k < 4 * s; k++) {
        uint8_t c = k == 0 ? PAL_RAMP(RAMP_MINT, 6) : k == 4 * s - 1 ? PAL_RAMP(RAMP_MINT, 2) : PAL_RAMP(RAMP_MINT, 4);
        plat_line(cx + 1, cyc + 2 * s + k, fx, fy + k, c);
    }
    off = pulls * 8 + ph * 8 / len;
    for (i = 0; i < 6; i++) {                                    /* ribs marching in (u runs 48 -> 0) */
        int u = (i * 8 + 48 - (off % 48)) % 48;
        int px = cx + (fx - cx) * u / 48, py = cyc + 2 * s + (fy - cyc - 2 * s) * u / 48;
        plat_rect(px, py - s, 2 * s, 4 * s + 2, PAL_RAMP(RAMP_MINT, 2));
    }
    gg_ellipse(cx + 1, cyc + 2 * s, 2 * s, 3 * s, PAL_RAMP(RAMP_MINT, 2));          /* and in it goes */
    plat_rect(cx, cyc - r * 2 / 3, 2, r / 2, GG_LINE);
    for (i = -3; i <= 3; i++) plat_rect(cx + (i < 0 ? -1 : 1), cyc + 2 * s + i * s, 1, 1, GG_LINE);   /* the crease pinches round it */
    /* hand over hand: each fist slides 85% -> 45% of the way in, then snaps back */
    s0 = pause ? 0 : ph * 100 / len;
    s1 = pause ? 50 : ((ph + len / 2) % len) * 100 / len;
    f0 = 85 - 40 * s0 / 100;
    f1 = 85 - 40 * s1 / 100;
    for (k = 0; k < 2; k++) {
        int f = k ? f1 : f0;
        int hx = cx + (fx - cx) * f / 100, hy = cyc + 2 * s + (fy - cyc - 2 * s) * f / 100 + (f > 50 ? 3 * s : 0);
        for (i = 0; i < 3 * s; i++) plat_line(hx + 4 * s, hy - s + i, sw, sh + 6 * s + i, i == 3 * s - 1 ? PAL_RAMP(RAMP_PEACH, 3) : sk);   /* the forearm */
        gg_hand(hx, hy, s, -1, sk);
    }
    /* the culprit's face in the corner: bracing, tongue out */
    face = portrait_image(F_CULPRIT, EXPR_ANGRY);
    if (face) plat_image(sw - face->w * s - 2, sh - face->h * s + 8 * s + (pause ? 0 : sq), face, s, IMG_SHADOW, 0);
    sweat(sw - 50 * s, sh - 40 * s, t0, 0);
    /* the coil on the ground shrinks as the hose goes in */
    for (i = 0; i < DOOMO_MAX(6 - pulls / 2, 0); i++)
        gg_ellipse(sw - 40 * s - 20, sh - 6 - i * 3 * s, 16 * s, 4 * s, PAL_RAMP(RAMP_MINT, 3 + (i & 1)));
    /* effort: motion lines, a squeak, a foot counter */
    for (i = 0; i < 3; i++) {
        plat_line(10, 50 + i * 6 * s, 26, 46 + i * 6 * s, PAL_UI_CREAM);
        plat_line(sw - 10, 50 + i * 6 * s, sw - 26, 46 + i * 6 * s, PAL_UI_CREAM);
    }
    {
        char buf[16];
        int ft = pulls * 6 + (pause ? 0 : ph * 6 / len);
        doomo_snprintf(buf, sizeof buf, "IN: %02d FT", ft);
        ui_text_outline(sw - 66, 22, buf, t >= 36 ? PAL_UI_GOLD : PAL_WHITE, PAL_BLACK);
    }
    ui_text_outline(sw - 84, 6, "CAM 7  REAR", PAL_WHITE, PAL_BLACK);
    if (t < 24 && ph < 3) gg_shout(cx - 60, sh - 14, "hnnng", PAL_WHITE);
    if (pause) { gg_shout(cx - 54, sh - 14, "surely that's enough?", PAL_UI_GOLD); gg_shout(cx - 14, 8, "phew", PAL_WHITE); }
    if (t >= 36 && ph < 2) gg_shout(cx - 60, sh - 14, "...nope.", PAL_UI_GOLD);
    if (!pause && ph == 0 && t > 0) gg_shout(cx - 76 + (pulls % 3) * 24, 40, "*squeak*", PAL_UI_GOLD);
    if (t >= 44) for (k = 0; k < 3; k++) {                       /* strain lines off the cheeks */
        plat_line(cx - r * 3 / 2 - 6 - k * 4, cyc - 8 + k * 8, cx - r * 3 / 2 - 20 - k * 5, cyc - 14 + k * 10, PAL_WHITE);
        plat_line(cx + r * 3 / 2 + 6 + k * 4, cyc - 8 + k * 8, cx + r * 3 / 2 + 20 + k * 5, cyc - 14 + k * 10, PAL_WHITE);
    }
    ui_text_outline(8, 22, "\"she'll hold.\"", PAL_UI_CREAM, PAL_BLACK);
}

/* the REACTION CUT: his face fills the frame as it goes in: eyes bulge one at a
 * time, a blush, a tiny "o" of a mouth. Then he looks dead into the lens. */
static void face_shot(uint32_t t0)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), s = sh >= 200 ? 4 : 3, t = (int)t0, i;
    GgParams p;
    gg_params_init(&p);
    p.layers = GGL_HEAD | GGL_FACE;
    p.seams = 1;
    gg_hall(sh - 10, GG_HALL_DUSK, t0);
    bunting(3, t0);
    gg_draw(sw / 2, sh / 2 + 60 * s + 6 * s, s, GGV_FRONT, GGP_STAND, t < 14 ? GGE_BLUSH : GGE_DEADPAN, &p);
    for (i = 0; i < 2; i++) sweat(sw / 2 + 12 * s + i * 6, sh / 2 - 10 * s, t0, i * 7);
    ui_text_outline(8, sh - 14, t < 14 ? "*gurgle?*" : "ahem.", PAL_UI_CREAM, PAL_BLACK);
    if (t >= 8) ui_text_outline(sw - 50, sh - 14, "...oh.", PAL_UI_CREAM, PAL_BLACK);
    if (t >= 18) gg_shout(sw / 2 - 36, 8, "one sec, hon.", PAL_UI_GOLD);
}

/* the yank: straight-on rear shot (cheeks only), the hose hauled on in three
 * heaves until it comes off: SHLORP. The board's censor sticker slaps on for
 * the last beat, a corner peeling as the cheek wobbles. */
static void rear_shot(uint32_t t0)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), cx = sw / 2, cy = sh / 2, t = (int)t0, s = sh >= 200 ? 2 : 1, i, k;
    int r = sh * 2 / 5, cyc = cy + 4 * s, pull, jig = 0, fx, fy;
    uint8_t sk = PAL_RAMP(RAMP_PEACH, 6);
    const DoomoImage *face;
    if (t < 10) pull = t / 3;
    else if (t < 19) pull = 3 + (t - 10);
    else if (t < 25) pull = 12 - (t - 19) * 2;
    else if (t < 36) pull = 2 + (t - 25) * 12 / 10;
    else if (t < 40) pull = 14;
    else pull = 0;
    pull *= s;
    if (t >= 40) jig = ui_wave(t * 5, 10, DOOMO_MAX(7 - (t - 40) / 2, 0)) * s;
    gg_hall(sh - 10, GG_HALL_DUSK, t0);
    gg_cheeks_closeup(cx, cyc, r, pull / 2, pull / 2, jig, 1);
    if (t < 40) {
        fx = cx + sw / 4 + pull * 2;
        fy = sh - 30 * s + pull;
        for (k = 0; k < 4 * s; k++) {
            uint8_t c = k == 0 ? PAL_RAMP(RAMP_MINT, 6) : k == 4 * s - 1 ? PAL_RAMP(RAMP_MINT, 2) : PAL_RAMP(RAMP_MINT, 4);
            plat_line(cx + 1, cyc + 2 * s + k, (cx + fx) / 2, cyc + 14 * s + k - pull, c);
            plat_line((cx + fx) / 2, cyc + 14 * s + k - pull, fx, fy + k, c);
        }
        for (i = 0; i < 6; i++) plat_rect(cx + (fx - cx) * i / 6, cyc + 2 * s + 12 * s * (i < 3 ? i : 5 - i) / 3 - pull * (i < 3 ? i : 5 - i) / 3, 2 * s, 4 * s + 2, PAL_RAMP(RAMP_MINT, 2));
        gg_ellipse(cx + 1, cyc + 2 * s, 2 * s, 3 * s, PAL_RAMP(RAMP_MINT, 2));
        plat_rect(cx, cyc - r * 2 / 3, 2, r / 2, GG_LINE);
        for (i = 0; i < 3 * s; i++) plat_line(fx + 4 * s, fy + 2 * s + i, sw, sh + 20 * s + i - pull, i == 3 * s - 1 ? PAL_RAMP(RAMP_PEACH, 3) : sk);
        gg_hand(fx, fy, s, -1, sk);
        if (pull > 8 * s) for (k = 0; k < 4; k++) plat_line(cx - 10 - k * 3, cyc + 4 + k * 4, cx - 22 - k * 5, cyc + 2 + k * 6, PAL_WHITE);
        if (t >= 10 && t < 19) gg_shout(cx - 22, 10, "nnngh!", PAL_WHITE);
        if (t >= 25) gg_shout(cx - 34, 10, "NNNGHHH!!", PAL_UI_GOLD);
    } else {
        int q = t - 40;
        gg_ellipse(cx + q * 3, cyc + 2 * s + q, 4 + q * 3, 3 + q * 2, PAL_BLACK);        /* the hose end, at the lens */
        gg_ellipse(cx + q * 3, cyc + 2 * s + q, 3 + q * 3, 2 + q * 2, PAL_RAMP(RAMP_MINT, 4));
        for (i = 0; i < 4; i++) plat_line(cx - 30 + i * 12, cyc - 4 - i * 3, cx - 54 + i * 14, cyc - 22 - i * 5, PAL_WHITE);
        if (q < 24) gg_gas(cx, cyc - 4 - q, 5 + q * 2 / 3, F_CUTE, t0);                /* the pucker lets go */
        gg_hand(cx + sw / 4 + q * 7, sh - 30 * s - q * 4, s, -1, sk);                   /* the fist flies off */
        ui_text_big((sw - ui_text_big_width("SHLORP!", 2)) / 2, 8, "SHLORP!", 2, PAL_UI_GOLD, PAL_RAMP(RAMP_BLOOD, 2));
        /* HELLVILE PARK BOARD OF DECENCY: the sticker slaps on, a corner peels */
        if (q >= 3) {
            int bx = cx - 36 * s, by = cyc - 22 * s, bw = 72 * s, bh = 44 * s, peel = DOOMO_MIN(q - 3, 8) * s;
            int pop = ui_pop_scale(q - 3);
            if (pop > 0) {
                plat_rect(bx + 2, by + 2, bw, bh, PAL_RAMP(RAMP_BLOOD, 0));
                plat_rect(bx, by, bw, bh, PAL_UI_CREAM);
                ui_frame(bx, by, bw, bh, GG_LINE);
                plat_rect(bx, by, bw, 10 * s, PAL_UI_RED);
                ui_text(bx + 3 * s, by + 2 * s, "HELLVILE PARK", PAL_WHITE);
                ui_text(bx + 4 * s, by + 14 * s, "BOARD OF", PAL_BLACK);
                ui_text(bx + 4 * s, by + 22 * s, "DECENCY", PAL_BLACK);
                ui_text(bx + 4 * s, by + 32 * s, "(approved)", PAL_GRAY_D);
                gg_tri(bx + bw - peel, by + bh - peel, bx + bw, by + bh, bx + bw - peel, by + bh, PAL_RAMP(RAMP_PEACH, 6));
                gg_tri(bx + bw - peel, by + bh - peel, bx + bw, by + bh - peel, bx + bw, by + bh, PAL_UI_CREAM);
                plat_line(bx + bw - peel, by + bh - peel, bx + bw, by + bh, GG_LINE);
            }
            for (i = 0; i < 3; i++) plat_line(cx + r + 6, cyc - 20 + i * 5 * s, cx + r + 20, cyc - 24 + i * 5 * s, PAL_UI_CREAM);
            ui_text_outline(8, sh - 14, "*wobble*", PAL_UI_CREAM, PAL_BLACK);
        }
        if (q >= 6 && q < 30) gg_shout(cx + 20, cyc - r - 14, "ahhh~", PAL_UI_PINK);
    }
    /* his head glances back over his shoulder, straight at the lens */
    {
        int hx = 44 * s, hy = 36 * s;
        GgParams p;
        gg_params_init(&p);
        p.layers = GGL_FACE;
        p.seams = s > 1;
        gg_draw(hx + 5 * s, hy + 60 * s + 2 * s, s, GGV_BACK_LOOK, GGP_STAND, t < 40 ? GGE_GRIT : GGE_RELIEF, &p);
        if (t < 40) sweat(hx + 18 * s, hy - 10 * s, t0, 0);
    }
    face = portrait_image(F_CULPRIT, t < 40 ? EXPR_ANGRY : EXPR_SHOCK);
    if (face && t < 48) plat_image(sw - face->w * s - 2 + (t >= 40 ? (t - 40) * 6 : 0), sh - face->h * s + 8 * s, face, s, IMG_SHADOW, 0);
}

/* returns 1 when it drew a full-screen close-up instead of the scene */
static void burp_shot(uint32_t t0, uint32_t ct)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), cx = sw / 2, cy = sh / 2, i, s = sh >= 200 ? 3 : 2;
    GgParams p;
    gg_params_init(&p);
    p.layers = GGL_HEAD | GGL_FACE;
    p.seams = 1;
    p.bob = ui_wave((int)ct * 9, 6, s);
    gg_hall(sh - 10, GG_HALL_DUSK, ct);
    bunting(3, ct);
    gg_draw(cx, cy + 60 * s + 4 * s, s, GGV_FRONT, GGP_STAND, GGE_YELP, &p);
    for (i = 0; i < 9; i++) {                                    /* bagel bits */
        int a = i * 227 + 100, rr = (int)t0 * (3 + i % 4) + 8 * s;
        int bx = cx + FIX2INT(fx_cos(a) * rr), by = cy + 6 * s + FIX2INT(fx_sin(a) * rr) / 2 + (int)t0 * (int)t0 / 30;
        gg_ellipse(bx, by, 3, 3, PAL_PAINT(PAINT_CARAMEL, 1));
        gg_ellipse(bx, by, 1, 1, PAL_RAMP(RAMP_WOOD, 1));
    }
    for (i = 0; i < 3; i++) gg_gas(cx - 20 * s + i * 20 * s + ui_wave((int)ct + i * 9, 30, 4), cy + 4 * s + (int)t0 * (i + 2) / 2 + i * 8, 7 + i * 3 + (int)t0 / 5, F_CUTE, ct + (uint32_t)i * 5);
    ui_text_big((sw - ui_text_big_width("BUUURP", 2)) / 2, 10, "BUUURP", 2, PAL_UI_GOLD, PAL_RAMP(RAMP_BLOOD, 2));
    ui_text_outline(cx - 30, sh - 16, "(wet)", PAL_UI_GOLD, PAL_BLACK);
}

/* the gas cloud, extreme close-up: every heart and sparkle, or every fly */
static void gas_shot(uint32_t t0, uint32_t ct)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), cx = sw / 2, cy = sh / 2, i, s = sh >= 200 ? 2 : 1, cute = F_CUTE;
    const DoomoImage *face;
    plat_rect(0, 0, sw, sh, cute ? PAL_RAMP(RAMP_MINT, 2) : PAL_RAMP(RAMP_SICK, 1));
    plat_checker(0, 0, sw, sh, cute ? PAL_RAMP(RAMP_MINT, 3) : PAL_RAMP(RAMP_SICK, 2));
    for (i = 0; i < 4; i++) {
        int bx = cx - sw / 3 + i * sw / 6 + ui_wave((int)ct + i * 7, 30, 4 * s);
        int by = cy - 6 + (i % 3) * 10 * s - ui_wave((int)ct + i * 5, 24, 4 * s);
        gg_gas(bx, by, sh / 7 + (i & 1) * 5 * s, cute, ct + (uint32_t)i * 11);
    }
    for (i = 0; i < 10; i++) {
        int a = i * 163 + (int)ct * (4 + i % 3), rr = 36 * s + (i * 11) % (50 * s);
        int fx = cx + FIX2INT(fx_cos(a) * rr), fy = cy + FIX2INT(fx_sin(a) * rr) / 2;
        if (cute) ui_sparkle(fx, fy, (int)((ct / 2 + (uint32_t)i) & 15), PAL_WHITE);
        else { plat_rect(fx, fy, 3, 2, PAL_BLACK); plat_rect(fx - 1, fy - 2, 2, 2, PAL_WHITE); plat_rect(fx + 2, fy - 2, 2, 2, PAL_WHITE); }
    }
    if (cute) for (i = 0; i < 4; i++) ui_heart(cx - 40 * s + i * 28 * s, cy - 20 + (i & 1) * 22 * s - (int)((ct + (uint32_t)i * 9) % 30u), 4 + s, PAL_UI_PINK);
    face = portrait_image(crowd_ids[(t0 / 12) % 6], EXPR_SHOCK);
    if (face) plat_image(sw - face->w * s - 4, sh - face->h * s + 6 * s, face, s, IMG_SHADOW, 0);
    sweat(sw - 30 * s, sh - 50 * s, ct, 0);
    ui_text_outline(8, sh - 14, cute ? "*pffrrt* (minty)" : "*pffrrt* (sulphur)", PAL_UI_GOLD, PAL_BLACK);
}

/* the belly skin stretched tight: the harness strap going fibre by fibre, PING */
static void belly_shot(uint32_t t0, uint32_t ct)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), cx = sw / 2, cy = sh / 2, i, s = sh >= 200 ? 2 : 1;
    int snap = (int)t0 * 14 / 36, pulse = ui_wave((int)ct * 6, 10, 2 * s);
    plat_rect(0, 0, sw, sh, PAL_RAMP(RAMP_VAPE, 1));
    gg_blob(cx, cy + sh / 6, sw * 2 / 3 + pulse, sh * 2 / 3 - pulse, RAMP_PEACH, 1);
    gg_ellipse(cx, cy + sh / 6 + 10 * s, 2 * s, 3 * s, PAL_RAMP(RAMP_PEACH, 3));           /* navel */
    for (i = 0; i < 6; i++) {                                                             /* stretch marks */
        int vx = 20 * s + i * (sw - 40 * s) / 5, vy = 10 * s + ((i * 37) % (30 * s));
        plat_line(vx, vy, vx + 6 * s, vy + 16 * s, PAL_RAMP(RAMP_PEACH, 4));
    }
    plat_rect(0, cy - 4 * s, sw, 10 * s, GG_LEATH);                                       /* the harness strap */
    plat_rect(0, cy - 4 * s, sw, 1, GG_LINE);
    plat_rect(0, cy + 6 * s - 1, sw, 1, GG_LINE);
    for (i = 0; i < 12; i++) {                                                            /* its fibres */
        int fy = cy - 3 * s + i * 8 * s / 12, broke = i < snap;
        plat_rect(0, fy, broke ? cx - 30 - i * 2 : sw, 1, PAL_RAMP(RAMP_WOOD, 2 + (i & 1)));
        if (broke) plat_rect(cx + 30 + i * 2, fy + ((int)ct & 1) * 2, sw - cx - 30 - i * 2, 1, PAL_RAMP(RAMP_WOOD, 2 + (i & 1)));
    }
    gg_studs(10, cy, sw - 10, cy, sw / (12 * s), s);
    if (t0 > 20) gg_shout(cx - 14, 6, "creeeak", PAL_WHITE);
    if (t0 >= 26) {                                                                       /* PING: the studs shoot at the lens */
        int q = (int)t0 - 26, k;
        for (k = 0; k < 10; k++) {
            int a = k * 410 + 90, d = 6 + q * (5 + k % 3), sz = 2 + q / 2;
            plat_rect(cx + FIX2INT(fx_cos(a) * d), cy + FIX2INT(fx_sin(a) * d) / 2, sz, sz, GG_STUD);
        }
        ui_text_big((sw - ui_text_big_width("PING!", 2)) / 2, cy - 30, "PING!", 2, PAL_UI_GOLD, PAL_RAMP(RAMP_BLOOD, 2));
    }
    ui_text_outline(8, sh - 14, "*squeeeak*", PAL_UI_CREAM, PAL_BLACK);
}

/* the X-RAY CUTAWAY: a thermometer belly filling with air and bagel */
static void xray_shot(const Stage *g, uint32_t t0, uint32_t ct)
{
    int s = g->s, cx = g->sw / 2 - 20 * s, yf = g->sh - 8 * s, i;
    int bcy = gg_belly_cy(yf, s, GGP_STAND, 300, 0, 0), bx = cx + 6 * s, rr = 22 * s;
    int level = 60 + (int)DOOMO_MIN(t0, 16u) * 2;
    GgParams p;
    gg_params_init(&p);
    p.swell = 300;
    p.seams = s > 1;
    gg_hall(g->sh - 10, GG_HALL_DUSK, ct);
    bunting(3, ct);
    gg_draw_cached(cx, yf, s, GGV_SIDE, GGP_STAND, GGE_BLUSH, &p);
    gg_ellipse(bx, bcy, rr + 2, rr + 2, PAL_UI_CREAM);                                    /* the x-ray circle */
    gg_ellipse(bx, bcy, rr, rr, PAL_RAMP(RAMP_STEEL, 2));
    gg_ellipse(bx, bcy + 2 * s, rr * 3 / 4, rr * 3 / 4 - s, PAL_RAMP(RAMP_STEEL, 4));      /* the stomach */
    gg_ellipse(bx, bcy + 2 * s + (rr * 3 / 4 - s) * (100 - level) / 100, rr * 3 / 4 - s, (rr * 3 / 4 - s) * level / 100, PAL_RAMP(RAMP_SICK, 5));   /* the air */
    for (i = 0; i < 4; i++) gg_ellipse(bx - 8 * s + (i * 7 * s) % (16 * s), bcy + 8 * s - (int)((ct * 2 + (uint32_t)i * 13) % (uint32_t)(14 * s)), 1, 1, PAL_WHITE);
    for (i = 0; i < 3; i++) gg_ellipse(bx - 6 * s + i * 6 * s, bcy + 12 * s + (i & 1) * 3, 2 * s, s + 1, PAL_PAINT(PAINT_CARAMEL, 1));    /* bagel */
    plat_rect(bx - 1, bcy + rr - 4, 2 * s, g->sh - bcy - rr, PAL_RAMP(RAMP_MINT, 4));     /* the hose from below */
    plat_rect(bx + 30 * s, 50, 6 * s, 36 * s, PAL_RAMP(RAMP_STONE, 1));                   /* the thermometer */
    plat_rect(bx + 30 * s + 1, 50 + 36 * s - 36 * s * level / 100, 6 * s - 2, 36 * s * level / 100, PAL_UI_RED);
    ui_text_outline(bx + 38 * s, 50, "FULL", PAL_UI_CREAM, PAL_BLACK);
    ui_text_outline(8, g->sh - 14, "X-RAY: 60% air, 40% bagel", PAL_UI_CREAM, PAL_BLACK);
}

/* the camera angles while he swells (panels 5-7) */
static void crowd_big(int sw, int gy, uint32_t ct, uint32_t t0, int scale, int cheer)
{
    int i;
    for (i = 0; i < 4; i++) {
        int hop = cheer ? ui_wave((int)ct * 2 + i * 11, 8, 6) : 0;
        citizen(crowd_ids[i], sw / 2 - 2 * 28 * scale + i * 28 * scale + 14 * scale, gy - (hop < 0 ? -hop : hop), scale, 0, cheer ? (int)((ct / 5 + (uint32_t)i) & 3) : 0, 1);
        if (cheer && ((t0 / 4 + (uint32_t)i) & 3) == 0) ui_sparkle(sw / 2 - 60 + i * 40, gy - 90 - (i & 1) * 12, (int)((t0 + (uint32_t)i * 3) & 15), PAL_UI_GOLD);
    }
}

static int angle_shot(int panel, uint32_t pt, uint32_t ct, int swell)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), cx = sw / 2, i, k = -1, s = sh >= 200 ? 2 : 1, cute = F_CUTE;
    uint32_t t0 = 0;
    GgParams p;
    for (i = 0; i < NSHOTS; i++)
        if (shots[i].panel == panel && pt >= shots[i].a && pt < shots[i].b) { k = i; t0 = pt - shots[i].a; }
    if (k < 0) return 0;
    if (t0 < 2) { plat_rect(0, 0, sw, sh, PAL_WHITE); return 1; }       /* whip cut */
    gg_params_init(&p);
    sp_idle(&p, ct, s);
    p.swell = swell;
    p.seams = s > 1;

    switch (shots[k].kind) {
    case AN_LOW: {                                       /* from the floor, looking up: the belly looms */
        int yf = sh + 20 * s;
        gg_hall(sh - 12, GG_HALL_DUSK, ct);
        bunting(3, ct);
        p.swell = swell + 60 + (int)t0 * 2;
        p.belly_y = ui_wave((int)ct * 6, 12, 2 * s);
        p.layers = GGL_WINGS | GGL_BACK | GGL_LIVE | GGL_HEAD | GGL_FACE;       /* the belly looms, the head peeks over it */
        gg_draw_cached(cx + 6, yf, s + 1, GGV_FRONT, GGP_STAND, GGE_SULTRY, &p);
        for (i = 0; i < 6; i++) citizen(crowd_ids[i], 20 + i * (sw - 40) / 5, sh + 6 * s, s, 3, 0, 0);   /* the awed crowd's backs */
        break;
    }
    case AN_PUSH: {                                      /* dolly in on the straining belly */
        int r = (int)t0 * 3, yf = sh - 10 * s + r / 2;
        gg_hall(sh - 10, GG_HALL_DUSK, ct);
        bunting(3, ct);
        for (i = 0; i < 10; i++) {                        /* speed lines rushing past the edges */
            int y = (i * 53 + (int)ct * 9) % sh;
            plat_rect(0, y, 14 + r / 6, 1, PAL_WHITE);
            plat_rect(sw - 14 - r / 6, (y + 70) % sh, 14 + r / 6, 1, PAL_WHITE);
        }
        p.swell = swell + 40 + r;
        p.bob = ((int)ct & 1) ? 1 : -1;
        p.belly_x = ui_wave((int)ct * 7, 10, s);
        p.layers = GGL_WINGS | GGL_BACK | GGL_LIVE | GGL_HEAD | GGL_FACE;
        gg_draw_cached(cx + 10 - r / 4, yf, s + 1, GGV_FRONT, GGP_STAND, GGE_BLUSH, &p);
        break;
    }
    case AN_TOP: {                                       /* straight down: a ring of guests round a round gut */
        int ocx = cx - 24 * s, ocy = sh / 2, r = 20 * s + swell * s / 16, a, hr = r + 30 * s;
        plat_rect(0, 0, sw, sh, PAL_RAMP(RAMP_STONE, 2));
        plat_checker(0, 0, sw, sh, PAL_RAMP(RAMP_STONE, 3));
        plat_rect(8, sh - 40 * s, 54 * s, 22 * s, PAL_RAMP(RAMP_WOOD, 3));                 /* the buffet table, from above */
        plat_rect(8, sh - 40 * s, 54 * s, 1, PAL_RAMP(RAMP_WOOD, 5));
        for (i = 0; i < 12; i++) {                                                      /* the ring of guests, every face turned to him */
            int px, py, dir;
            a = i * 341 + 60;
            px = ocx + FIX2INT(fx_cos(a) * hr);
            py = ocy + FIX2INT(fx_sin(a) * hr) * 3 / 4;
            dir = (a & 4095) < 1024 || (a & 4095) >= 3072 ? 1 : 2;
            if (((a + 512) & 4095) < 1024) dir = 3;
            if (((a + 512) & 4095) >= 2048 && ((a + 512) & 4095) < 3072) dir = 0;
            citizen(crowd_ids[i % 6], px, py + 10 * s, s, dir, 0, 1);
        }
        gg_blob(ocx + r * 9 / 10, ocy - r / 3, r / 2 + 1, r / 2 + 1, RAMP_BLOOD, s > 1);   /* rump (top view) */
        gg_blob(ocx + r * 9 / 10, ocy + r / 3, r / 2 + 1, r / 2 + 1, RAMP_BLOOD, s > 1);
        gg_blob(ocx, ocy, r + 1 + p.belly_y, r - p.belly_y, RAMP_PEACH, 1);               /* the belly */
        plat_line(ocx - r, ocy - r / 3, ocx + r, ocy + r / 3, GG_LEATH);                  /* harness */
        plat_line(ocx - r, ocy - r / 3 + 1, ocx + r, ocy + r / 3 + 1, GG_LEATH);
        gg_blob(ocx - r - 8 * s, ocy, 10 * s, 10 * s, RAMP_BLOOD, s > 1);                 /* head, horns out */
        gg_tri(ocx - r - 14 * s, ocy - 7 * s, ocx - r - 10 * s, ocy - 4 * s, ocx - r - 20 * s, ocy - 16 * s, GG_HORN);
        gg_tri(ocx - r - 14 * s, ocy + 7 * s, ocx - r - 10 * s, ocy + 4 * s, ocx - r - 20 * s, ocy + 16 * s, GG_HORN);
        {   /* the valve being cranked, and the culprit, from above */
            int vx = ocx + r + 28 * s, vy = ocy + r / 2, an = (int)ct * 48;
            gg_ellipse(vx, vy, 4 * s, 4 * s, PAL_RAMP(RAMP_STEEL, 3));
            plat_line(vx, vy, vx + FIX2INT(fx_cos(an) * 14 * s), vy + FIX2INT(fx_sin(an) * 14 * s), PAL_RAMP(RAMP_STEEL, 6));
            plat_line(vx, vy, vx - FIX2INT(fx_cos(an) * 14 * s), vy - FIX2INT(fx_sin(an) * 14 * s), PAL_RAMP(RAMP_STEEL, 6));
            gg_blob(vx + 20 * s, vy + 10 * s, 7 * s, 7 * s, RAMP_PEACH, 0);
        }
        break;
    }
    case AN_BEHIND: {                                    /* low, from behind the crowd: backs of heads, the rump huge */
        int yf = sh - 10 * s;
        gg_hall(sh - 10, GG_HALL_DUSK, ct);
        bunting(3, ct);
        p.ly = ui_wave((int)ct * 4, 10, 2 + swell / 60);
        p.ry = -p.ly;
        gg_draw_cached(cx, yf, s + 1, GGV_BACK, GGP_INFLATED, GGE_GRIN, &p);
        for (i = 0; i < 6; i++) {                         /* foreground: the crowd's backs, every head turned his way */
            int hx = 22 * s + i * (sw - 44 * s) / 5, hy = sh - 36 * s - (i & 1) * 8 * s;
            gg_ellipse(hx, hy, 15 * s, 15 * s, PAL_BLACK);
            gg_ellipse(hx, hy + 24 * s, 24 * s, 16 * s, PAL_BLACK);
            if (((t0 / 5 + (uint32_t)i) & 3) == 0) ui_text_outline(hx - 3, hy - 32 * s, "!", PAL_UI_GOLD, PAL_BLACK);
        }
        break;
    }
    case AN_FRONT: {                                     /* straight on, FULL: gas both ways, the hose in the back */
        int yf = sh - 16 * s, bx, by, n;
        gg_hall(sh - 12, GG_HALL_DUSK, ct);
        bunting(3, ct);
        p.swell = DOOMO_MAX(swell, 256);
        p.bob = ui_wave((int)ct * 9, 6, 1 + (int)DOOMO_MIN(t0, 16u) / 8);
        p.belly_y = ui_wave((int)ct * 9 + 2, 6, 1 + (int)DOOMO_MIN(t0, 16u) / 4);
        gg_draw_cached(cx, yf, s, GGV_FRONT, GGP_INFLATED, GGE_YELP, &p);
        bx = cx + 32 * s;
        by = yf - 30 * s + 4 * s;
        hose(bx, by + 2 * s, sw - 20, sh - 20, s, 6, ct);
        valve(bx, by, s, 0);
        n = 1 + (int)DOOMO_MIN(t0, 20u) / 5;
        for (i = 0; i < n; i++) {                                                        /* BURP */
            int u = (int)((ct * 3 + (uint32_t)i * 17) % 40u);
            gg_gas(cx + 10 * s + u, yf - 66 * s - u / 2 + ui_wave((int)ct * 4 + i * 7, 12, 3), 5 + u / 8, cute, ct + (uint32_t)i * 7);
        }
        for (i = 0; i < n; i++) {                                                        /* BRAAP */
            int u = (int)((ct * 3 + (uint32_t)i * 13) % 50u);
            gg_gas(bx + 10 * s + u, by + 12 * s + u / 3 + ui_wave((int)ct * 5 + i * 9, 10, 3), 5 + u / 8, cute, ct + (uint32_t)i * 11);
        }
        gg_shout(cx + 30 * s, yf - 80 * s, "BURP", PAL_UI_CREAM);
        gg_shout(bx + 14 * s, by + 24 * s, shots[k].panel == 5 ? "TOOT" : "BRAAAP", PAL_UI_CREAM);
        if (t0 < 10)                                                                     /* speed lines on the first beat */
            for (i = 0; i < 10; i++) {
                int a = i * 410;
                plat_line(cx + FIX2INT(fx_cos(a) * 50 * s), yf - 40 * s + FIX2INT(fx_sin(a) * 50 * s) / 2,
                          cx + FIX2INT(fx_cos(a) * 220), yf - 40 * s + FIX2INT(fx_sin(a) * 220) / 2, PAL_WHITE);
            }
        break;
    }
    default:                                             /* cutaway: the crowd, thinking it's over */
        gg_hall(sh * 3 / 4, GG_HALL_DUSK, ct);
        bunting(3, ct);
        crowd_big(sw, sh * 3 / 4 + 10 * s, ct, t0, s + 1, 1);
        for (i = 0; i < 5; i++) ui_heart(36 + i * (sw - 60) / 4, 40 + (i & 1) * 18 - ui_wave((int)ct + i * 7, 20, 4), 4, PAL_UI_PINK);
        ui_text_big((sw - ui_text_big_width("YAAAY!", 2)) / 2, 20, "YAAAY!", 2, PAL_UI_GOLD, PAL_RAMP(RAMP_BLOOD, 2));
        break;
    }
    /* letterbox bars + camera tag */
    plat_rect(0, 0, sw, 12, PAL_BLACK);
    plat_rect(0, sh - 12, sw, 12, PAL_BLACK);
    plat_text(14, 2, shots[k].name, PAL_WHITE);
    if ((ct / 12) & 1) gg_ellipse(7, 6, 3, 3, PAL_UI_RED);
    return 1;
}

/* where the loose rump is t ticks after the POP: it zig-zags round the park like a
 * let-go balloon (wide swings that settle, a slow fall), then lands */
static void rump_pos(int t, int cx, int cy, int gy, int *x, int *y)
{
    int f = DOOMO_MIN(t, 96);
    *x = cx + 10 + ui_wave(f * 3, 56, 70 - f / 2);
    *y = cy - 40 + f * 2 / 3 + ui_wave(f * 5 + 14, 36, 26 - f / 5);
    if (*y > gy - 12) *y = gy - 12;
}

/* ====================================================================== */
/* the epilogue: candlelit, slow, sultry (panels 10 and 11)                 */
static void epilogue(int panel, uint32_t pt, uint32_t ct)
{
    Stage g;
    int t = (int)pt, i, s;
    GgParams p;
    stage(&g);
    s = g.s;
    gg_params_init(&p);
    if (panel == 10) {
        int cx = g.cx - 10 * s;
        hall_wide(&g, GG_HALL_NIGHT, 0, GG_CROWD_SWOON, 0, ct);
        gg_chair(cx + 20 * s, g.yf - 14 * s, 26 * s, 0);
        sp_idle(&p, ct, s);
        p.swell = 60 + ui_wave((int)ct, 60, 20);                                        /* slow, satisfied breaths */
        p.bob = ui_wave((int)ct, 64, 1);
        p.dir = -1;
        p.seams = s > 1;
        gg_draw_cached(cx, g.yf, s, GGV_SIDE, GGP_STAND, GGE_SULTRY, &p);
        plat_rect(cx - 22 * s, g.yf - 54 * s, 8, 2, PAL_RAMP(RAMP_BONE, 6));             /* the cigarette */
        plat_rect(cx - 23 * s, g.yf - 54 * s, 2, 2, PAL_UI_GOLD);
        smoke(cx - 24 * s, g.yf - 56 * s, 40, ct);
        if (t > 20 && t < 110) gg_shout(cx - 56, g.yf - 76 * s - t / 8, "mmm~", PAL_UI_PINK);
        hearts(cx + 50 * s, g.yf - 40 * s, 3, ct);
        return;
    }
    /* panel 11: the close-up, lit by candle glow, straight into the lens */
    {
        int sc = s + 2, wk = t >= 60 && t < 78, cx = g.sw / 2, cy = g.sh / 2 - 4 * sc;
        plat_rect(0, 0, g.sw, g.sh, PAL_RAMP(RAMP_BLOOD, 0));
        gg_ellipse(cx, cy, g.sw / 2, g.sh / 2, PAL_RAMP(RAMP_LAVA, 0));                /* warm glow */
        gg_ellipse(cx, cy, g.sw / 3, g.sh / 3, PAL_RAMP(RAMP_LAVA, 1));
        plat_checker(0, 0, g.sw, g.sh, PAL_RAMP(RAMP_BLOOD, 0));
        p.layers = GGL_HEAD | GGL_FACE;
        p.seams = 1;
        p.bob = ui_wave((int)ct, 40, sc);
        gg_draw(cx, cy + 60 * sc + 2 * sc, sc, GGV_FRONT, GGP_STAND, wk ? GGE_WINK : GGE_SULTRY, &p);
        if (t > 90) for (i = 0; i < 7; i++) {                                              /* the exhale, over the lens */
            int q = t - 90, k = i * 13;
            gg_ellipse(cx + ui_wave(q * 3 + k, 40, 20 + i * 6), cy + 4 * sc - q * (i + 2) / 3, 6 + q / 3 + i, 4 + q / 4 + i, PAL_RAMP(RAMP_STONE, 5 + (i & 1)));
        }
        for (i = 0; i < 3; i++) ui_heart(cx - 60 + i * 60, 20 + (int)((ct + (uint32_t)i * 13) % 20u), 4, PAL_UI_PINK);
        if (t > 30 && t < 150) gg_shout(cx - 36, 8, "stay a while~", PAL_UI_PINK);
        if (t > 130) {                                                                     /* iris-out: bars close */
            int bar = (t - 130) * g.sh / 80;
            plat_rect(0, 0, g.sw, DOOMO_MIN(bar, g.sh / 2), PAL_BLACK);
            plat_rect(0, g.sh - DOOMO_MIN(bar, g.sh / 2), g.sw, DOOMO_MIN(bar, g.sh / 2), PAL_BLACK);
        }
    }
}

/* ---- the bike pump (panels 12 and 13) -------------------------------------------------
 * Night park again. A floor pump, a hose that goes round to the rump (brass valve
 * at the back, cheeks only, profile), and Grumbleguts working the handle himself.
 * Every stroke adds a notch of belly. Eyes cross, tongue out. He just gets bigger
 * and stays put: no pop, no lift-off. */
static void pump_scene(int panel, uint32_t pt, uint32_t ct)
{
    Stage g;
    int t = (int)pt, per = panel == 12 ? 20 : 24, n = t / per, ph = t % per, i, s, pr, swell, shake;
    int cx, px, py, hy2, ax, ay, k;
    GgParams p;
    stage(&g);
    s = g.s;
    cx = g.cx + 10 * s;
    if (panel == 12) pr = n * 6 + (ph >= per / 2 ? DOOMO_MIN((ph - per / 2) * 6 / 4, 6) : 0);
    else pr = 60 + n * 5 + (ph >= per / 2 ? DOOMO_MIN((ph - per / 2) * 5 / 4, 5) : 0);
    pr = DOOMO_MIN(pr, 100);
    swell = 40 + pr * 4;
    shake = ui_wave((int)ct * 5, 10, pr / 25);
    px = 30 * s;
    py = g.floor - 2 * s;
    hall_wide(&g, GG_HALL_NIGHT, 0, panel == 12 ? GG_CROWD_SWOON : GG_CROWD_SLEEP, 0, ct);

    gg_params_init(&p);
    sp_idle(&p, ct, s);
    p.swell = swell;
    p.dir = 1;
    p.bob += shake;
    p.belly_y = ph >= per / 2 ? -(ph - per / 2) * s / 3 : 0;
    p.seams = s > 1;
    ax = cx - 10 * s - gg_butt_r(s, GGP_STAND, swell) + 2 * s;                     /* the valve at the back */
    ay = g.yf - 30 * s + 8 * s;
    /* the floor pump, behind him: base, barrel, handle going down on the push */
    hy2 = py - 34 * s + (ph < per / 2 ? ph * 14 * s / (per / 2) : (per - ph) * 14 * s / (per / 2));
    plat_rect(px - 12 * s, py - 3, 24 * s, 4, PAL_BLACK);
    plat_rect(px - 5 * s, py - 34 * s, 10 * s, 32 * s, PAL_RAMP(RAMP_STEEL, 3));
    plat_rect(px - 5 * s, py - 34 * s, 3 * s, 32 * s, PAL_RAMP(RAMP_STEEL, 6));
    plat_rect(px - s, hy2, 2 * s, py - 34 * s - hy2, PAL_RAMP(RAMP_STEEL, 5));
    plat_rect(px - 10 * s, hy2 - 3, 20 * s, 4, PAL_BLACK);                            /* T-handle */
    plat_rect(px - 9 * s, hy2 - 2, 18 * s, 2, PAL_UI_GOLD);
    hose(px + 5 * s, py - 30 * s, ax, ay, s, 5, ct);
    if (ph >= per / 2) {                                                             /* a bulge runs down the hose */
        int u = (ph - per / 2) * 100 / (per / 2), mx = (px + ax) / 2, my = DOOMO_MAX(py - 30 * s, ay) + 10 * s, bx, by;
        if (u < 50) { bx = px + (mx - px) * u / 50; by = py - 30 * s + (my - py + 30 * s) * u / 50; }
        else { bx = mx + (ax - mx) * (u - 50) / 50; by = my + (ay - my) * (u - 50) / 50; }
        gg_ellipse(bx, by + 2, 3 * s, 3 * s, PAL_RAMP(RAMP_MINT, 5));
    }
    gg_draw_cached(cx, g.yf, s, GGV_SIDE, GGP_STAND, GGE_DOPEY, &p);
    valve(ax, ay, s, 0);
    /* his far arm reaches back to the handle */
    for (k = 0; k < 3 * s; k++) plat_line(cx - 4 * s, g.yf - 44 * s + k, px + 2 * s, hy2 + k, k == 3 * s - 1 ? PAL_RAMP(RAMP_BLOOD, 2) : PAL_RAMP(RAMP_BLOOD, 5));
    gg_blob(px + 2 * s, hy2, 4 * s, 4 * s, RAMP_BLOOD, 0);
    if (pr > 40) sweat(cx + 12 * s, g.yf - 70 * s, ct, 0);
    /* sound effects, in letters */
    if (ph >= per / 2 && ph < per / 2 + 8) gg_shout(px - 12, py - 44 * s - ph, "pff!", PAL_WHITE);
    if (panel == 12 && pr > 20 && (t % 40) > 22 && (t % 40) < 36) gg_shout(cx + 20 * s, g.yf - 78 * s, "mmm~", PAL_UI_PINK);
    if (panel == 13 && pr > 70) gg_shout(cx + 10 * s, g.yf - 30 * s - gg_belly_ry(s, GGP_STAND, swell) - 14, "squeeeeak", PAL_UI_GOLD);
    if (panel == 13 && t > 130 && t < 168) gg_shout(cx + 30 * s, g.yf - 60 * s, "...one more.", PAL_UI_PINK);
    if (panel == 13 && pr > 85) for (i = 0; i < 3; i++) plat_line(cx + 30 * s + i * 4, g.yf - 20 * s - i * 6, cx + 40 * s + i * 4, g.yf - 24 * s - i * 6, PAL_WHITE);
}

/* ====================================================================== */
/* the long night (panels 14-19)                                            */
/* a hand (an imp's, a guest's) coming in to slap or boop. fingers point toward dir */
static void guest_hand(int x, int y, int s, int dir, uint8_t col, uint8_t cuff)
{
    plat_rect(DOOMO_MIN(x - dir * 14 * s, x - dir * 5 * s), y - 3 * s, 9 * s, 6 * s, cuff);
    gg_hand(x, y, s, dir, col);
}

/* the full-screen cut-ins (see gg_cuts): a hard cut, and the camera goes where the
 * night wants it: onto his rump (BACK / LOOK) or square on the belly (SIDE). Every
 * slap or boop lands on the same tick the springs are kicked (sp_motion), so the
 * hand, the SMACK!, the flush on the skin and the ripple all agree. */
static void cut_shot(int panel, uint32_t pt, uint32_t ct, const GgCut *cut)
{
    static const char *const smack[4] = { "SMACK!", "WHAP!", "CLAP!", "SPLAT~" };
    static const char *const back_say[6] = { "hnnnf~", "oooh~", "page one~", "next~", "don't stop~", "mmm~" };
    static const char *const look_say[6] = { "...see something?", "like the view?", "Shriekra~", "front of the line~", "your turn~", "brunch?" };
    int sw = plat_screen_w(), sh = plat_screen_h(), t = (int)pt, k = t - cut->a, i;
    int s0 = sh >= 200 ? 2 : 1, s = s0, dawn = panel == 19;         /* the figure at the wide shot's scale: both layers stay cached */
    int big = panel == 14 || panel == 15 || panel == 18, rear = cut->view == GGV_BACK || cut->view == GGV_BACK_LOOK;
    int every = sp_every(panel), h0 = cut->a + 4, nn, np, dtn, dtp, handed;
    int expr = (rear && cut->view == GGV_BACK_LOOK) ? ((t / 24) & 1 ? GGE_WINK : GGE_SMUG)
             : panel == 14 ? GGE_BLUSH : panel == 15 ? GGE_LOVE : panel == 18 ? (((t / 20) & 1) ? GGE_LOVE : GGE_BLUSH) : (((t / 16) & 1) ? GGE_WINK : GGE_GRIN);
    int cx = sw / 2 + ui_wave((int)ct * 7, 14, rear ? (panel >= 17 ? s : s / 2 + 1) : 0), yf, cheeky = sh * 72 / 100;
    int swell = big ? 256 : 0, r = gg_butt_r(s, GGP_STAND, swell);
    GgParams m;

    if (rear) yf = cheeky + 26 * s - r / 2; else yf = sh - 10;
    gg_params_init(&m);
    sp_motion(&m, every, cut->a + 4, (panel == 17 ? 4 : panel == 18 ? 5 : panel == 19 ? 2 : 3) * s, t, ct, s, 1);
    m.swell = swell;
    m.seams = s0 > 1;
    m.dir = 1;

    /* hard cut: two white frames, then the lit stage */
    if (k < 2) { plat_rect(0, 0, sw, sh, PAL_WHITE); return; }
    if (s0 == 1) yf = sh - 26;                               /* the small screen frames him higher, over the crowd's heads */
    gg_hall(yf - 8, dawn ? GG_HALL_DAWN : GG_HALL_NIGHT, ct);
    gg_ellipse(sw / 2, yf - 8, sw / 2, 12, dawn ? PAL_RAMP(RAMP_GRASS, 5) : PAL_RAMP(RAMP_STONE, 3));   /* a spotlight pool */
    bunting(3, ct);

    /* which hit is coming up, and which one has just landed */
    nn = t <= h0 ? 0 : (t - h0 + every - 1) / every;
    dtn = h0 + nn * every - t;
    np = t >= h0 ? (t - h0) / every : -1;
    dtp = np >= 0 ? t - (h0 + np * every) : 99;
    handed = rear && panel != 14;                            /* the valve panel has no slapper: the pulses do it */

    if (rear) {
        int lcx = cx - r * 7 / 10 + m.lx, rcx = cx + r * 7 / 10 + m.rx, cyc = yf - 26 * s + r / 2;
        int hit, hs, hx, hy, vy;
        gg_draw_cached(cx, yf, s, cut->view, GGP_STAND, expr, &m);
        vy = cyc - r / 2 + 9 * s;
        if (panel == 14) {                                   /* the valve in the crease, and the hiss that comes out of it */
            valve(cx, vy, s0, 1);
            for (i = 0; i < 5; i++) {
                int u = (int)((ct * 3 + (uint32_t)i * 9) % 60u);
                gg_gas(cx + u * (i & 1 ? 1 : -1) / 2, vy + 8 + u / 2 + ui_wave((int)ct * 5 + i * 7, 10, 3), 3 + u / 12, F_CUTE, ct + (uint32_t)i * 5);
            }
            if ((t % 22) < 12) gg_shout(cx - 34, vy + 24 * s, "hissssss~", PAL_WHITE);
        }
        /* the slap: a hand sweeps in, lands on a cheek, rings it */
        if (handed) {
            for (hit = 0; hit < 2; hit++) {
                int n = hit ? np : nn, d = hit ? dtp : dtn;
                if (n < 0 || d > 5) continue;
                hs = (n & 1) ? 1 : -1;
                hx = (hs < 0 ? lcx : rcx) + hs * (r / 4 + d * 11 * s / 2);
                hy = (hs < 0 ? cyc + m.ly : cyc + m.ry) - r / 4 - (hit ? d * 2 : 0);
                guest_hand(hx, hy, s0, -hs, PAL_RAMP(RAMP_PEACH, 4 + (n % 3)), PAL_PAINT((n * 7 + 3) % PAL_PAINT_COUNT, 1));
            }
        }
        if (np >= 0 && handed) {
            hs = (np & 1) ? 1 : -1;
            hx = hs < 0 ? lcx : rcx;
            hy = (hs < 0 ? cyc + m.ly : cyc + m.ry) - r / 4;
            if (dtp < every - 2) {                           /* the flush: a pink hand print that cools off */
                uint8_t pc = dtp < 18 ? PAL_RAMP(RAMP_CANDY, 6) : PAL_RAMP(RAMP_CANDY, 4);
                gg_ellipse(hx - hs * r / 5, hy, 4 * s, 3 * s, pc);
                for (i = 0; i < 4; i++) gg_ellipse(hx - hs * (r / 5 + 5 * s), hy - 4 * s + i * 3 * s, 3 * s, s, pc);
            }
            if (dtp < 7) {
                gg_burst(hx + hs * r / 3, hy, 8 * s + (6 - dtp) * 2, dtp < 3 ? PAL_WHITE : PAL_UI_GOLD);
                if (sh >= 200) ui_text_big(DOOMO_CLAMP(hx - 30, 4, sw - 100), hy - r - 6 * s - dtp, smack[np & 3], 2, PAL_WHITE, PAL_UI_RED_D);
                else gg_shout(hx - 20, hy - r - 6, smack[np & 3], PAL_WHITE);
            }
            for (i = 0; i < 3; i++)                          /* hearts pop off every hit */
                if (dtp < 22) ui_heart(hx + (i - 1) * 12 * s + hs * 6, hy - r / 2 - dtp * 3 - i * 5 * s, 2 + (i & 1), PAL_UI_PINK);
        }
        for (i = 0; i < 2; i++) {                            /* shimmy lines hugging a ringing cheek */
            int cyl = i ? m.ry : m.ly, cxx = i ? rcx + r + 2 : lcx - r - 2, sg = i ? 1 : -1, j;
            if (cyl > 2 || cyl < -2) for (j = 0; j < 3; j++)
                plat_line(cxx + sg * 2, cyc + cyl - 8 * s + j * 6 * s, cxx + sg * (6 + j * 3), cyc + cyl - 8 * s + j * 6 * s, PAL_WHITE);
        }
        ui_sparkle(lcx - r / 2, cyc - r / 2 + m.ly, (int)((ct / 2) & 15), PAL_WHITE);              /* glistening */
        ui_sparkle(rcx + r / 2, cyc - r / 2 + m.ry, (int)((ct / 2 + 7) & 15), PAL_WHITE);
        if (cut->view == GGV_BACK_LOOK) {
            if (((t / 26) & 1)) gg_shout(cx + 26 * s, yf - 56 * s, look_say[panel - 14], PAL_UI_PINK);
        } else if ((t % 30) < 18 && panel != 14) {
            gg_shout(cx - 70, yf - 62 * s - 8, back_say[panel - 14], PAL_UI_PINK);
        }
        if (panel == 14 && cut->view == GGV_BACK_LOOK && t > 190) gg_shout(cx - 24, cheeky - r - 10 * s, "BLORP", PAL_UI_GOLD);
        if (panel == 18 && ((t / 30) & 1)) gg_shout(4, 6, "Mavis~", PAL_UI_PINK);
        if (panel == 18 && !((t / 30) & 1)) gg_shout(4, 6, "Gary~?", PAL_UI_PINK);
    } else {
        /* PROFILE: belly out front, a hand booping it, the whole body ringing */
        int dir = 1, belly_r = gg_belly_r(s, GGP_STAND, swell), belly_ry = gg_belly_ry(s, GGP_STAND, swell);
        int bcy = gg_belly_cy(yf, s, GGP_STAND, swell, m.bob, m.belly_y);
        int fx = cx + dir * (6 * s + m.belly_x) + dir * (belly_r - 2 * s), hx = cx + 4 * s + 11 * s, hy = yf - 60 * s - 3 * s;
        gg_draw_cached(cx, yf, s, GGV_SIDE, GGP_STAND, expr, &m);
        if (np >= 0 && dtp < 12)                             /* ripple rings run out from every hit */
            for (i = 1; i <= 3; i++)
                if (dtp > i) gg_ellipse(fx - dir * (dtp + i * 3), bcy, 1 + i, 4 * s + i * 2, PAL_RAMP(RAMP_PEACH, 4 + (i & 1)));
        if (panel == 15 || panel == 17) {                    /* an imp / a ticket-holder boops the belly */
            for (i = 0; i < 2; i++) {
                int n = i ? np : nn, d = i ? dtp : dtn;
                if (n < 0 || d > 5) continue;
                guest_hand(fx + dir * (6 * s + d * 12 * s / 2), bcy + (n & 1 ? -s : s) * 3, s0, -dir, PAL_RAMP(RAMP_PEACH, 4 + (n % 3)), PAL_PAINT((n * 7 + 3) % PAL_PAINT_COUNT, 1));
            }
            if (np >= 0 && dtp < 8) gg_shout(fx - 30, bcy - belly_ry - 14 - dtp, (np & 1) ? "squelch~" : "boop!", PAL_UI_GOLD);
        }
        if (panel == 15) {                                   /* BRRRAAAP: it leaves his mouth and crosses the park */
            for (i = 0; i < 6; i++) {
                int u = (int)((ct * 4 + (uint32_t)i * 13) % 90u);
                gg_gas(hx + 14 * s + u, hy + 10 * s + ui_wave((int)ct * 3 + i * 5, 16, 4), 5 + u / 12, F_CUTE, ct + (uint32_t)i * 7);
            }
            if (k > 4) gg_shout(hx + 10 * s, hy - 24, "BRRRAAAAP", PAL_UI_GOLD);
        }
        if (panel == 14) {                                   /* the valve whistles at his hip */
            for (i = 0; i < 6; i++) {
                int u = (int)((ct * 3 + (uint32_t)i * 11) % 70u);
                gg_gas(fx - 10 * s + u, bcy - 6 * s - u / 4 + ui_wave((int)ct * 4 + i * 7, 12, 4), 4 + u / 14, F_CUTE, ct + (uint32_t)i * 7);
            }
            valve(fx - 2, bcy - 6, s0, 1);
            gg_shout(cx - 30 * s, hy - 26, "gurrrgle~", PAL_UI_PINK);
        }
        if (panel == 18) {                                   /* the pump: a hose from the valve, the handle hammering */
            int stroke = ui_wave((int)ct * 6, 12, 5);
            hose(fx - 2, bcy, sw - 30, yf - 20, s0, 5, ct);
            plat_rect(sw - 34, yf - 22, 6, 24, PAL_BLACK);
            plat_rect(sw - 40, yf - 28 + stroke, 14, 3, PAL_BLACK);
            if ((t % 10) > 4) gg_shout(sw - 70, yf - 54, "pff!", PAL_WHITE);
        }
        hearts(cx, yf - 50 * s, 3, ct);
        if ((t % 34) < 16) gg_shout(hx - 26 * s, hy - 14 * s, panel == 17 ? "next~" : panel == 18 ? "harder~" : "mmh~", PAL_UI_PINK);
    }
    if (s0 == 1)                                             /* shot from behind the crowd: the backs of their heads */
        for (i = 0; i < 5; i++) {
            int hx2 = 24 + i * (sw - 48) / 4 + ui_wave((int)ct + i * 9, 40, 2), hy2 = sh - 2 - (i & 1) * 5;
            gg_ellipse(hx2, hy2, 12, 11, PAL_BLACK);
        }
}

static void night_scene(int panel, uint32_t pt, uint32_t ct)
{
    static const char *const names[5] = { "Shriekra~", "Spearmint~", "the waiter~", "the band~", "the gazebo~" };
    static const char *const wrong[4] = { "oh, Gary~", "yes, Mavis~", "...the Gate?", "Steve~~" };
    static const char *const nums[3] = { "No. 46", "No. 47", "No. 48" };
    Stage g;
    int i, k, t = (int)pt, s, dawn = panel == 19, big = panel == 14 || panel == 15 || panel == 18;
    int shake, cx, yf, r, cy, hx, hy, ax, ay, poke = panel == 15 ? ((t % 28) < 10 ? (t % 28) : 0) : 0;
    const GgCut *cut = cut_at(panel, pt);
    GgParams mo;
    int expr = panel == 14 ? GGE_BLUSH : panel == 15 ? GGE_LOVE : panel == 16 ? GGE_SMUG
             : panel == 17 ? (((t / 40) & 1) ? GGE_WINK : GGE_GRIN) : panel == 18 ? (((t / 50) & 1) ? GGE_LOVE : GGE_BLUSH) : GGE_SULTRY;

    if (cut) { cut_shot(panel, pt, ct, cut); return; }       /* rump-cam / profile cut-in */
    stage(&g);
    s = g.s;
    shake = ui_wave((int)ct * 6, 8, (panel == 14 || panel == 18) ? 3 : 1) * (big ? 1 : 0);
    cx = g.cx + shake + ui_wave((int)ct * 5, 10, panel >= 17 && panel <= 18 ? 2 : 1);
    yf = g.yf;
    gg_params_init(&mo);
    sp_motion(&mo, sp_every(panel), 0, (panel == 17 ? 4 : panel == 18 ? 5 : panel == 19 ? 2 : 3) * s, t, ct, s, 0);
    mo.swell = big ? 256 : 0;
    mo.seams = s > 1;
    r = gg_belly_r(s, GGP_STAND, mo.swell);
    cy = gg_belly_cy(yf, s, GGP_STAND, mo.swell, mo.bob, mo.belly_y);
    hx = cx;
    hy = yf - 60 * s;
    ax = cx + r + 2 * s;
    ay = cy - 6 * s;

    hall_wide(&g, dawn ? GG_HALL_DAWN : GG_HALL_NIGHT, 0, dawn ? GG_CROWD_SLEEP : GG_CROWD_SWOON, 0, ct);
    gg_draw_cached(cx, yf, s, GGV_FRONT, GGP_STAND, expr, &mo);
    {   /* every beat throws sweat and a heart, and a speed arc shows which way the hips went */
        int kt = t % sp_every(panel);
        if (kt < 7) {
            ui_heart(cx + 22 * s + kt * 3, yf - 62 * s - kt * 3, 2 + (kt < 3), PAL_UI_PINK);
            gg_ellipse(cx - 20 * s - kt * 2, yf - 52 * s + kt, 1, 1, PAL_RAMP(RAMP_SKY, 7));
            if (kt < 3) { plat_line(cx - 30 * s, yf - 24 * s, cx - 36 * s, yf - 24 * s, PAL_WHITE); plat_line(cx + 30 * s, yf - 24 * s, cx + 36 * s, yf - 24 * s, PAL_WHITE); }
        }
    }

    if (panel == 14) {
        /* the valve whistles at his hip, a rolling train of gas puffs leaves it */
        for (i = 0; i < 7; i++) {
            int u = (int)((ct * 3 + (uint32_t)i * 11) % 80u);
            gg_gas(ax + 8 + u, ay - 4 + ui_wave((int)ct * 4 + i * 7, 12, 5) - u / 8, 4 + u / 14, F_CUTE, ct + (uint32_t)i * 7);
        }
        valve(ax + 2, ay, s, 1);
        for (i = 0; i < 3; i++) gg_ellipse(cx - r / 3 + i * 14 * s / 2, cy + r / 4 + ui_wave((int)ct * 5 + i * 9, 10, 3), 5, 3, PAL_RAMP(RAMP_PEACH, 7));   /* gurgling */
        if ((t % 32) > 4 && (t % 32) < 26) gg_shout(ax - 6, ay - 34 - (t % 32) / 3, "hissssss~", PAL_WHITE);
        if (t > 30 && (t % 36) > 18) gg_shout(cx - r - 20, cy + r / 6, "gurrrgle", PAL_UI_PINK);
        if (t > 40 && (t % 40) < 20) gg_shout(hx - 60, hy - 6, "mmmh~", PAL_UI_PINK);
        for (i = 0; i < 3; i++) sweat(hx + 12 * s + i * 4, hy - 4 * s, ct, i * 7);
        if (t > 110) {                                                  /* a bubble climbs his throat */
            int by = hy + 12 * s + (t - 110) * 18 / 80;
            gg_ellipse(hx, DOOMO_MIN(by, hy + 18 * s), 3 + (t - 110) / 30, 3 + (t - 110) / 30, PAL_RAMP(RAMP_SKY, 6));
        }
        if (t > 170) gg_shout(hx - 30, hy - 40, "BLORP", PAL_UI_GOLD);
    } else if (panel == 15) {
        /* the burp rolls out of his mouth and across the park */
        for (i = 0; i < 6; i++) {
            int u = (int)((ct * 4 + (uint32_t)i * 13) % 90u);
            if (t > 6) gg_gas(hx - 18 * s - u, hy + 8 * s + ui_wave((int)ct * 3 + i * 5, 16, 4), 5 + u / 12, F_CUTE, ct + (uint32_t)i * 11);
        }
        if (t > 8 && t < 70) gg_shout(hx - 90, hy - 20, "BRRRAAAAP", PAL_UI_GOLD);
        /* an imp pokes the belly; it ripples and answers */
        citizen(ID_SOOTKIN, 14 * s, g.floor + 4 * s, s, 2, 0, 1);
        plat_rect(20 * s, cy + r / 4, cx - r + 6 + poke * 2 - 20 * s, 2 * s, PAL_RAMP(RAMP_PEACH, 5));
        if (poke) {
            for (i = 1; i < 4; i++) gg_ellipse(cx - r + 8, cy + r / 4, poke * i, 3 + poke, PAL_RAMP(RAMP_PEACH, 3 + i));
            gg_shout(cx - r - 14, cy + r / 4 - 18, "squelch~", PAL_UI_PINK);
        }
        hearts(g.sw / 2, g.floor - 40 * s, 4, ct);
        if (t > 80 && (t % 30) < 16) gg_shout(hx + 40, hy - 10, "harder~", PAL_UI_PINK);
    } else if (panel == 16) {
        /* the little black book: pages flip, names pop out as hearts */
        int bx = cx - 20 * s, by = cy - 6 * s, flip = (t / 14) & 1;
        plat_rect(bx, by, 40, 28, PAL_BLACK);
        plat_rect(bx + 2, by + 2, 17 + flip, 24, PAL_RAMP(RAMP_BONE, 6));
        plat_rect(bx + 21 + flip, by + 2, 17 - flip, 24, PAL_RAMP(RAMP_BONE, 5));
        for (k = 0; k < 4; k++) {                                        /* scribbled names */
            plat_rect(bx + 5, by + 6 + k * 5, 10 + (k * 5 + t / 14) % 7, 1, PAL_BLACK);
            plat_rect(bx + 24, by + 6 + k * 5, 8 + (k * 3 + t / 14) % 9, 1, PAL_BLACK);
        }
        plat_rect(bx + 30, by - 6, 3, 10, PAL_RAMP(RAMP_BLOOD, 3));      /* the pencil */
        gg_shout(hx - 90, hy - 4, names[(t / 40) % 5], PAL_UI_PINK);
        for (i = 0; i < 5; i++)
            ui_heart(bx + 6 + i * 9 + ui_wave((int)ct + i * 7, 12, 3), by - 10 - (int)((ct + (uint32_t)i * 13) % 50u), 2 + (i & 1), PAL_UI_PINK);
        if (t > 110 && (t % 18) < 9) gg_shout(hx + 40, hy, "mmm~", PAL_UI_PINK);
    } else if (panel == 17) {
        /* the queue: residents shuffle forward along the floor, a ticket machine dings */
        for (i = 0; i < 6; i++) {
            int px = g.sw - 14 * s - i * 20 * s - (int)(ct / 40u % 2u) * 2;
            citizen(crowd_ids[(i + 2) % 6], px, g.floor + 10 * s + ui_wave((int)ct * 2 + i * 6, 12, 1), s, 1, (int)((ct / 8 + (uint32_t)i) & 3), 1);
            if (((i + t / 20) & 3) == 0) ui_heart(px, g.floor - 20 * s - (int)((ct + (uint32_t)i * 9) % 12u), 2, PAL_UI_PINK);
        }
        plat_rect(g.sw - 62, 16, 50, 22, PAL_BLACK);
        plat_rect(g.sw - 60, 18, 46, 18, PAL_RAMP(RAMP_BONE, 6));
        gg_shout(g.sw - 58, 20, nums[DOOMO_MIN(t / 70, 2)], PAL_UI_RED);
        if ((t % 70) > 4 && (t % 70) < 24) gg_shout(g.sw - 76, 4, "DING!", PAL_UI_GOLD);
        if (t > 100 && (t % 50) < 25) gg_shout(hx - 90, hy, "next~", PAL_UI_PINK);
    } else if (panel == 18) {
        /* the pump goes down the line: a new pair of hands every few strokes */
        int stroke = ui_wave((int)ct * 7, 20, 6), who = (t / 40) % 4;
        hose(ax + 4, ay, g.sw - 40, g.floor - 10, s, 5, ct);
        plat_rect(g.sw - 42, g.floor - 12, 6, 14 + 6 * s, PAL_BLACK);
        plat_rect(g.sw - 46, g.floor - 18 + stroke, 14, 3, PAL_BLACK);          /* the handle */
        citizen(crowd_ids[who], g.sw - 24, g.floor + 12 * s, s, 1, 0, 1);
        valve(ax + 2, ay, s, 0);
        if ((t % 20) > 10 && (t % 20) < 18) gg_shout(g.sw - 62, g.floor - 40, "pff!", PAL_WHITE);
        if ((t % 40) > 8 && (t % 40) < 34) gg_shout(hx - 100, hy - 6, wrong[who], PAL_UI_PINK);
        hearts(g.sw / 3, g.floor - 50 * s, 4, ct);
        if (t > 190) gg_shout(hx + 30, hy - 20, "don't stop~", PAL_UI_PINK);
    } else {
        /* dawn: heaps of sleepers, bunting, shoes, a tally on the fence, a smug gazebo */
        plat_rect(4, g.floor - 46 * s / 2 - 10, 74, 5, PAL_RAMP(RAMP_WOOD, 3));
        plat_rect(4, g.floor - 46 * s / 2 - 10, 3, 46 * s / 2 + 10, PAL_RAMP(RAMP_WOOD, 2));
        plat_rect(75, g.floor - 46 * s / 2 - 10, 3, 46 * s / 2 + 10, PAL_RAMP(RAMP_WOOD, 2));
        for (k = 0; k < DOOMO_MIN(47, t / 4); k++)                       /* forty-seven */
            plat_rect(7 + (k % 16) * 4, g.floor - 46 * s / 2 - 6 + (k / 16) * 8, 1, 6, PAL_BLACK);
        for (i = 0; i < 3; i++) plat_rect(g.sw - 60 + i * 18, g.floor + 2 - (i & 1) * 2, 5, 3, PAL_BLACK);   /* abandoned shoes */
        plat_rect(g.sw - 60, g.floor - 36, 44, 4, PAL_RAMP(RAMP_WOOD, 4));      /* the smug gazebo */
        plat_rect(g.sw - 54, g.floor - 42, 32, 6, PAL_RAMP(RAMP_WOOD, 5));
        plat_rect(g.sw - 46, g.floor - 48, 16, 6, PAL_RAMP(RAMP_WOOD, 5));
        plat_rect(g.sw - 56, g.floor - 32, 3, 32, PAL_RAMP(RAMP_WOOD, 2));
        plat_rect(g.sw - 23, g.floor - 32, 3, 32, PAL_RAMP(RAMP_WOOD, 2));
        ui_heart(g.sw - 38, g.floor - 60 - ui_wave((int)ct * 2, 16, 3), 3, PAL_UI_PINK);
        if (t > 60) gg_shout(hx - 60, hy, "mmm.", PAL_UI_PINK);
        if (t > 160 && (t % 40) < 20) gg_shout(hx - 90, hy - 16, "...brunch?", PAL_UI_GOLD);
    }
}

/* ====================================================================== */
/* the feast proper (panels 0-9)                                            */
void feast_draw(int panel, uint32_t pt, uint32_t ct)
{
    Stage g;
    int i, t = (int)pt, s, cx, yf, swell = swell_at(panel, pt), cute = F_CUTE;
    GgParams p;

    gg_set_quality(plat_screen_h() >= 200 ? 1 : 0);                /* the live parts: four tones on the PS1, three on the GBA */
    if (panel == 10 || panel == 11) { epilogue(panel, pt, ct); return; }
    if (panel == 12 || panel == 13) { pump_scene(panel, pt, ct); return; }
    if (panel >= 14 && panel <= 19) { night_scene(panel, pt, ct); return; }
    stage(&g);
    s = g.s;
    cx = g.cx;
    yf = g.yf;
    gg_params_init(&p);
    sp_idle(&p, ct, s);
    p.swell = swell;
    p.seams = s > 1;

    if (panel == 20) {                                   /* title card, then the memorial */
        plat_rect(0, 0, g.sw, g.sh, PAL_BLACK);
        if (pt > 10) {
            int sc = g.sh >= 200 ? 3 : 2;
            ui_text_big((g.sw - ui_text_big_width("What an Ass", sc)) / 2, g.sh / 3 - 12, "What an Ass", sc,
                        PAL_UI_GOLD, PAL_RAMP(RAMP_BLOOD, 2));
        }
        if (pt > 50) {                                   /* oversized headstone + plaque, and him peeking round it */
            int hx = g.sw / 2 - 40, hy = g.sh / 2 + 6;
            ui_round_rect(hx, hy, 80, 56, 18, PAL_RAMP(RAMP_STONE, 4));
            ui_round_rect(hx + 3, hy + 3, 74, 53, 16, PAL_RAMP(RAMP_STONE, 5));
            plat_rect(hx + 12, hy + 34, 56, 12, PAL_RAMP(RAMP_STEEL, 3));
            ui_heart(hx + 40, hy + 18, 6, PAL_UI_PINK);
            gg_ellipse(hx + 40, hy - 6 + ui_wave((int)ct, 40, 2), 14, 3, PAL_UI_GOLD);   /* halo */
            if (pt > 80) {
                p.layers = GGL_FRONT | GGL_FACE;
                p.bob = -(int)DOOMO_MIN(pt - 80, 20u) * s;
                gg_draw(hx + 80 + 20 * s, g.sh + 60 * s + 14 * s, s, GGV_FRONT, GGP_STAND, GGE_WINK, &p);
            }
        }
        return;
    }

    /* ---- full-screen cuts ---- */
    if (panel == 3 && pt >= CU_PUSH_A && pt < CU_REAR_B) {
        uint32_t t0 = pt - (pt < CU_PUSH_B ? CU_PUSH_A : pt < CU_REAR_A ? CU_FACE_A : CU_REAR_A);
        if (t0 < 2) { plat_rect(0, 0, g.sw, g.sh, PAL_WHITE); return; }      /* the smash cut */
        if (pt < CU_PUSH_B) push_shot(t0); else if (pt < CU_REAR_A) face_shot(t0); else rear_shot(t0);
        return;
    }
    if (panel == 4 && pt >= CU_BURP_A && pt < CU_BURP_B) { if (pt - CU_BURP_A < 2) plat_rect(0, 0, g.sw, g.sh, PAL_WHITE); else burp_shot(pt - CU_BURP_A, ct); return; }
    if (panel == 5 && pt >= CU_GAS_A && pt < CU_GAS_B) { if (pt - CU_GAS_A < 2) plat_rect(0, 0, g.sw, g.sh, PAL_WHITE); else gas_shot(pt - CU_GAS_A, ct); return; }
    if (panel == 7 && pt >= CU_BELLY_A && pt < CU_BELLY_B) { if (pt - CU_BELLY_A < 2) plat_rect(0, 0, g.sw, g.sh, PAL_WHITE); else belly_shot(pt - CU_BELLY_A, ct); return; }
    if (panel == 7 && pt < CU_XRAY_B) { xray_shot(&g, pt, ct); return; }
    if (angle_shot(panel, pt, ct, swell)) return;

    /* ---- the wide shot: the hall, the table, the crowd ---- */
    {
        int cmood = panel <= 0 ? GG_CROWD_CALM : panel <= 3 ? GG_CROWD_SWOON : panel <= 5 ? GG_CROWD_SHOCK : panel <= 7 ? GG_CROWD_CHEER : GG_CROWD_SHOCK;
        if (panel == 3 && pt < CU_PUSH_A) cmood = GG_CROWD_SHOCK;
        hall_wide(&g, GG_HALL_DUSK, panel < 2 ? 5 - (int)DOOMO_MIN(ct / 60, 5u) : 0, cmood, 0, ct);
    }
    if (panel < 8) gg_chair(cx + 30 * s, g.floor - 2 * s, 24 * s, (panel == 7 && pt > 90) ? (int)(pt - 90) / 6 : 0);

    /* ---- him ---- */
    if (panel == 0) {
        /* butt to camera at the table, hips swaying on every bite; a wink back twice */
        int wink = (pt >= 50 && pt < 72) || (pt >= 110 && pt < 132);
        sp_motion(&p, 24, 6, 2 * s, t, ct, s, 1);
        p.swell = swell;
        p.bob += ui_wave((int)ct, 48, 1);
        gg_draw_cached(cx, yf, s, wink ? GGV_BACK_LOOK : GGV_BACK, GGP_STAND, GGE_WINK, &p);
        fullness(&g, 40 + swell);
        if ((pt % 24) >= 6 && (pt % 24) < 14) gg_shout(cx + 20 * s, yf - 84 * s, "mmm~", PAL_UI_PINK);
        if (wink) ui_sparkle(cx - 14 * s, yf - 62 * s, (int)((ct / 2) & 15), PAL_WHITE);
        gg_bubble(6, 24, 0, "Let him eat?", -1);
    } else if (panel == 1) {
        /* every eye on him: he looks back, licking his lips; the exes pretend not to look */
        int lick = (t / 30) & 1;
        sp_motion(&p, 24, 6, 2 * s, t, ct, s, 1);
        p.swell = swell;
        gg_draw_cached(cx, yf, s, GGV_BACK_LOOK, GGP_STAND, lick ? GGE_SULTRY : GGE_SMUG, &p);
        fullness(&g, 40 + swell);
        if (pt >= 40 && pt < 100) gg_shout(g.sw - 120, 30, "*wolf whistle*", PAL_UI_GOLD);
        hearts(g.sw / 2 + 50 * s, g.floor - 30 * s, 3, ct);
        {   /* a blown kiss drifting at the crowd */
            int kx = cx + 10 * s + (int)(ct % 48u) * 2, ky = yf - 60 * s - ui_wave((int)ct * 2, 24, 4);
            ui_heart(kx, ky, 3, PAL_UI_PINK);
            if ((ct % 48u) < 8) gg_shout(cx + 4 * s, yf - 76 * s, "mwah", PAL_UI_PINK);
        }
    } else if (panel == 2) {
        /* the line: a record scratch, an eyebrow wiggle at YOU, then he turns round, thighs wide */
        int turned = pt >= 70;
        sp_idle(&p, ct, s);
        p.swell = swell;
        if (pt >= 40 && !turned) gg_draw_cached(cx, yf, s, GGV_BACK_LOOK, GGP_STAND, ((pt / 8) & 1) ? GGE_WINK : GGE_SMUG, &p);
        else if (turned) gg_draw_cached(cx, yf, s, GGV_FRONT, GGP_STAND, ((pt / 12) & 1) ? GGE_SULTRY : GGE_SMUG, &p);
        else gg_draw_cached(cx, yf, s, GGV_BACK, GGP_STAND, GGE_GRIN, &p);
        citizen(F_CULPRIT, cx + 44 * s, g.floor + 10 * s, s, 1, 0, 1);
        if (pt < 70) { gg_bubble(cx + 28 * s, 18, 0, "Look who's full of", 1); gg_bubble(cx + 28 * s, 32, 0, "himself, now?", 1); }
        else gg_bubble(cx - 70 * s, 18, 0, "Born this way, sugar.", 1);
        if (pt >= 40 && pt < 64) gg_shout(cx + 40 * s, 50, "*SCRTCH*", PAL_UI_GOLD);
        if (turned && (t / 10) & 1) gg_shout(cx - 60 * s, yf - 80 * s, "form a line, loves~", PAL_UI_PINK);
    } else if (panel == 3) {
        /* the hose: side-on, the culprit behind him with the coil; then the SLAP */
        int slapped = pt >= CU_REAR_B;
        sp_idle(&p, ct, s);
        p.swell = swell;
        p.dir = -1;
        if (slapped) {
            int q = t - (int)CU_REAR_B;
            p.ly = sp_ring(q - 4, 6 * s, 10, 8);
            p.ry = -p.ly / 2;
            gg_draw_cached(cx, yf, s, GGV_BACK_LOOK, GGP_STAND, q < 30 ? GGE_YELP : GGE_LOVE, &p);
            if (q >= 4) {                                                        /* the hand print on a cheek */
                int px = cx + gg_butt_r(s, GGP_STAND, swell) * 7 / 10, py = yf - 26 * s + gg_butt_r(s, GGP_STAND, swell) / 2 + p.ry;
                uint8_t pc = q < 40 ? PAL_RAMP(RAMP_CANDY, 6) : PAL_RAMP(RAMP_CANDY, 4);
                gg_ellipse(px, py, 4 * s, 3 * s, pc);
                for (i = 0; i < 4; i++) gg_ellipse(px + 5 * s, py - 4 * s + i * 3 * s, 3 * s, s, pc);
                if (q < 10) gg_burst(px + 6 * s, py, 8 * s + (10 - q), q < 4 ? PAL_WHITE : PAL_UI_GOLD);
                for (i = 0; i < 3; i++) if (q < 24) plat_line(px + 14 * s + i * 3, py - 6 + i * 6, px + 20 * s + i * 3, py - 6 + i * 6, PAL_WHITE);
            }
            if (q > 2 && q < 44) { int sc = ui_pop_scale(q - 2); if (sc > 0) ui_text_big(cx + 30 * s, DOOMO_MAX(yf - 90 * s, 4), "SLAP!", 2, PAL_WHITE, PAL_UI_RED_D); }
            citizen(F_CULPRIT, cx + 48 * s + DOOMO_MIN(q, 10), g.floor + 10 * s, s, 1, 0, 1);
            if (q > 14) {                                                        /* three hands go up */
                for (i = 0; i < 3; i++) gg_hand(g.sw / 2 - 50 * s + i * 40 * s, g.floor - 30 * s - DOOMO_MIN(q - 14, 12) * s + (i & 1) * 4, s, 0, PAL_RAMP(RAMP_PEACH, 4 + i));
                if ((q / 8) & 1) gg_shout(g.sw / 2 - 40, g.floor - 56 * s, "me next!", PAL_UI_GOLD);
            }
        } else {
            gg_draw_cached(cx, yf, s, GGV_SIDE, GGP_STAND, pt < 20 ? GGE_SMUG : GGE_SHOCK, &p);
            citizen(F_CULPRIT, cx + 44 * s, g.floor + 10 * s, s, 1, (int)((pt / 6) & 3), 1);
            for (i = 0; i < 5; i++) gg_ellipse(cx + 70 * s, g.floor + 8 * s - i * 3 * s, 14 * s, 4 * s, PAL_RAMP(RAMP_MINT, 3 + (i & 1)));   /* the coil */
            hose(cx + 60 * s, g.floor - 2 * s, cx + 22 * s, yf - 22 * s, s, 4, ct);
            if (pt >= 10) gg_bubble(cx + 10 * s, 14, 0, "Is that a hose...?", -1);
            if (pt >= 24) gg_shout(cx - 40 * s, yf - 86 * s, "*squeak*", PAL_UI_GOLD);
        }
    } else if (panel == 4) {
        /* the burp: he holds it like a moan; the imp winks back */
        sp_idle(&p, ct, s);
        p.swell = swell;
        p.bob += pt >= 20 && pt < 30 ? -(int)(pt - 20) / 3 : 0;
        gg_draw_cached(cx, yf, s, GGV_FRONT, GGP_STAND, pt < 30 ? GGE_SUCK : pt < 80 ? GGE_SULTRY : GGE_WINK, &p);
        citizen(ID_SOOTKIN, cx - 50 * s, g.floor + 10 * s, s, 2, 0, 1);
        if (pt >= CU_BURP_B) gg_gas(cx - 20 * s - (int)(pt - CU_BURP_B), yf - 70 * s - (int)(pt - CU_BURP_B) / 3, 6 + (int)(pt - CU_BURP_B) / 5, cute, ct);
        if (pt >= CU_BURP_B && pt < 90) gg_shout(cx - 50 * s, yf - 86 * s, "pardon me.", PAL_UI_PINK);
        if (pt >= 80) { ui_heart(cx - 44 * s, g.floor - 30 * s - (int)((ct * 2) % 20u), 3, PAL_UI_PINK); gg_shout(cx - 60 * s, g.floor - 46 * s, "*wink*", PAL_UI_GOLD); }
    } else if (panel == 5) {
        /* the fart: lips parted, he rises an inch off the chair */
        sp_idle(&p, ct, s);
        p.swell = swell;
        p.dir = -1;
        if (pt >= 40) p.bob -= 6 * s;
        if (pt < 40) p.bob += (int)(pt / 8) & 1 ? 1 : -1;
        gg_draw_cached(cx, yf, s, GGV_SIDE, GGP_STAND, pt < 40 ? GGE_SUCK : GGE_LOVE, &p);
        if (pt >= 40) {
            int q = (int)(pt - 40);
            for (i = 0; i < 3; i++) gg_gas(cx + 20 * s + i * 10 * s + q / 2, yf - 22 * s + i * 4 * s, 5 + q / 6 + i, cute, ct + (uint32_t)i * 9);
            gg_shout(cx + 20 * s, yf - 44 * s, "*pffrrt*", PAL_UI_GOLD);
            hearts(cx + 30 * s, yf - 60 * s, 2, ct);
        } else gg_shout(cx - 30 * s, yf - 84 * s, "mmm~", PAL_WHITE);
        if (pt >= 96) gg_shout(cx - 40 * s, yf - 84 * s, "a love letter~", PAL_UI_PINK);
    } else if (panel == 6) {
        /* both ends at once: the big one swells, then PFFT */
        sp_idle(&p, ct, s);
        p.swell = swell;
        p.bob += (pt >= 40 && pt < 50) ? (((int)ct & 1) ? 3 : -3) : 0;
        p.belly_y += pt < 50 ? ui_wave((int)ct * 8, 8, 1 + (int)pt / 20) : 0;
        gg_draw_cached(cx, yf, s, GGV_FRONT, GGP_STAND, pt < 50 ? GGE_BLUSH : pt < 104 ? GGE_RELIEF : GGE_SULTRY, &p);
        if (pt >= 50 && pt < 110) {
            int q = (int)(pt - 50);
            gg_gas(cx - 24 * s - q / 2, yf - 70 * s - q / 3, 8 + q / 4, cute, ct);
            gg_gas(cx + 36 * s + q / 2, yf - 14 * s, 8 + q / 4, cute, ct + 9);
            gg_shout(cx - 30 * s, yf - 86 * s - q / 4, "ohhh, finally~", PAL_UI_PINK);
        }
        if (pt > 110) { smoke(cx - 60 * s, g.floor - 30 * s, 30, ct); gg_shout(cx - 80 * s, g.floor - 50 * s, "afterparty?", PAL_UI_GOLD); }
    } else if (panel == 7) {
        /* the valve wins: the belly swells past the gauge, the harness groans */
        sp_idle(&p, ct, s);
        p.swell = swell;
        p.bob += (int)((ct * 5) % 5) - 2 + (int)pt / 40;
        p.belly_y += ui_wave((int)ct * 9, 6, 1 + swell / 120);
        p.no_harness = pt > CU_BELLY_B;
        if (pt > 60) p.bob -= (int)(pt - 60) / 10;                                  /* parade balloon */
        gg_draw_cached(cx, yf, s, GGV_FRONT, GGP_STAND, pt > 130 ? GGE_YELP : GGE_BLUSH, &p);
        hose(cx + 30 * s, yf - 26 * s, g.sw - 10, g.floor + 6 * s, s, 6, ct);
        valve(cx + 30 * s, yf - 28 * s, s, 0);
        if (pt > 40) gg_shout(cx - 34, yf - 92 * s, "...gurgle", PAL_WHITE);
        if (pt > 60 && pt < 90) gg_shout(cx + 10 * s, yf - 100 * s, "don't stop~", PAL_UI_PINK);
        if (pt > 120) {                                                              /* the pressure gauge */
            gg_ellipse(g.sw - 30, 44, 12, 12, PAL_WHITE);
            gg_ellipse(g.sw - 30, 44, 11, 11, PAL_UI_CREAM);
            plat_line(g.sw - 30, 44, g.sw - 30 + (int)DOOMO_MIN(pt - 120, 28u) / 2 - 2, 36, PAL_UI_RED);
            ui_text_outline(g.sw - 44, 58, "MAX", PAL_UI_RED, PAL_BLACK);
        }
        for (i = 0; i < 3; i++) sweat(cx + 14 * s + i * 4, yf - 66 * s, ct, i * 5);
    } else if (panel == 8) {
        /* POP: flash, shockwave, the pieces, the rump off round the park, the belly last */
        int bcy = yf - 46 * s, rx, ry, k;
        if (t < 6) { plat_rect(0, 0, g.sw, g.sh, PAL_WHITE); return; }
        if (t < 26) {                                    /* shockwave ring */
            int rad = t * 7;
            for (k = 0; k < 24; k++) {
                int a = k * 171, a2 = (k + 1) * 171;
                plat_line(cx + FIX2INT(fx_cos(a) * rad), bcy + FIX2INT(fx_sin(a) * rad) * 3 / 4,
                          cx + FIX2INT(fx_cos(a2) * rad), bcy + FIX2INT(fx_sin(a2) * rad) * 3 / 4,
                          t < 14 ? PAL_WHITE : PAL_UI_GOLD);
            }
        }
        for (i = 0; i < 40; i++) {
            int a = i * 163 + 40, rad = DOOMO_MIN(t * (2 + i % 5), 110 * s);
            int x = cx + FIX2INT(fx_cos(a) * rad), y = bcy + FIX2INT(fx_sin(a) * rad) + t * t / 40;
            if (y > g.floor + 8 * s) y = g.floor + 8 * s - ((y - g.floor) % 10);          /* bounce off the floor */
            if (cute) {
                if (i % 5 == 0) ui_heart(x, y, 3, PAL_UI_PINK);
                else if (i % 3 == 0) gg_ellipse(x, y, 3, 2, PAL_RAMP(RAMP_BONE, 6));      /* plush stuffing */
                else plat_rect(x, y, 3, 3, PAL_PAINT(i & 31, 2));                          /* confetti       */
            } else {
                plat_rect(x, y, 3 + i % 3, 3 + i % 2, i % 4 ? PAL_RAMP(RAMP_BLOOD, 3 + i % 3) : PAL_RAMP(RAMP_FLESH, 3));
            }
        }
        if (!cute) plat_checker(0, 0, g.sw, g.floor, PAL_RAMP(RAMP_BLOOD, 3));
        /* the rump is the last thing to go: it tears off round the park like a let-go
         * balloon, trailing puffs (hearts on CUTE), then flops down and sits there */
        {
            int rad = (12 - DOOMO_MIN(t / 12, 5)) * s;
            for (k = 5; k >= 1; k--) {
                int tt = t - k * 4, qx, qy;
                if (tt < 6 || t >= 96) continue;
                rump_pos(tt, cx, bcy, g.floor + 8 * s, &qx, &qy);
                if (cute) ui_heart(qx + 8, qy + 4, 2, PAL_UI_PINK); else gg_gas(qx + 8, qy + 4, 2 + (5 - k) / 2, 0, ct);
            }
            rump_pos(t, cx, bcy, g.floor + 8 * s, &rx, &ry);
            gg_blob(rx - rad * 2 / 3, ry, rad, rad, RAMP_BLOOD, 0);
            gg_blob(rx + rad * 2 / 3, ry, rad, rad, RAMP_BLOOD, 0);
            plat_rect(rx, ry - rad / 2, 1, rad, GG_LINE);
            if (t < 90 && ((t / 6) & 1)) gg_shout(rx - 22, ry - rad - 12, "pbbbbt~", PAL_WHITE);
            if (t >= 90) gg_shout(rx - 12, ry - rad - 12, "plap.", PAL_WHITE);
        }
        if (cute) gg_ellipse(cx, bcy - 30 * s + ui_wave((int)ct, 30, 3), 14, 3, PAL_UI_GOLD);   /* his halo */
        if (!cute && t > 40) {                                                       /* the belly lands last */
            int by = -40 + (t - 40) * (t - 40) / 8;
            if (by > g.floor - 10 * s) by = g.floor - 10 * s;
            gg_blob(cx, by, 22 * s, by >= g.floor - 10 * s ? 14 * s : 18 * s, RAMP_PEACH, 0);
            if (by >= g.floor - 10 * s && t < 110) gg_shout(cx - 20, by - 30 * s, "SPLAT.", PAL_UI_GOLD);
        }
        for (i = 0; i < 3; i++) citizen(crowd_ids[i], DOOMO_CLAMP(rx - 30 - i * 14 * s + (t % 7), 10, g.sw - 10), g.floor + 10 * s, s, 2, (int)((ct / 4 + (uint32_t)i) & 3), 1);   /* the chase */
        if (t > 60) gg_shout(cx - 50, g.floor - 30, "gurgle...", PAL_WHITE);
    } else if (panel == 9) {
        /* "...that's hot." then the dust-cloud brawl */
        if (t < 30) {                                    /* the Said-It-Out-Loud citizen, red-faced */
            citizen(ID_VEX, g.sw - 60, g.floor + 10 * s, s, 0, 0, 1);
            gg_bubble(g.sw - 120, 30, 0, "...that's hot.", 1);
        } else {
            for (i = 0; i < 6; i++)
                gg_ellipse(g.sw - 70 * s + (i * 13 * s) % (50 * s), g.floor - 10 * s + (i * 7) % 18 + ui_wave(t + i * 5, 6, 2), 12 * s, 10 * s, PAL_RAMP(RAMP_STONE, 5 + (i & 1)));
            for (i = 0; i < 3; i++) gg_hand(g.sw - 80 * s + i * 28 * s, g.floor - 24 * s - ui_wave(t * 3 + i * 7, 8, 4), s, 0, PAL_RAMP(RAMP_PEACH, 4 + i));
            for (i = 0; i < 4; i++) ui_sparkle(g.sw - 60 + i * 14, g.floor - 40 - (i * 9) % 10, (t / 2 + i * 3) & 15, PAL_UI_GOLD);
            if (t > 60) gg_ellipse(g.sw - 20, g.floor - 70 - (t - 60) * 2 + (t - 60) * (t - 60) / 20, 10, 2, PAL_UI_GOLD);   /* halo flies out */
            gg_shout(g.sw - 90, 24, "(instantly mobbed)", PAL_UI_CREAM);
        }
        gg_blob(cx - 8 * s, g.floor + 2 * s, 8 * s, 8 * s, RAMP_BLOOD, 0);     /* what's left of him, resting */
        gg_blob(cx + 8 * s, g.floor + 2 * s, 8 * s, 8 * s, RAMP_BLOOD, 0);
        plat_rect(cx, g.floor - 4 * s, 1, 10 * s, GG_LINE);
        if (cute) gg_ellipse(cx, g.floor - 18 * s + ui_wave((int)ct, 30, 2), 10, 2, PAL_UI_GOLD);
    }
}
