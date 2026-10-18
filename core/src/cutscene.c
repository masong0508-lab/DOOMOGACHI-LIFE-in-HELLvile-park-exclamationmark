/* cutscene.c - SCENE_CUTSCENE (arg = CUT_*): chapter title cards and the
 * story's animated vignettes (owner: story; rebuilt on beat.h by S1 M4,
 * docs/design/story/SPEC.md sec.7 S1, story_final.md sec.15.7,
 * comic_timing.md sec.6.1).
 *
 * A cutscene is a STAGE plus a BeatScript. The stage is the vignette's
 * drawing code (backdrop, actors, camera), which reads the current shot
 * (`panel`, switched by BOP_SHOT) and the shot's age (`pt`) for every
 * entrance, and the camfx state for everything else. The script owns all
 * timing: shots, captions, music, SFX, shakes, flashes and the iris hang
 * off its labels, so re-timing a beat drags its sound and shake along
 * (no frame constants in draw code). Each shot waits for A (after R13-ish
 * 12 frames) or moves on by itself (BEAT_WAIT_A_B); START skips the whole
 * cutscene (beat_skip applies the flags only); A never shortens a HOLD
 * after L_PUNCH (beat.c). The host kinds (SHOT, CAPTION, IRIS) come back
 * from beat_tick() as BT_EXT and are handled here (SPEC C6).
 * CUT_FEAST_POP keeps calling feast_tick / feast_draw (story_feast.c)
 * with its exact panel lengths until S5 ports it onto stages.
 */
#include "story_int.h"
#include "story_ch12.h"          /* S2: the Ch1-2 beat stages (cutscene_ch12.c) */
#include "grumbleguts_gfx.h"
#include "doomo/camfx.h"
#include "doomo/tween.h"
#include "doomo/beat.h"

/* ---- captions (one per shot; NULL = none) ---------------------------------------- */
static const char *const C_none[] = { NULL };
static const char *const C_arrival[] = {
    "A sleepy pink bus rolls over the candy-coloured hills...",
    "Welcome to HaHaland: the happiest place on Earth!",
    "Population: 500 lovely citizens. And now, @P!"
};
static const char *const C_home[] = {
    "Home sweet home! Your very own cottage on Hillvile Lane.",
    "The Doomogachi egg is wiggling... it's warm... it's HATCHING!"
};
static const char *const C_fog[] = {
    "Purple fog pours over the Wall like spilled grape soda.",
    "Two little lights blink at you from inside it.",
    "\"Hee hee hee... Hillvile... come and PLAY with us...\""
};
static const char *const C_eruption[] = {
    "Somewhere beyond the Wall, an enormous vape cloud starts to swell...",
    "It grows. And grows. It blots out the pink sky...",
    "...and with a sound like the world's biggest cough, it tears the gate WIDE OPEN.",
    "THE DEMONS OF HELL ARE HERE."
};
static const char *const C_vapula[] = {
    "Lord Vapula, Duke of the Ninth Cloud, wheezes his last mighty wheeze...",
    "...and deflates. Pfffffffff. With a sad little squeak.",
    "The vape cloud dissolves. The sky blushes pink again."
};
static const char *const C_wall[] = {
    "The Wall creaks... groans... and comes tumbling down.",
    "For the first time, Hillvile and Hellvile see each other.",
    "And then, all at once, everybody runs to say hello."
};
static const char *const C_freeplay[] = {
    "FREE PLAY UNLOCKED! Visit the united park, Hellvile and HaHaland any time.",
    "Thank you for playing, @P. Hellvile Park is better with you in it."
};
/* The Feast of Rebuilding, outcome 1 ("What an Ass"). Slapstick on his
 * vanity and his appetite, cartoon-modest (SF sec.3, R15): innuendo at most,
 * nothing sexual. The panel lengths are story_feast.c's exact timings. */
static const char *const C_feast[] = {
    "Hellvile Park, the Feast of Rebuilding. Gatekeeper Grumbleguts eats with his back to the whole park, so everybody gets the full view. He calls it sharing.",
    "Every eye in the park is on one thing, and every course makes it bigger. His exes Shriekra and Spearmint pretend not to look. Nana fans herself with a menu.",
    "\"Look who's full of himself, now?\" The guest beside him jerks a thumb at the seat of the problem. Grumbleguts beams. \"Born this way, sugar.\"",
    "A hose, impossibly long, fed in hand over hand like a rope climb in reverse. \"Surely that's enough?\" ...It is not. It keeps going. A mechanic's pat. One big SLAP. \"She'll hold.\"",
    "The first false alarm: a burp so big it rattles the bunting. \"Pardon me.\" He does not look sorry. He looks hungry.",
    "...pfffrrrt. The second false alarm lifts him an inch off the chair. Minty, he insists. Nobody believes him.",
    "Both ends at once. He sags. The crowd cheers: it's over! ...It is not over. The valve is still turning.",
    "The valve wins. The belly swells, the straps creak, the chair files a complaint. Every camera in the park finds the same angle. By accident. Again.",
    "POP. Confetti, stuffing and one very surprised halo. High above the bunting a lone pair of cheeks drifts down like a balloon. \"...Is there dessert?\"",
    "\"...that's hot,\" says one voice in the silence. Every head turns. The mob is swift, cartoonish and thorough. A single shoe lands in frame.",
    "Later. Moonlight, candles, the crowd lingering by the empty tables. Grumbleguts is back in one piece, mostly, reclined and glowing, digesting at volume.",
    "He finds @P in the crowd and holds the look. \"Don't be shy, sugar. There's room at my table. There's always room at my table.\"",
    "Later still, he hauls out a bike pump. \"For my posture.\" The hose goes round the back. The handle goes up and down. Pff. Pff.",
    "Pff. Pff. Pff. The belly is not so much round as ambitious. He is inflating himself like a parade balloon and has never been prouder. \"...one more.\"",
    "The valve gives a long, wobbling hiss and the whole park goes quiet to listen. Something deep in the belly gurgles back. It has opinions.",
    "BRRRAAAP. It rolls across the park like a tuba falling down the stairs. A brave imp pokes the belly. The belly pokes back.",
    "Out comes a little black book the size of a phone directory. \"Shriekra. Spearmint. The waiter, the band, the gazebo...\" Every page says NO in a different hand.",
    "A queue snakes round the park and somebody is handing out numbers. It is the queue for the leftovers. He is at the front of it. Twice.",
    "The pump gets passed down the line and everybody has a go. He calls every helper by the wrong name and thanks each one warmly.",
    "Dawn. Bunting, abandoned shoes and one very smug gazebo. Grumbleguts, flat and glowing, scratches a tally on the fence: forty-seven helpings. \"...Brunch?\"",
    NULL
};

/* ---- the beat scripts -------------------------------------------------------------
 * Shot k hangs off label k (label 0 = the start): SHOT, CAPTION, then a
 * WAIT_A that fires label k + 1 on A (after 12 frames) or after `dur`.
 * Effects of shot k are rows anchored to label k; L_PUNCH (30) carries
 * the punch beat where a cut has one. Rows are sorted by (anchor, at). */
#define EV(at, anc, op, a, b) { (uint16_t)(at), (uint8_t)(anc), (uint8_t)(op), (int16_t)(a), (int16_t)(b) }
#define SHOT(k, dur) EV(0, k, BOP_SHOT, k, 0), EV(0, k, BOP_CAPTION, 0, k), \
                     EV(0, k, BOP_WAIT_A, (k) + 1, BEAT_WAIT_A_B(12, dur))
#define SHOT_FIXED(k, dur) EV(0, k, BOP_SHOT, k, 0), EV(0, k, BOP_CAPTION, 0, k), \
                     EV(0, k, BOP_WAIT_A, (k) + 1, BEAT_WAIT_A_B(12, dur))
#define END_AT(k) EV(0, k, BOP_END, 0, 0)
#define MUSIC(at, k, s) EV(at, k, BOP_MUSIC, s, 0)
#define SFX(at, k, s) EV(at, k, BOP_SFX, s, 0)

static const BeatEv B_card1[] = { MUSIC(0, 0, SONG_TITLE), SHOT(0, 260), END_AT(1) };
static const BeatEv B_card2[] = { MUSIC(0, 0, SONG_VAPE_LOUNGE), SHOT(0, 260), END_AT(1) };
static const BeatEv B_card3[] = { MUSIC(0, 0, SONG_INVASION), SHOT(0, 260), SFX(2, 0, SFX_THUD), END_AT(1) };
static const BeatEv B_card4[] = { MUSIC(0, 0, SONG_HELLVILE), SHOT(0, 260), END_AT(1) };
static const BeatEv B_arrival[] = {
    MUSIC(0, 0, SONG_HAHALAND_DAY), SHOT(0, 250),
    SHOT(1, 250), SFX(2, 1, SFX_SPARKLE),
    SHOT(2, 250), SFX(13, 2, SFX_FANFARE),
    END_AT(3)
};
static const BeatEv B_home[] = {
    MUSIC(0, 0, SONG_PET), SHOT(0, 240),
    SHOT(1, 250), SFX(10, 1, SFX_EGG_CRACK), SFX(40, 1, SFX_EGG_CRACK), SFX(70, 1, SFX_EGG_CRACK),
    SFX(100, 1, SFX_EGG_CRACK), SFX(130, 1, SFX_EGG_CRACK), SFX(160, 1, SFX_EGG_CRACK),
    SFX(190, 1, SFX_EGG_CRACK), SFX(220, 1, SFX_EGG_CRACK),
    END_AT(2)
};
static const BeatEv B_fog[] = {
    MUSIC(0, 0, SONG_OMINOUS), SHOT(0, 250),
    SHOT(1, 250), SFX(3, 1, SFX_TIK),                         /* the two lights blink on */
    SHOT(2, 260), EV(0, 2, BOP_MUSIC_DUCK, 30, 140), SFX(4, 2, SFX_POSSESS),
    END_AT(3)
};
static const BeatEv B_eruption[] = {
    MUSIC(0, 0, SONG_OMINOUS), SHOT(0, 260),
    SHOT(1, 250),
    SHOT(2, 250), EV(7, 2, BOP_MUSIC_MUTE, 13, 0), EV(20, 2, BOP_LABEL, L_PUNCH, 0),
    SHOT(3, 270), EV(2, 3, BOP_SHAKE, SHK_ROAR, 0), SFX(2, 3, SFX_DEMON_ROAR),
    END_AT(4),
    /* the cough that tears the gate open: silence R13, then the FWOOMP */
    EV(0, L_PUNCH, BOP_SHAKE, SHK_FWOOMP, 0), EV(0, L_PUNCH, BOP_FLASH, PAL_RAMP(RAMP_LAVA, 6), 8 | (3 << 8)),
    SFX(0, L_PUNCH, SFX_EXPLODE), MUSIC(0, L_PUNCH, SONG_INVASION), EV(0, L_PUNCH, BOP_NOBUTTON, 0, 0)
};
static const BeatEv B_vapula[] = {
    MUSIC(0, 0, SONG_VICTORY), SHOT(0, 250),
    SHOT(1, 250), SFX(10, 1, SFX_DEMON_DIE), EV(13, 1, BOP_TAPESTOP, 8, 0), SFX(60, 1, SFX_SQUEAK),
    EV(60, 1, BOP_LABEL, L_PUNCH, 0),
    SHOT(2, 250), EV(0, 2, BOP_MUSIC_MUTE, 0, 0), SFX(0, 2, SFX_CONFETTI),
    END_AT(3),
    /* *squeak*: the Duke's last breath, then the button R13 later */
    EV(13, L_PUNCH, BOP_LABEL, L_BUTTON, 0), SFX(13, L_PUNCH, SFX_POP2)
};
static const BeatEv B_wall[] = {
    MUSIC(0, 0, SONG_FESTIVAL), SHOT(0, 250),
    SFX(8, 0, SFX_EXPLODE), SFX(24, 0, SFX_EXPLODE), EV(30, 0, BOP_SHAKE, SHK_RUMBLE, 0),
    EV(42, 0, BOP_LABEL, L_PUNCH, 0),
    SFX(56, 0, SFX_EXPLODE), SFX(72, 0, SFX_EXPLODE), SFX(88, 0, SFX_EXPLODE), SFX(104, 0, SFX_EXPLODE),
    SFX(120, 0, SFX_EXPLODE), SFX(136, 0, SFX_EXPLODE), SFX(152, 0, SFX_EXPLODE), SFX(168, 0, SFX_EXPLODE),
    SFX(184, 0, SFX_EXPLODE), SFX(200, 0, SFX_EXPLODE), SFX(216, 0, SFX_EXPLODE), SFX(232, 0, SFX_EXPLODE),
    SHOT(1, 250),
    SHOT(2, 260), SFX(50, 2, SFX_HAPPY),
    END_AT(3),
    /* the Wall hits the ground: never undercut (SF sec.7) */
    EV(0, L_PUNCH, BOP_SHAKE, SHK_IMPACT, 0), EV(0, L_PUNCH, BOP_FLASH, PAL_WHITE, 8 | (2 << 8)),
    SFX(0, L_PUNCH, SFX_EXPLODE), EV(0, L_PUNCH, BOP_NOBUTTON, 0, 0)
};
static const BeatEv B_freeplay[] = {
    MUSIC(0, 0, SONG_CREDITS), SHOT(0, 260),
    SHOT(1, 240), EV(200, 1, BOP_IRIS_OUT, 0, RUNG_R21 | (1 << 6)), SFX(200, 1, SFX_SHWIP),
    END_AT(2)
};
static const BeatEv B_feast[] = {
    MUSIC(0, 0, SONG_FESTIVAL),
    SHOT_FIXED(0, 150), SHOT_FIXED(1, 140), SHOT_FIXED(2, 110), SHOT_FIXED(3, 252),
    SHOT_FIXED(4, 100), SHOT_FIXED(5, 100), SHOT_FIXED(6, 130), SHOT_FIXED(7, 170),
    SHOT_FIXED(8, 130), MUSIC(8, 8, SONG_NONE),               /* silence after the pop */
    SHOT_FIXED(9, 110),
    SHOT_FIXED(10, 180), MUSIC(2, 10, SONG_HAHALAND_NIGHT),   /* the slow, candlelit epilogue */
    SHOT_FIXED(11, 170), SHOT_FIXED(12, 200), SHOT_FIXED(13, 170), SHOT_FIXED(14, 210),
    SHOT_FIXED(15, 220), SHOT_FIXED(16, 230), SHOT_FIXED(17, 240), SHOT_FIXED(18, 240),
    SHOT_FIXED(19, 230),
    SHOT_FIXED(20, 120), MUSIC(2, 20, SONG_OMINOUS),
    END_AT(21)
};

typedef struct CutDef {
    const BeatEv *ev; uint16_t n;
    const char *const *cap; uint8_t ncap;
    uint8_t dark;                  /* captions in WIN_DARK from this shot on (0xFF never) */
} CutDef;
#define CUTDEF(b, c, d) { b, (uint16_t)(sizeof(b) / sizeof(b[0])), c, (uint8_t)(sizeof(c) / sizeof(c[0])), d }
static const CutDef cuts[CUT_COUNT] = {
    CUTDEF(B_card1, C_none, 0xFF), CUTDEF(B_card2, C_none, 0xFF), CUTDEF(B_card3, C_none, 0xFF),
    CUTDEF(B_card4, C_none, 0xFF), CUTDEF(B_arrival, C_arrival, 0xFF), CUTDEF(B_fog, C_fog, 0),
    CUTDEF(B_eruption, C_eruption, 2), CUTDEF(B_vapula, C_vapula, 0xFF), CUTDEF(B_wall, C_wall, 0xFF),
    CUTDEF(B_freeplay, C_freeplay, 0xFF), CUTDEF(B_home, C_home, 0xFF), CUTDEF(B_feast, C_feast, 0xFF)
};

static const char *const cut_names[CUT_COUNT] = {
    "Chapter 1", "Chapter 2", "Chapter 3", "Chapter 4", "Arrival in HaHaland",
    "The Purple Fog", "The Gate Erupts", "Vapula Deflates", "The Wall Falls",
    "Free Play", "Home Sweet Home", "What an Ass"
};

const char *story_cutscene_name(int c) { return (c >= 0 && c < CUT_COUNT) ? cut_names[c] : ""; }

/* the script of a cutscene (tests: every one must be beat_script_valid) */
int cutscene_script(int c, BeatScript *out)
{
    if (c < 0 || c >= CUT_COUNT || !out) return 0;
    out->ev = cuts[c].ev;
    out->n = cuts[c].n;
    out->nlabels = BEAT_LABEL_MAX;
    return 1;
}

static int cut, panel, cap = -1;
int cutscene_showcase_script(int k, BeatScript *out);
static int showcase_arg(int arg);
static uint32_t ct, pt;            /* ticks in cutscene / in the current shot */
static uint16_t shot_t0, cap_t0;   /* beat_now() when the shot / caption began */
static int s2cut;                  /* S2: cutscene_ch12.c draws this one */

static void cs_end(void)
{
    if (showcase_arg(-1)) {                      /* the timing showcase just holds its last frame */
        return;
    }
    if (cut > CUT_CHAPTER4 && cut < CUT_COUNT) story_unlock_quiet(USTORY_CUT_FIRST + cut);
    if (scene_depth() > 1) scene_pop();
    else scene_set(&SCENE_STORY, STORY_ARG_CONTINUE);
}

static void cs_enter(int arg)
{
    BeatScript s;
    s2cut = 0;
    if (showcase_arg(arg)) return;
    if (cut12_owns(arg)) {                   /* --- S2: P0, the Bottle, F1, the Ch1-2 vignettes --- */
        s2cut = 1;
        cut = CUT_CHAPTER1;
        cap = -1;
        cut12_enter(arg);
        return;
    }
    cut = (arg >= 0 && arg < CUT_COUNT) ? arg : CUT_CHAPTER1;
    panel = 0;
    cap = -1;
    ct = pt = 0;
    shot_t0 = cap_t0 = 0;
    camfx_reset();
    cutscene_script(cut, &s);
    beat_start(&s);
}

/* the host kinds of beat.h (SPEC C6) */
static void cs_event(const BeatEv *e)
{
    switch (e->op) {
    case BOP_SHOT:
        panel = e->a;
        shot_t0 = (uint16_t)beat_now();
        pt = 0;
        if (panel > 0) audio_sfx(SFX_BLIP);
        break;
    case BOP_CAPTION:
        cap = e->b < cuts[cut].ncap && cuts[cut].cap[e->b] ? e->b : -1;
        cap_t0 = (uint16_t)beat_now();
        break;
    case BOP_IRIS_OUT:
        camfx_iris_out(plat_screen_w() / 2, plat_screen_h() / 2 - 20, plat_screen_h() / 4,
                       (e->b >> 6) & 1, e->b & 63);
        break;
    default: break;                          /* POP / EXPR / ACT: stages to come */
    }
}

static void cs_update(void)
{
    int r, guard = 64;
    if (s2cut) { if (cut12_update()) cs_end(); return; }   /* S2 */
    camfx_tick();                            /* post-FX clock first */
    ct++;
    if (btn_pressed(BTN_START)) { audio_sfx(SFX_CANCEL); beat_skip(); cs_end(); return; }
    if (btn_pressed(BTN_A) || btn_pressed(BTN_B)) beat_press_a();
    do {
        r = beat_tick();
        if (r == BT_EXT) cs_event(beat_ext_event());
    } while (r == BT_EXT && guard-- > 0);
    pt = (uint32_t)(uint16_t)(beat_now() - shot_t0);
    if (cut == CUT_FEAST_POP && !showcase_arg(-1)) feast_tick(panel, pt);
    if (r == BT_DONE) cs_end();
}

/* ---- drawing helpers ------------------------------------------------------------- */
static void sky_bands(uint8_t top, uint8_t mid, uint8_t bottom, int y1)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), h3 = y1 / 3;
    plat_rect(0, 0, sw, h3, top);
    plat_checker(0, h3, sw, 2, mid);
    plat_rect(0, h3 + 1, sw, h3, mid);
    plat_checker(0, 2 * h3, sw, 2, bottom);
    plat_rect(0, 2 * h3 + 1, sw, y1 - 2 * h3, bottom);
    (void)sh;
}

static void tile_tex(int tex, int x0, int y0, int w, int h, int scale)
{
    const DoomoImage *im = texture_image(tex, (uint16_t)ct);
    int x, y, step = 64 * scale;
    if (!im) return;
    for (y = y0; y < y0 + h; y += step)
        for (x = x0; x < x0 + w; x += step) plat_image(x, y, im, scale, 0, 0);
}

static void cloud(int cx, int cy, int r, uint8_t c, int checker)
{
    if (checker) {
        plat_checker(cx - r, cy - r / 2, r * 2, r, c);
    } else {
        ui_fill_circle(cx, cy, r, c);
        ui_fill_circle(cx - r, cy + r / 3, r * 2 / 3, c);
        ui_fill_circle(cx + r, cy + r / 3, r * 2 / 3, c);
    }
}

/* A soft cartoon cloud: coloured outline, 3-tone body, glossy top puffs,
 * gently breathing. r ~ half the width. */
static void puff_cloud(int cx, int cy, int r, int ramp, int bright, uint32_t t)
{
    static const int8_t px[6] = { -10, -5, 1, 7, 11, 0 }, py[6] = { 3, -3, -5, -2, 3, 4 };
    static const uint8_t pr[6] = { 6, 8, 9, 7, 5, 7 };
    uint8_t dark = PAL_RAMP(ramp, DOOMO_CLAMP(bright - 3, 0, 7));
    uint8_t mid = PAL_RAMP(ramp, DOOMO_CLAMP(bright - 1, 0, 7));
    uint8_t base = PAL_RAMP(ramp, DOOMO_CLAMP(bright, 0, 7));
    uint8_t light = PAL_RAMP(ramp, DOOMO_CLAMP(bright + 2, 0, 7));
    int i, w = ui_wave((int)t, 48, 1);
    for (i = 0; i < 6; i++)
        ui_fill_circle(cx + px[i] * r / 10, cy + py[i] * r / 10 + (i & 1 ? w : -w), pr[i] * r / 10 + 1, dark);
    for (i = 0; i < 6; i++)
        ui_fill_circle(cx + px[i] * r / 10, cy + py[i] * r / 10 + (i & 1 ? w : -w), pr[i] * r / 10, mid);
    for (i = 0; i < 5; i++)
        ui_fill_circle(cx + px[i] * r / 10 - r / 12, cy + py[i] * r / 10 - r / 10 + (i & 1 ? w : -w),
                       pr[i] * r / 10 * 3 / 4, base);
    for (i = 1; i < 4; i++)
        ui_fill_circle(cx + px[i] * r / 10 - r / 6, cy + py[i] * r / 10 - r / 4 + (i & 1 ? w : -w),
                       DOOMO_MAX(1, pr[i] * r / 30), light);
}

/* top edge of this shot's caption box (the pictures compose above it) */
int cutscene_caption_y(void)
{
    static int for_cut = -1, for_cap = -2, for_w, cap_y;
    char buf[280];
    int sw = plat_screen_w(), sh = plat_screen_h(), lines;
    if (cap < 0) return sh;
    if (for_cut == cut && for_cap == cap && for_w == sw) return cap_y;   /* measured once per caption */
    story_fmt(buf, sizeof(buf), cuts[cut].cap[cap]);
    lines = ui_text_wrap(0, 0, sw - 32, buf, 0, 0);
    for_cut = cut; for_cap = cap; for_w = sw;
    cap_y = sh - (lines * FONT_LINE + 12) - 6;
    return cap_y;
}

static void caption(void)
{
    char buf[280];     /* the Feast captions run to ~200 chars */
    int sw = plat_screen_w(), sh = plat_screen_h(), lines, h, y, style, age;
    uint32_t chars;
    if (cap < 0) return;
    if (cut == CUT_FEAST_POP && feast_front_shot(panel, pt)) return;     /* the front shot needs the whole screen */
    style = panel >= cuts[cut].dark ? WIN_DARK : WIN_CREAM;
    story_fmt(buf, sizeof(buf), cuts[cut].cap[cap]);
    lines = ui_text_wrap(0, 0, sw - 32, buf, 0, 0);
    h = lines * FONT_LINE + 12;
    y = sh - h - 6;
    age = (uint16_t)(beat_now() - cap_t0);
    if (!ui_window_pop(8, y, sw - 16, h, style, age)) return;
    chars = (uint32_t)age * 2;
    ui_text_wrap_n(16, y + 6, sw - 32, buf, ui_style_text(style), (int)chars);
    if (chars > strlen(buf) && ((ct / 10) & 1))
        plat_text(sw - 22, y + h - 10, STR_ICON_ARROW_D, ui_style(style)->accent);
}

/* ---- chapter cards ------------------------------------------------------------------ */
static void draw_card(int ch)
{
    static const uint8_t styles[4] = { WIN_CUTE, WIN_DARK, WIN_HELL, WIN_MINT };
    int sw = plat_screen_w(), sh = plat_screen_h(), s, y, i;
    char num[16];
    const char *title = story_chapter_title(ch);
    int style = styles[(ch - 1) & 3];
    ui_backdrop(style, ct);
    if (ch == 2) {
        for (i = 0; i < 5; i++)
            puff_cloud((int)((i * 67 + ct / 2) % (uint32_t)(sw + 80)) - 40, sh - 14 - (i % 3) * 16,
                       14 + (i % 3) * 5, RAMP_VAPE, 4 + (i & 1), ct + (uint32_t)i * 9);
    }
    if (ch == 3) {
        for (i = 0; i < 18; i++) {
            int x = (int)((i * 71 + ct * (2 + i % 3)) % (uint32_t)sw);
            int yy = sh - (int)((ct * (3 + i % 4) + (uint32_t)i * 29) % (uint32_t)sh);
            plat_rect(x, yy, 1, 2, PAL_RAMP(RAMP_LAVA, 4 + i % 4));
        }
        if ((ct % 60) < 3) plat_checker(0, 0, sw, sh, PAL_UI_RED);
    }
    if (ch == 4) {
        plat_checker(sw / 2, 0, sw / 2, sh, PAL_RAMP(RAMP_VAPE, 2));
        ui_heart(sw / 2, sh / 2 + 34, 10 + ui_wave((int)ct, 30, 2), PAL_UI_PINK);
    }
    doomo_snprintf(num, sizeof(num), "Chapter %d", ch);
    s = ui_pop_scale((int)ct);
    y = sh / 2 - 30;
    if (ch == 3 && ct < 40) y += ((ct * 7) % 5) - 2;
    if (s > 0) {
        int sc = sh >= 200 ? 3 : 2;
        ui_text_big((sw - ui_text_big_width(num, sc)) / 2, y, num, sc, PAL_WHITE,
                    ch == 3 ? PAL_UI_RED_D : ch == 2 ? PAL_RAMP(RAMP_VAPE, 1) : PAL_UI_PINK_D);
    }
    if (ct > 18) {
        int sc = (ui_text_big_width(title, 2) < sw - 16) ? 2 : 1;
        int ty = y + (sh >= 200 ? 34 : 26);
        if (sc == 2) ui_text_big((sw - ui_text_big_width(title, 2)) / 2, ty, title, 2, PAL_UI_CREAM, PAL_BLACK);
        else ui_title(sw / 2, ty, title, style);
    }
    for (i = 0; i < 6; i++)
        ui_sparkle((int)((i * 41 + 17) % (uint32_t)sw), (int)((i * 29 + 11) % (uint32_t)(sh / 3)) + 6,
                   (int)((ct / 2 + (uint32_t)i * 5) & 15), PAL_WHITE);
    if (ct > 60 && ((ct / 16) & 1)) ui_text_center(sw / 2, sh - 14, STR_ICON_A " Continue", PAL_WHITE);
}

/* ---- vignettes ------------------------------------------------------------------------- */
static void draw_hills(int sw, int sh, uint8_t far, uint8_t near)
{
    ui_fill_ellipse(sw / 4, sh * 3 / 4, sw / 2, sh / 4, far);
    ui_fill_ellipse(sw * 3 / 4, sh * 3 / 4 + 6, sw / 2, sh / 4, far);
    ui_fill_ellipse(sw / 2, sh + 10, sw * 3 / 4, sh / 3, near);
}

static void draw_player_figure(int x, int y, int expr)
{
    const DoomoResident *p = town_res(DOOMO_PLAYER_ID);
    const DoomoImage *f;
    if (!p || p->status == RES_EMPTY) return;
    f = figure_image(&p->look, p->status, expr);
    if (f) plat_image(x, y, f, 1, IMG_SHADOW, 0);
}

static void draw_arrival(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), bx, by, i;
    sky_bands(PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_CANDY, 7), sh * 2 / 3);
    ui_fill_circle(sw - 40, 30, 14, PAL_RAMP(RAMP_CANDY, 6));
    ui_fill_circle(sw - 43, 27, 4, PAL_WHITE);
    for (i = 0; i < 3; i++) cloud((int)((i * 97 + ct / 3) % (uint32_t)(sw + 80)) - 40, 22 + i * 14, 9, PAL_WHITE, 0);
    draw_hills(sw, sh, PAL_RAMP(RAMP_GRASS, 6), PAL_RAMP(RAMP_GRASS, 5));
    plat_rect(0, sh * 3 / 4 + 4, sw, 10, PAL_RAMP(RAMP_PEACH, 5));
    plat_rect(0, sh * 3 / 4 + 4, sw, 1, PAL_RAMP(RAMP_PEACH, 3));
    for (i = 0; i < sw; i += 16) plat_rect(i + (int)((ct) % 16), sh * 3 / 4 + 8, 6, 1, PAL_WHITE);
    /* the bus */
    bx = (int)DOOMO_MIN((int)ct * 2 - 80, sw / 2 - 30);
    by = sh * 3 / 4 - 22 + (((ct / 4) & 1) && bx < sw / 2 - 30 ? 1 : 0);
    ui_round_rect(bx, by, 60, 26, 6, PAL_RAMP(RAMP_PASTEL_PINK, 2));
    ui_round_rect(bx + 1, by + 1, 58, 23, 5, PAL_RAMP(RAMP_PASTEL_PINK, 5));
    plat_rect(bx + 3, by + 3, 54, 1, PAL_RAMP(RAMP_PASTEL_PINK, 7));
    for (i = 0; i < 4; i++) ui_round_rect(bx + 6 + i * 13, by + 6, 10, 8, 2, PAL_RAMP(RAMP_SKY, 6));
    ui_fill_circle(bx + 14, by + 26, 5, PAL_RAMP(RAMP_STONE, 1));
    ui_fill_circle(bx + 46, by + 26, 5, PAL_RAMP(RAMP_STONE, 1));
    ui_fill_circle(bx + 14, by + 26, 2, PAL_GRAY_L);
    ui_fill_circle(bx + 46, by + 26, 2, PAL_GRAY_L);
    ui_heart(bx + 52, by + 18, 3, PAL_UI_RED);
    /* the bus stop: a candy-striped pole and a round sign */
    plat_rect(sw / 6 + 2, sh * 3 / 4 - 40, 3, 44, PAL_RAMP(RAMP_PASTEL_PINK, 3));
    for (i = 0; i < 5; i++) plat_rect(sw / 6 + 2, sh * 3 / 4 - 40 + i * 9, 3, 4, PAL_WHITE);
    gg_ellipse(sw / 6 + 3, sh * 3 / 4 - 44, 9, 9, PAL_RAMP(RAMP_PASTEL_PINK, 3));
    gg_ellipse(sw / 6 + 3, sh * 3 / 4 - 44, 7, 7, PAL_UI_CREAM);
    ui_heart(sw / 6 + 3, sh * 3 / 4 - 44, 3, PAL_UI_PINK);
    if (panel >= 1) {                                    /* the welcome committee waits by the sign */
        static const int16_t hosts[2] = { ID_MAYOR, ID_NANA };
        gg_crowd(hosts, 2, sw / 6 + 18, sh * 3 / 4 + 4, 18, 0, 1, panel >= 2 ? GG_CROWD_CHEER : GG_CROWD_SWOON, ct);
    }
    if (bx >= sw / 2 - 30 && panel >= 1) {
        int hop = ui_wave((int)pt, 20, 3);
        draw_player_figure(bx + 64, sh * 3 / 4 - FIGURE_H + 10 - (hop < 0 ? -hop : hop), EXPR_HAPPY);
        for (i = 0; i < 5; i++) ui_sparkle(bx + 64 + (i * 13) % 50, sh / 2 - 30 + (i * 7) % 30, (int)((pt / 2 + i * 3) & 15), PAL_WHITE);
    }
}

static void draw_home(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), i, cx = sw / 2, ey = sh / 2 + 6;
    const DoomoImage *egg;
    tile_tex(TEX_WALLPAPER_HEARTS, 0, 0, sw, sh * 2 / 3, 1);
    tile_tex(TEX_WOOD_FLOOR, 0, sh * 2 / 3, sw, sh / 3 + 64, 1);
    plat_rect(0, sh * 2 / 3 - 2, sw, 3, PAL_RAMP(RAMP_WOOD, 2));
    /* window */
    ui_round_rect(sw / 6, 16, 50, 40, 3, PAL_RAMP(RAMP_WOOD, 3));
    plat_rect(sw / 6 + 4, 20, 42, 32, PAL_RAMP(RAMP_SKY, 6));
    cloud(sw / 6 + 24, 34, 6, PAL_WHITE, 0);
    plat_rect(sw / 6 + 24, 20, 2, 32, PAL_RAMP(RAMP_WOOD, 3));
    /* bed */
    ui_round_rect(sw - 90, sh * 2 / 3 - 20, 80, 26, 5, PAL_RAMP(RAMP_PASTEL_PINK, 4));
    ui_round_rect(sw - 88, sh * 2 / 3 - 26, 24, 12, 5, PAL_WHITE);
    /* cushion + egg */
    ui_fill_ellipse(cx, ey + 26, 26, 8, PAL_RAMP(RAMP_PASTEL_PINK, 5));
    egg = sprite_image(SPR_EGG, panel >= 1 ? (int)((pt / 4) & 1) : 0);
    if (egg) {
        int wob = panel >= 1 ? ui_wave((int)pt, 8, 2) : 0;
        plat_image(cx - egg->w * 3 / 2 + wob, ey + 24 - egg->h * 3, egg, 3, 0, 0);
    }
    if (panel >= 1) {
        for (i = 0; i < 8; i++) {
            int a = (int)(i * 512 + pt * 20);
            ui_sparkle(cx + FIX2INT(fx_cos(a) * 34), ey + FIX2INT(fx_sin(a) * 22), (int)((pt / 2 + i * 2) & 15), PAL_UI_GOLD);
        }
    }
    draw_player_figure(sw / 6 - 10, sh * 2 / 3 - FIGURE_H + 14, EXPR_LOVE);
}

static void draw_wall_row(int y, int h, int sw)
{
    tile_tex(TEX_RUST_METAL, 0, y, sw, h, 1);
    plat_rect(0, y, sw, 1, PAL_RAMP(RAMP_STEEL, 1));
}

static void draw_fog(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), i;
    sky_bands(PAL_RAMP(RAMP_VAPE, 0), PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 2), sh);
    draw_wall_row(sh / 3, sh / 2, sw);
    for (i = 0; i < 7; i++) {
        int x = (int)((i * 71 + ct * (1 + i % 2) / 2) % (uint32_t)(sw + 80)) - 40;
        puff_cloud(x, sh / 3 - 4 + (i % 4) * 16 + ui_wave((int)ct + i * 9, 60, 3), 14 + (i % 3) * 5,
                   RAMP_VAPE, 4 + (i % 3), ct + (uint32_t)i * 7);
    }
    if (panel >= 1) {
        int blink = ((ct / 50) % 4) == 3 && (ct % 50) < 5;
        int ex = sw / 2, ey = sh / 3 + 18;
        if (!blink) {
            ui_fill_circle(ex - 12, ey, 3, PAL_RAMP(RAMP_LAVA, 6));
            ui_fill_circle(ex + 12, ey, 3, PAL_RAMP(RAMP_LAVA, 6));
            plat_rect(ex - 13, ey - 1, 1, 1, PAL_WHITE);
            plat_rect(ex + 11, ey - 1, 1, 1, PAL_WHITE);
        }
    }
    if (panel >= 2) {
        static const char *const hee[3] = { "hee", "hee hee", "hee hee hee" };
        ui_text_outline(sw / 2 - 30 + ui_wave((int)ct, 20, 4), sh / 3 - 18, hee[(ct / 15) % 3],
                        PAL_RAMP(RAMP_VAPE, 7), PAL_RAMP(RAMP_VAPE, 0));
    }
    /* Hillvile Lane below the Wall: a street lamp, the neighbours, you */
    plat_rect(0, sh * 5 / 6, sw, sh / 6, PAL_RAMP(RAMP_VAPE, 1));
    plat_checker(0, sh * 5 / 6, sw, 2, PAL_RAMP(RAMP_VAPE, 2));
    plat_checker(sw - 40, sh * 5 / 6 - 50, 32, 30, PAL_RAMP(RAMP_CANDY, 2));           /* the lamp's glow */
    plat_rect(sw - 25, sh * 5 / 6 - 40, 3, 42, PAL_RAMP(RAMP_STEEL, 2));
    gg_ellipse(sw - 24, sh * 5 / 6 - 42, 5, 4, PAL_RAMP(RAMP_CANDY, 6));
    {
        static const int16_t folk[2] = { ID_PIP, ID_NANA };
        gg_crowd(folk, 2, sw - 70, sh * 5 / 6 + 6, 22, 3, 1, panel >= 1 ? GG_CROWD_SHOCK : GG_CROWD_CALM, ct);
    }
    draw_player_figure(sw / 6 - 10, sh * 5 / 6 - FIGURE_H + 14, panel >= 2 ? EXPR_SHOCK : EXPR_NEUTRAL);
}

static void draw_eruption(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), i, r, shake = 0;
    int phase = panel;
    if (phase >= 2) shake = ((ct * 7) % 7) - 3;
    if (phase == 0) sky_bands(PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_PASTEL_PINK, 6), sh);
    else if (phase == 1) sky_bands(PAL_RAMP(RAMP_VAPE, 4), PAL_RAMP(RAMP_VAPE, 3), PAL_RAMP(RAMP_PASTEL_PINK, 4), sh);
    else sky_bands(PAL_RAMP(RAMP_BLOOD, 1), PAL_RAMP(RAMP_BLOOD, 3), PAL_RAMP(RAMP_LAVA, 4), sh);
    /* the swelling cloud */
    r = 18 + (int)(phase * 22) + (int)DOOMO_MIN(pt, 60u) / 2;
    puff_cloud(sw / 2 + shake, sh / 3 + 4, r, RAMP_VAPE, phase >= 2 ? 3 : 5, ct);
    if (phase >= 1) {
        puff_cloud(sw / 2 - r + shake, sh / 3 + 16, r / 2, RAMP_VAPE, phase >= 2 ? 2 : 4, ct + 20);
        puff_cloud(sw / 2 + r + shake, sh / 3 + 14, r / 2, RAMP_VAPE, phase >= 2 ? 2 : 4, ct + 40);
    }
    if (phase >= 2) {
        /* the cloud's grin */
        int k;
        for (k = -4; k <= 4; k++) plat_rect(sw / 2 + shake + k * r / 10, sh / 3 + 6 + (k * k) / 3, r / 10 + 1, 2, PAL_RAMP(RAMP_LAVA, 6));
        ui_fill_circle(sw / 2 + shake - r / 4, sh / 3 - r / 5, 3, PAL_RAMP(RAMP_LAVA, 7));
        ui_fill_circle(sw / 2 + shake + r / 4, sh / 3 - r / 5, 3, PAL_RAMP(RAMP_LAVA, 7));
    }
    if (phase >= 2) {
        for (i = 0; i < 4; i++) {
            int lx = (int)((i * 83 + ct * 5) % (uint32_t)sw);
            if (((ct + (uint32_t)i * 11) % 23) < 3) plat_line(lx, 0, lx + 10, sh / 3, PAL_WHITE);
        }
        if (pt < 6) plat_rect(0, 0, sw, sh, PAL_WHITE);
    }
    draw_wall_row(sh / 2 + shake, sh / 4, sw);
    plat_rect(0, sh * 3 / 4, sw, sh / 4, phase >= 2 ? PAL_RAMP(RAMP_FLESH, 2) : PAL_RAMP(RAMP_GRASS, 5));
    if (phase >= 3) {
        static const uint8_t dem[5] = { SPR_IMPLING, SPR_PUFF_DEMON, SPR_VAPE_WRAITH, SPR_GARGOYLE, SPR_HELLHOUND_PUP };
        for (i = 0; i < 5; i++) {
            const DoomoImage *d = sprite_image(dem[i], (int)((ct / 6 + (uint32_t)i) & 1));
            int x = sw / 2 - 90 + i * 40 + ui_wave((int)ct + i * 20, 40, 6);
            int y = sh / 3 - 10 + (int)((pt * (2 + (uint32_t)i % 3)) % 60u) - 30;
            if (d) plat_image(x - d->w / 2, y, d, 1, IMG_SILHOUETTE, PAL_BLACK);
        }
        ui_text_big((sw - ui_text_big_width("!!!", 3)) / 2 + shake, 18, "!!!", 3, PAL_UI_RED, PAL_BLACK);
        {   /* the citizens scatter */
            static const int16_t folk[4] = { ID_MAYOR, ID_PIP, ID_NANA, ID_FUZZ };
            gg_crowd(folk, 4, sw / 2 - 60 - (int)DOOMO_MIN(pt, 60u), sh * 3 / 4 + 10, 30, 1, 1, GG_CROWD_CHEER, ct);
        }
    }
    if (phase < 3) draw_player_figure(sw / 6 - 10, sh * 3 / 4 - FIGURE_H + 14 + shake, phase >= 2 ? EXPR_SHOCK : EXPR_NEUTRAL);
}

static void draw_vapula(void)
{
    static const int16_t folk[6] = { ID_MAYOR, ID_NANA, ID_PIP, ID_FUZZ, ID_SOOTKIN, ID_RIVET };
    int sw = plat_screen_w(), sh = plat_screen_h(), i, scale, frame, big = sh >= 200, gy = sh * 3 / 4 + 6;
    const DoomoImage *v;
    if (panel < 2) sky_bands(PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 4), sh);
    else sky_bands(PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_CANDY, 7), sh);
    /* the top of the spire, where it happened */
    plat_rect(0, gy, sw, sh - gy, panel < 2 ? PAL_RAMP(RAMP_STONE, 2) : PAL_RAMP(RAMP_STONE, 4));
    plat_checker(0, gy, sw, 2, panel < 2 ? PAL_RAMP(RAMP_VAPE, 3) : PAL_RAMP(RAMP_PASTEL_PINK, 6));
    for (i = 0; i < sw; i += 24) plat_rect(i + 4, gy - 6, 12, 6, panel < 2 ? PAL_RAMP(RAMP_STONE, 1) : PAL_RAMP(RAMP_STONE, 3));   /* battlements */
    scale = big ? 4 : 3;
    frame = panel == 0 ? DFRAME_PAIN : panel == 1 ? DFRAME_DEATH0 + (int)DOOMO_MIN(pt / 12, 3u) : DFRAME_DEATH3;
    if (panel == 1) scale = DOOMO_MAX(scale - (int)DOOMO_MIN(pt / 24, 2u), 1);          /* the Duke lets his air out */
    v = sprite_image(SPR_LORD_VAPULA, frame);
    if (v && panel < 2) {
        int sx = panel == 0 ? ((int)(ct * 3) % 5) - 2 : 0;
        gg_ellipse(sw / 2, gy - 1, v->w * scale / 3, 2 + scale, PAL_RAMP(RAMP_STONE, 0));
        plat_image(sw / 2 - v->w * scale / 2 + sx, gy - v->h * scale - 2, v, scale, 0, 0);
    }
    if (panel == 1) {
        for (i = 0; i < 8; i++) {                                                          /* Pfffffff: the vapour leaves him */
            int a = i * 512 + (int)pt * 12, rr = (int)DOOMO_MIN(pt * 2, 90u);
            puff_cloud(sw / 2 + FIX2INT(fx_cos(a) * rr), sh / 2 - 10 + FIX2INT(fx_sin(a) * rr) / 2,
                       6 + (int)(pt / 20), RAMP_VAPE, 5, pt + (uint32_t)i * 5);
        }
        if (pt > 10 && pt < 60) ui_text_outline(sw / 2 + 24, sh / 3, "pfffffff", PAL_RAMP(RAMP_VAPE, 7), PAL_RAMP(RAMP_VAPE, 0));
        if (pt >= 60) ui_text_outline(sw / 2 + 24, sh / 3 + 8, "*squeak*", PAL_UI_PINK, PAL_RAMP(RAMP_VAPE, 0));
    }
    if (panel == 0) {
        for (i = 0; i < 3; i++) puff_cloud(30 + i * (sw - 60) / 2, 24 + (i & 1) * 12, 16, RAMP_VAPE, 3, ct + (uint32_t)i * 13);
        if ((ct / 20) & 1) ui_text_outline(sw / 2 + 30, sh / 4, "*wheeze*", PAL_RAMP(RAMP_VAPE, 7), PAL_RAMP(RAMP_VAPE, 0));
    }
    if (panel == 2) {
        const DoomoImage *small = sprite_image(SPR_LORD_VAPULA, DFRAME_WALK0);
        if (small) plat_image(sw / 2 - small->w / 2, gy - small->h, small, 1, 0, 0);       /* a sad little plush */
        gg_crowd(folk, 3, sw / 2 - 90, gy + 4, 26, 2, 1, GG_CROWD_CHEER, ct);
        gg_crowd(folk + 3, 3, sw / 2 + 40, gy + 4, 26, 1, 1, GG_CROWD_CHEER, ct);
        for (i = 0; i < 24; i++) {
            int x = (int)((i * 37 + pt * (1 + i % 3)) % (uint32_t)sw);
            int y = (int)((pt * (2 + (uint32_t)i % 3) + (uint32_t)i * 13) % (uint32_t)sh);
            plat_rect(x, y, 2, 2, PAL_PAINT(i & 31, 2));
        }
        for (i = 0; i < 4; i++) ui_heart(40 + i * (sw - 80) / 3, sh / 3 - (int)((pt * 2 + (uint32_t)i * 11) % 40u), 3, PAL_UI_PINK);
    }
    draw_player_figure(sw / 6 - 10, gy - FIGURE_H + 8, panel == 2 ? EXPR_HAPPY : EXPR_SHOCK);
}

static void draw_wall_falls(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), i, j, gy = sh * 3 / 4;
    plat_rect(0, 0, sw / 2, gy, PAL_RAMP(RAMP_SKY, 6));
    plat_rect(sw / 2, 0, sw - sw / 2, gy, PAL_RAMP(RAMP_VAPE, 3));
    plat_rect(0, gy, sw / 2, sh - gy, PAL_RAMP(RAMP_GRASS, 5));
    plat_rect(sw / 2, gy, sw - sw / 2, sh - gy, PAL_RAMP(RAMP_GRASS, 3));
    /* the Wall: a tall rusty brick wall that holds, shudders, then topples */
    {
        int rows = (gy - 6) / 10, cols = 5, bw = 12, bh = 10, x0 = sw / 2 - cols * bw / 2;
        int fall0 = 40;
        for (j = 0; j < rows; j++)
            for (i = 0; i < cols; i++) {
                int off = (j & 1) ? bw / 2 : 0;
                int bx = x0 + i * bw - off, by = gy - (j + 1) * bh, dx = 0;
                int delay = fall0 + (rows - 1 - j) * 3 + ((i * 7 + j * 3) % 5) * 2;
                int ft = (int)ct - delay;
                uint8_t c = PAL_RAMP(RAMP_STEEL, 2 + (i + j) % 3);
                if (i == 0 && off) { bx += off; }
                if ((int)ct < fall0 && (ct & 2) && ct > 20) dx = ((i + j) & 1) ? 1 : -1;   /* shudder */
                if (ft > 0) {
                    int land = gy - 4 - ((i * 5 + j * 3) % 4) * 3;
                    by += ft * ft / 6;
                    dx = (i - cols / 2) * ft * 2 / 3 + ((j & 1) ? ft / 3 : -ft / 3);
                    if (by > land) by = land;
                }
                plat_rect(bx + dx, by, bw - 1, bh - 1, PAL_RAMP(RAMP_STEEL, 1));
                plat_rect(bx + dx + 1, by + 1, bw - 3, bh - 3, c);
                plat_rect(bx + dx + 1, by + 1, bw - 3, 1, PAL_RAMP(RAMP_STEEL, 6));
                if (((i + j) % 3) == 0) plat_rect(bx + dx + 3, by + 4, 3, 2, PAL_RAMP(RAMP_WOOD, 2));   /* rust */
            }
        if (ct > (uint32_t)fall0 && ct < (uint32_t)fall0 + 70)
            for (i = 0; i < 5; i++)
                puff_cloud(sw / 2 - 48 + i * 24, gy - 8 - (int)((ct - (uint32_t)fall0) / 6), 9 + i % 2 * 3, RAMP_STONE, 5, ct + (uint32_t)i * 11);
    }
    /* both sides run together */
    if (panel >= 1) {
        static const int16_t left[4] = { ID_MAYOR, ID_PIP, ID_NANA, ID_FUZZ };
        static const int16_t right[4] = { ID_MOTHER_NOX, ID_SOOTKIN, ID_ANVIL, ID_TALLYWORN };
        int run = panel >= 2 ? (int)DOOMO_MIN(pt, 80u) : 0;
        for (i = 0; i < 4; i++) {
            const DoomoImage *a = bodyspr_image(left[i], 2, run ? (int)((ct / 4 + (uint32_t)i) & 3) : 0);
            const DoomoImage *b = bodyspr_image(right[i], 1, run ? (int)((ct / 4 + (uint32_t)i) & 3) : 0);
            int y = gy - 24 + (i & 1) * 10;
            if (a && i == 0 && run && run < 80) {     /* the leaders' running bounce (squash) */
                TwSquash q = tw_squash(&TW_DROP_SOFT_SX, &TW_DROP_SOFT_SY, &TW_DROP_SOFT_DY,
                                       (int)(ct & 7), tw_bounce_amount());
                plat_image_xform(18 + run + a->w / 2, y + a->h + q.dy, a, q.sx, q.sy, 0, 0);
            } else if (a) plat_image(18 + i * 14 + run, y, a, 1, 0, 0);
            if (b && i == 0 && run && run < 80) {
                TwSquash q = tw_squash(&TW_DROP_SOFT_SX, &TW_DROP_SOFT_SY, &TW_DROP_SOFT_DY,
                                       (int)((ct + 4) & 7), tw_bounce_amount());
                plat_image_xform(sw - 34 - run + b->w / 2, y + b->h + q.dy, b, q.sx, q.sy, 0, 0);
            } else if (b) plat_image(sw - 34 - i * 14 - run, y, b, 1, 0, 0);
        }
        if (panel >= 2 && pt > 50)
            for (i = 0; i < 6; i++) ui_heart(sw / 2 - 40 + i * 16, gy - 40 - (int)((pt * 2 + (uint32_t)i * 9) % 30u), 4, PAL_UI_PINK);
    }
}

static void draw_freeplay(void)
{
    static const int16_t left[4] = { ID_MAYOR, ID_PIP, ID_NANA, ID_FUZZ };
    static const int16_t right[4] = { ID_MOTHER_NOX, ID_SOOTKIN, ID_ANVIL, ID_RIVET };
    int sw = plat_screen_w(), sh = plat_screen_h(), i, s, gy = sh * 3 / 4;
    sky_bands(PAL_RAMP(RAMP_SKY, 6), PAL_RAMP(RAMP_PASTEL_PINK, 7), PAL_RAMP(RAMP_CANDY, 7), gy);
    plat_checker(sw / 2, 0, sw - sw / 2, gy, PAL_RAMP(RAMP_VAPE, 3));
    draw_hills(sw, sh, PAL_RAMP(RAMP_GRASS, 6), PAL_RAMP(RAMP_GRASS, 5));
    plat_rect(0, gy, sw, sh - gy, PAL_RAMP(RAMP_GRASS, 5));
    plat_checker(0, gy, sw, 2, PAL_RAMP(RAMP_GRASS, 6));
    for (i = 0; i < 5; i++) {                                                             /* what is left of the Wall */
        gg_ellipse(sw / 2 - 20 + i * 10, gy + 2 + (i & 1) * 3, 7, 4, PAL_RAMP(RAMP_STEEL, 1));
        gg_ellipse(sw / 2 - 20 + i * 10, gy + 1 + (i & 1) * 3, 6, 3, PAL_RAMP(RAMP_STEEL, 3));
    }
    gg_lanterns(4, ct);
    gg_crowd(left, 4, sw / 2 - 86, gy + 10, 20, 2, 1, GG_CROWD_CHEER, ct);               /* both sides, together */
    gg_crowd(right, 4, sw / 2 + 26, gy + 10, 20, 1, 1, GG_CROWD_SWOON, ct + 9);
    draw_player_figure(sw / 2 - FIGURE_W / 2, gy - FIGURE_H + 14 - (ui_wave((int)ct, 20, 3) < 0 ? -ui_wave((int)ct, 20, 3) : ui_wave((int)ct, 20, 3)), EXPR_HAPPY);
    for (i = 0; i < 4; i++) {
        int fx = (int)((i * 67 + 30) % (uint32_t)sw), fy = 26 + (i * 19) % 40, t2 = (int)((ct + (uint32_t)i * 17) % 50u);
        int k;
        for (k = 0; k < 12; k++) {
            int a = k * 341;
            plat_rect(fx + FIX2INT(fx_cos(a) * t2), fy + FIX2INT(fx_sin(a) * t2), 2, 2, PAL_PAINT((i * 7 + k) & 31, 2));
        }
    }
    s = sh >= 200 ? 3 : 2;
    ui_text_big((sw - ui_text_big_width("THE END", s)) / 2, sh / 2 - 34, "THE END", s, PAL_WHITE, PAL_UI_PINK_D);
    if (ct > 30) ui_title(sw / 2, sh / 2 - 6, "...of the beginning!", WIN_CUTE);
}

/* ---- the timing showcase (S1: runner timing_showcase.txt) -------------------------
 * SCENE_CUTSCENE arg CUT_SHOWCASE_ARG + k replays three of the trailer's
 * gags in-engine, on beat scripts and the shared tween tables, so their
 * frames can be held against the trailer's (trailer/scenes/s04, s12, s14):
 *   0  s04 the pet turn: "beep.", a 4-frame hell insert, "beep." again with
 *      horns (the one persistent change), "...it's fine." (the button)
 *   1  s12 the census: 1000 / 500 / 500 / 1 slams (rule of three), and one
 *      critter pops out of the top like a cork: "(cozy.)"
 *   2  s14 the handheld DROP: squash on landing, the power-on ping, then a
 *      deadpan silence and its button */
#define CUT_SHOWCASE_ARG 256
enum { L_SC1 = 1, L_SC2, L_SC3, L_SC4, L_SC5 };
static const BeatEv B_sc_pet[] = {
    MUSIC(0, 0, SONG_PET), EV(0, 0, BOP_SHOT, 0, 0),
    EV(40, 0, BOP_LABEL, L_SC1, 0),
    EV(128, 0, BOP_LABEL, L_PUNCH, 0),
    SFX(1, L_SC1, SFX_BEEP), EV(1, L_SC1, BOP_MUSIC_DUCK, 13, 140),
    /* the whiplash: 4 frames of hell (mutes itself), back to cute with horns */
    EV(0, L_PUNCH, BOP_INSERT, 1, 4), EV(0, L_PUNCH, BOP_PAL_LERP, PAL_HELLIFY, 16),
    SFX(0, L_PUNCH, SFX_GLITCH), EV(4, L_PUNCH, BOP_PAL_LERP, PAL_HELLIFY, 0),
    SFX(7, L_PUNCH, SFX_BEEP),
    EV(19, L_PUNCH, BOP_LABEL, L_BUTTON, 0), SFX(19, L_PUNCH, SFX_TIK),
    EV(60, L_PUNCH, BOP_END, 0, 0)
};
static const BeatEv B_sc_census[] = {
    MUSIC(0, 0, SONG_FESTIVAL), EV(0, 0, BOP_SHOT, 1, 0),
    EV(22, 0, BOP_LABEL, L_SC1, 0), EV(46, 0, BOP_LABEL, L_SC2, 0),
    EV(68, 0, BOP_LABEL, L_SC3, 0), EV(90, 0, BOP_LABEL, L_SC4, 0),
    EV(104, 0, BOP_LABEL, L_PUNCH, 0),
    SFX(0, L_SC1, SFX_THUD), EV(0, L_SC1, BOP_SHAKE, SHK_STAMP, 0),
    SFX(0, L_SC2, SFX_THUD), EV(0, L_SC2, BOP_SHAKE, SHK_STAMP, 0),
    SFX(0, L_SC3, SFX_THUD), EV(0, L_SC3, BOP_SHAKE, SHK_STAMP3, 0),
    SFX(0, L_SC4, SFX_THUD), EV(0, L_SC4, BOP_SHAKE, SHK_SLAM, 0), EV(0, L_SC4, BOP_MUSIC_DUCK, 14, 140),
    SFX(0, L_PUNCH, SFX_POP2), EV(13, L_PUNCH, BOP_LABEL, L_BUTTON, 0), SFX(13, L_PUNCH, SFX_TINK),
    EV(46, L_PUNCH, BOP_END, 0, 0)
};
static const BeatEv B_sc_drop[] = {
    EV(0, 0, BOP_SHOT, 2, 0), SFX(0, 0, SFX_WHOOSH),
    EV(8, 0, BOP_LABEL, L_SC1, 0), EV(20, 0, BOP_LABEL, L_SC2, 0),
    EV(74, 0, BOP_LABEL, L_PUNCH, 0),
    SFX(0, L_SC1, SFX_THUD), EV(0, L_SC1, BOP_SHAKE, SHK_TAP, 0),
    SFX(0, L_SC2, SFX_TINK), MUSIC(0, L_SC2, SONG_TITLE),
    EV(0, L_PUNCH, BOP_MUSIC_MUTE, 40, 0), EV(0, L_PUNCH, BOP_NOBUTTON, 0, 0),
    EV(21, L_PUNCH, BOP_LABEL, L_BUTTON, 0),
    EV(50, L_PUNCH, BOP_END, 0, 0)
};
static const BeatEv *const sc_ev[3] = { B_sc_pet, B_sc_census, B_sc_drop };
static const uint16_t sc_n[3] = {
    (uint16_t)(sizeof(B_sc_pet) / sizeof(B_sc_pet[0])), (uint16_t)(sizeof(B_sc_census) / sizeof(B_sc_census[0])),
    (uint16_t)(sizeof(B_sc_drop) / sizeof(B_sc_drop[0]))
};
static int showcase = -1;

int cutscene_showcase_script(int k, BeatScript *out)
{
    if (k < 0 || k >= 3 || !out) return 0;
    out->ev = sc_ev[k];
    out->n = sc_n[k];
    out->nlabels = BEAT_LABEL_MAX;
    return 1;
}

static void speech(int x, int y, const char *s, int age)
{
    int sc = tw_tab(&TW_POP_BUBBLE, age), w = ui_text_width(s) + 10, h = 13;
    int ww = w * sc / 256, hh = h * sc / 256;
    if (age < 0 || ww < 4) return;
    ui_round_rect(x - ww / 2 - 1, y - hh / 2 - 1, ww + 2, hh + 2, 4, PAL_BLACK);
    ui_round_rect(x - ww / 2, y - hh / 2, ww, hh, 4, PAL_WHITE);
    plat_rect(x - ww / 2 - 4, y + 2, 4, 2, PAL_WHITE);
    if (age >= 2) ui_text(x - w / 2 + 5, y - 4, s, PAL_BLACK);
}

static void draw_sc_pet(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), cx = sw / 2, cy = sh / 2 + 6;
    int ins = beat_age(L_PUNCH), horns = ins >= 6;
    const DoomoImage *pet = sprite_image(SPR_PET, (int)(ct / 8) & 3);
    if (ins >= 0 && ins < 4) {                       /* the insert: the screen is hell */
        int i;
        plat_clear(PAL_RAMP(RAMP_BLOOD, 0));
        for (i = 0; i < 5; i++) {
            int a0 = i * 3277, a1 = ((i + 2) % 5) * 3277;
            plat_line(cx + FIX2INT(fx_sin(a0) * 40), cy - FIX2INT(fx_cos(a0) * 40),
                      cx + FIX2INT(fx_sin(a1) * 40), cy - FIX2INT(fx_cos(a1) * 40), PAL_UI_RED);
        }
        ui_fill_circle(cx - 8, cy - 6, 3, PAL_UI_RED);
        ui_fill_circle(cx + 8, cy - 6, 3, PAL_UI_RED);
        return;
    }
    ui_backdrop(WIN_CUTE, ct);
    /* the keychain egg: a glossy plastic shell around the LCD */
    gg_ellipse(cx, cy, 52, 46, PAL_RAMP(RAMP_PASTEL_PINK, 2));
    gg_ellipse(cx, cy, 50, 44, PAL_RAMP(RAMP_PASTEL_PINK, 5));
    gg_ellipse(cx - 16, cy - 22, 14, 8, PAL_RAMP(RAMP_PASTEL_PINK, 7));
    ui_round_rect(cx - 30, cy - 24, 60, 40, 5, PAL_RAMP(RAMP_STONE, 1));
    ui_round_rect(cx - 28, cy - 22, 56, 36, 4, PAL_RAMP(RAMP_GRASS, 6));
    if (pet) {
        int dy = tw_tab(&TW_DEVICE_DY, (int)ct % 40), ps = 2;
        int px = cx - pet->w * ps / 2, py = cy - 4 - pet->h * ps / 2 + dy / 2;
        plat_image(px, py, pet, ps, 0, 0);
        if (horns) {                                 /* the persistent change: tiny horns */
            int k, hx[2] = { cx - pet->w * ps / 4, cx + pet->w * ps / 4 - 1 };
            for (k = 0; k < 2; k++) {
                plat_rect(hx[k] - 1, py + 1, 3, 2, PAL_RAMP(RAMP_BLOOD, 2));
                plat_rect(hx[k], py - 2, 2, 3, PAL_RAMP(RAMP_BLOOD, 3));
                plat_rect(hx[k] + (k ? 1 : 0), py - 4, 1, 2, PAL_RAMP(RAMP_BLOOD, 4));
            }
        }
    }
    {
        static const uint8_t btn[3] = { 0, 1, 2 };
        int i;
        for (i = 0; i < 3; i++)
            ui_fill_circle(cx - 14 + i * 14, cy + 26 + (i == 1), 5,
                           btn[i] == 0 ? PAL_RAMP(RAMP_PASTEL_PINK, 3) : btn[i] == 1 ? PAL_RAMP(RAMP_MINT, 4) : PAL_RAMP(RAMP_SKY, 4));
    }
    if (beat_age(L_SC1) >= 0 && ins < 0 && beat_age(L_SC1) < 60) speech(cx + 54, cy - 30, "beep.", beat_age(L_SC1));
    if (ins >= 7) speech(cx + 54, cy - 30, "beep.", ins - 7);
    if (beat_age(L_BUTTON) >= 0) {                   /* deadpan: it appears, no pop */
        int dy = tw_tab(&TW_DEADPAN_DY, beat_age(L_BUTTON));
        ui_text_outline(cx - 40, 10 + dy, "...it's fine.", PAL_WHITE, PAL_RAMP(RAMP_PASTEL_PINK, 1));
    }
}

static void critter(int x, int y, uint8_t c, int hop, int bad)
{
    ui_fill_circle(x, y - hop, 3, c);
    plat_rect(x - 3, y - hop - 4, 1, 2, c);
    plat_rect(x + 2, y - hop - 4, 1, 2, c);
    plat_rect(x - 1, y - hop - 1, 1, 1, bad ? PAL_UI_RED : PAL_BLACK);
    plat_rect(x + 1, y - hop - 1, 1, 1, bad ? PAL_UI_RED : PAL_BLACK);
}

static void slam_text(int cx, int y, const char *s, int age, uint8_t col, uint8_t out)
{
    int sc = tw_tab(&TW_SLAM_COUNTER, age), big = plat_screen_h() >= 200 ? 3 : 2;
    int w = ui_text_big_width(s, big);
    if (age < 0) return;
    y -= (sc - 256) / 16;
    ui_text_big(cx - w / 2 - (sc - 256) / 20, y, s, big, col, out);
}

static void draw_sc_census(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), x, y, i = 0, gy = sh / 3;
    int good = beat_age(L_SC2), bad = beat_age(L_SC3), park = beat_age(L_SC4), cork = beat_age(L_PUNCH);
    int squeeze = park >= 0 ? DOOMO_MIN(park, 12) : 0;
    plat_rect(0, 0, sw / 2, sh, bad >= 0 && good < 0 ? PAL_RAMP(RAMP_SKY, 3) : PAL_RAMP(RAMP_SKY, 6));
    plat_rect(sw / 2, 0, sw - sw / 2, sh, PAL_RAMP(RAMP_BLOOD, bad >= 0 ? 2 : 1));
    for (y = gy; y < sh - 6; y += 8)
        for (x = 6 + squeeze; x < sw - 6 - squeeze; x += 8, i++) {
            int left = x < sw / 2, hop = 0, ph = (int)(doomo_hash32((uint32_t)i) & 63);
            uint8_t c = left ? PAL_PAINT(ph & 31, 2) : PAL_RAMP(RAMP_BLOOD, 4 + (ph & 3));
            int a1 = beat_age(L_SC1);
            if (a1 >= 0 && a1 - (x + y) / 16 >= 0 && a1 - (x + y) / 16 < 6) hop = 2;     /* counted: everyone hops */
            if (good >= 0 && bad < 0 && left) hop = (ui_wave(good * 3 - x / 3, 30, 3) > 1) ? 2 : 0;   /* the wave */
            if (bad >= 0 && park < 0 && !left) hop = (ui_wave((int)ct * 2 + ph * 9, 40, 1) > 0);
            critter(x, y + 4, c, hop, !left);
        }
    if (park >= 0) {                                 /* the picket fence squeezes in */
        for (x = squeeze; x < sw - squeeze; x += 6) plat_rect(x, gy - 10, 3, 10, PAL_WHITE);
        plat_rect(squeeze, gy - 6, sw - 2 * squeeze, 2, PAL_WHITE);
    }
    if (cork >= 0) {                                 /* the cork pops out of the top */
        int py = gy - 8 - DOOMO_MIN(cork, 6) * 3 + (cork > 6 ? DOOMO_MIN(cork - 6, 4) : 0);
        int r = 5 * tw_tab(&TW_POP_BLOB, cork) / 256;
        ui_fill_circle(sw / 2, py, r + 1, PAL_UI_PINK_D);                   /* the cork: one bunny */
        ui_fill_circle(sw / 2, py, r, PAL_UI_PINK);
        plat_rect(sw / 2 - 3, py - r - 5, 2, 5, PAL_UI_PINK);
        plat_rect(sw / 2 + 2, py - r - 5, 2, 5, PAL_UI_PINK);
        plat_rect(sw / 2 - 2, py - 1, 1, 2, PAL_BLACK);
        plat_rect(sw / 2 + 2, py - 1, 1, 2, PAL_BLACK);
        if (cork < 10) ui_text_outline(sw / 2 + 10, py - 4, "pop!", PAL_WHITE, PAL_BLACK);
    }
    if (beat_age(L_SC1) < 0) {                       /* the counter rolls up */
        char n[8];
        doomo_snprintf(n, sizeof n, "%d", (int)DOOMO_MIN(1000u, ct * 1000u / 22u));
        slam_text(sw / 2, 12, n, 99, PAL_UI_GOLD, PAL_BLACK);
    } else if (good < 0) slam_text(sw / 2, 12, "1000 CITIZENS.", beat_age(L_SC1), PAL_UI_GOLD, PAL_BLACK);
    else if (bad < 0) slam_text(sw / 2, 12, "500 GOOD.", good, PAL_UI_PINK, PAL_WHITE);
    else if (park < 0) slam_text(sw / 2, 12, "500 BAD.", bad, PAL_UI_RED, PAL_BLACK);
    else slam_text(sw / 2, 12, "1 PARK.", park, PAL_UI_MINT, PAL_BLACK);
    if (beat_age(L_BUTTON) >= 0)
        ui_text_outline(sw / 2 + 30, (sh >= 200 ? 46 : 34) + tw_tab(&TW_TAG_DY, beat_age(L_BUTTON)), "(cozy.)", PAL_WHITE, PAL_BLACK);
}

static void draw_sc_drop(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), cx = sw / 2, floor = sh * 3 / 4, i;
    int land = beat_age(L_SC1), on = beat_age(L_SC2), dead = beat_age(L_PUNCH);
    int w = 76, h = 40, x, y, sx = 256, sy = 256, dy = 0;
    sky_bands(PAL_RAMP(RAMP_VAPE, 1), PAL_RAMP(RAMP_VAPE, 2), PAL_RAMP(RAMP_VAPE, 3), floor);
    plat_rect(0, floor, sw, sh - floor, PAL_RAMP(RAMP_VAPE, 1));
    plat_checker(cx - 60, 0, 120, floor, PAL_RAMP(RAMP_VAPE, 4));          /* the spotlight */
    for (i = 0; i < 6; i++)
        ui_sparkle((int)((i * 53 + 20) % (uint32_t)sw), 12 + (i * 17) % 50, (int)((ct / 2 + (uint32_t)i * 5) & 15), PAL_WHITE);
    if (land < 0) y = floor - h - 8 - (8 - (int)ct) * (8 - (int)ct) * 2;   /* falling: in2 */
    else {
        sx = tw_tab(&TW_DROP_SOFT_SX, land);
        sy = tw_tab(&TW_DROP_SOFT_SY, land);
        dy = tw_tab(&TW_DROP_SOFT_DY, land);
        sx = 256 + (sx - 256) * tw_bounce_amount() / 256;
        sy = 256 + (sy - 256) * tw_bounce_amount() / 256;
        y = floor - h - 8;
    }
    w = w * sx / 256;
    h = h * sy / 256;
    y = floor - 8 - h + dy;
    x = cx - w / 2;
    gg_ellipse(cx, floor - 6, w / 2 + 4, 3, PAL_RAMP(RAMP_VAPE, 0));        /* contact shadow */
    ui_round_rect(x - 1, y - 1, w + 2, h + 2, 9, PAL_RAMP(RAMP_VAPE, 0));
    ui_round_rect(x, y, w, h, 8, PAL_RAMP(RAMP_VAPE, 5));
    plat_rect(x + 8, y + 2, w - 16, 2, PAL_RAMP(RAMP_VAPE, 7));
    ui_round_rect(x + w / 4, y + h / 6, w / 2, h * 2 / 3, 2, PAL_RAMP(RAMP_STONE, 0));
    if (on >= 0) {
        if (on < 3) plat_rect(x + w / 4, y + h / 2, w / 2, 1, PAL_WHITE);      /* the CRT line */
        else {
            ui_round_rect(x + w / 4 + 1, y + h / 6 + 1, w / 2 - 2, h * 2 / 3 - 2, 2, PAL_RAMP(RAMP_PASTEL_PINK, 6));
            ui_text(x + w / 4 + 3, y + h / 6 + 3, "DOOMO", PAL_UI_RED_D);
            if ((ct / 15) & 1) ui_text(x + w / 4 + 3, y + h / 6 + 13, "START", PAL_UI_PINK_D);
        }
    }
    ui_fill_circle(x + 9, y + h / 2, 4, PAL_RAMP(RAMP_STONE, 1));
    ui_fill_circle(x + w - 10, y + h / 2 - 3, 3, PAL_RAMP(RAMP_PASTEL_PINK, 3));
    ui_fill_circle(x + w - 15, y + h / 2 + 3, 3, PAL_RAMP(RAMP_PASTEL_PINK, 3));
    if (on >= 6 && dead < 0) ui_title(cx, 14, "Coming to GBA", WIN_CUTE);
    if (dead >= 0) {                                  /* deadpan: it just appears */
        ui_text_outline(cx - 56, sh - 26, "Yes, the actual handheld.", PAL_WHITE, PAL_BLACK);
        if (beat_age(L_BUTTON) >= 0) ui_text_outline(cx - 30, sh - 15, "The purple one.", PAL_WHITE, PAL_BLACK);
    }
}

static void cs_draw(void)
{
    if (plat_draw_skipped()) return;
    if (s2cut) { cut12_draw(); return; }      /* S2 */
    if (showcase_arg(-1)) {
        if (showcase == 0) draw_sc_pet();
        else if (showcase == 1) draw_sc_census();
        else draw_sc_drop();
        camfx_apply();
        return;
    }
    switch (cut) {
    case CUT_CHAPTER1: case CUT_CHAPTER2: case CUT_CHAPTER3: case CUT_CHAPTER4:
        draw_card(cut - CUT_CHAPTER1 + 1);
        return;                                  /* cards have no caption */
    case CUT_ARRIVAL: draw_arrival(); break;
    case CUT_HOME: draw_home(); break;
    case CUT_FOG: draw_fog(); break;
    case CUT_ERUPTION: draw_eruption(); break;
    case CUT_VAPULA_DOWN: draw_vapula(); break;
    case CUT_WALL_FALLS: draw_wall_falls(); break;
    case CUT_FREEPLAY: draw_freeplay(); break;
    case CUT_FEAST_POP: feast_draw(panel, pt, ct); break;
    default: plat_clear(PAL_BLACK); break;
    }
    camfx_apply();                               /* the picture shakes, the caption does not (PS1) */
    caption();
}

const DoomoScene SCENE_CUTSCENE = {
    "CUTSCENE", cs_enter, NULL, cs_update, cs_draw, 0
};

/* arg >= 0: set up showcase arg if it is one (1); arg -1: 1 while one runs */
static int showcase_arg(int arg)
{
    BeatScript s;
    if (arg < 0) return showcase >= 0;
    showcase = -1;
    if (arg < CUT_SHOWCASE_ARG || arg >= CUT_SHOWCASE_ARG + 3) return 0;
    showcase = arg - CUT_SHOWCASE_ARG;
    cut = CUT_CHAPTER1;
    panel = 0;
    cap = -1;
    ct = pt = 0;
    camfx_reset();
    cutscene_showcase_script(showcase, &s);
    beat_start(&s);
    return 1;
}
