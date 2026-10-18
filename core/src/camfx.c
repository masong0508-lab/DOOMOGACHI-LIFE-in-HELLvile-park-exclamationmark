/* camfx.c - full-screen comic post effects (camfx.h; owner: S1,
 * docs/design/story/SPEC.md sec.7 S1, comic_timing.md sec.3.10, 3.14, 7.3, 9,
 * Appendix C; story_final.md sec.15.5).
 *
 * The core keeps the state and the clock; the platform applies it through
 * the plat_post_* hooks (platform.h). Resident on the PS1 (SPEC R4).
 *
 * Frame protocol for a scene that uses it:
 *   enter():  camfx_reset();
 *   update(): camfx_tick();  ...then the beats / gameplay that call
 *             camfx_shake(), camfx_flash(), ... (an effect started this
 *             tick shows its frame 0 in this tick's drawing);
 *   draw():   ...world...; camfx_apply(); ...steady HUD (optional)...
 * camfx_apply() pushes the state to the platform for the frame being
 * built. The platforms treat it as per-frame state, so a scene that stops
 * calling camfx_apply() (or a scene pushed on top) shows a clean frame.
 *
 * Accessibility (CT sec.9): options scale AMPLITUDE only, never timing.
 *   OPT_SCREEN_SHAKE 0/1/2 = 0/50/100% of every shake and punch; at 0 the
 *   first frame of a shake becomes a 4/16 darken pulse, so the beat is
 *   still marked on the same frame.
 *   OPT_REDUCE_FLASH: flashes are capped at 6/16 and red / lava flashes
 *   become darken pulses.
 *   Full-screen flashes never start less than 10 frames apart (< 3 Hz).
 */
#include "doomo/doomo.h"
#include "doomo/camfx.h"
#include "doomo/tween.h"
#include "doomo/beat.h"

/* ---- shake profiles (CT Appendix C, verbatim) ----------------------------- */
enum { SX_ALT, SX_RAW, SX_NONE };            /* x = (f&1) ? +amp : -amp | amp signed | 0     */
enum { SY_NONE, SY_HALF, SY_CONST, SY_RAW }; /* y = 0 | (f&2) ? +-amp/2 | (f&2) ? +-yc | amp  */
typedef struct { uint8_t frames, xmode, ymode; int8_t yc; int8_t amp[14]; } ShakeProf;
static const ShakeProf SHAKE[SHK_COUNT] = {
    [SHK_TAP]     = {  2, SX_NONE, SY_RAW,   0, { 3, -1 } },             /* s12, s02, s14         */
    [SHK_BRAKE]   = {  3, SX_RAW,  SY_NONE,  0, { 1, -2, 1 } },          /* s03 record scratch    */
    [SHK_POW]     = {  3, SX_ALT,  SY_CONST, 1, { 2, 2, 2 } },           /* s06 POW, s14 clunk    */
    [SHK_IMPACT]  = {  3, SX_ALT,  SY_CONST, 2, { 3, 3, 3 } },           /* s02 splat, s04 insert */
    [SHK_STAMP]   = {  3, SX_ALT,  SY_CONST, 2, { 3, 2, 1 } },           /* s11 fail x1/x2, LOST  */
    [SHK_STAMP3]  = {  5, SX_ALT,  SY_CONST, 2, { 3, 3, 2, 2, 1 } },     /* s11 fail x3           */
    [SHK_SLAM]    = {  5, SX_ALT,  SY_HALF,  0, { 3, 3, 3, 3, 3 } },     /* s08 caption slams     */
    [SHK_IGNITE]  = {  5, SX_ALT,  SY_CONST, 2, { 3, 3, 3, 3, 3 } },     /* s15, s11 monster      */
    [SHK_SHOTGUN] = {  7, SX_ALT,  SY_HALF,  0, { 7, 6, 5, 4, 3, 2, 1 } },               /* s10   */
    [SHK_FWOOMP]  = {  8, SX_ALT,  SY_CONST, 2, { 4, 4, 3, 3, 2, 2, 1, 1 } },            /* s07   */
    [SHK_CRACK]   = {  8, SX_ALT,  SY_HALF,  0, { 3, 3, 3, 3, 3, 3, 3, 3 } },            /* s08   */
    [SHK_RUMBLE]  = { 12, SX_ALT,  SY_CONST, 2, { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 } },/* s13   */
    [SHK_ROAR]    = { 14, SX_ALT,  SY_HALF,  0, { 4, 4, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2, 1, 1 } },
    [SHK_GUNFIRE] = {  1, SX_ALT,  SY_CONST, 1, { 1 } },                 /* repeat while firing   */
};

#define FLASH_GAP   10      /* frames between full-screen flash starts (< 3 Hz) */
#define REDUCE_CAP  6       /* OPT_REDUCE_FLASH level cap                       */
#define PULSE_LEVEL 4       /* the darken pulse that replaces a shake at 0%     */

enum { IRIS_OFF, IRIS_STATIC, IRIS_OUT, IRIS_IN };
#define IRIS_CLOSE RUNG_R5          /* the radius move, out or in (CT sec.2: "iris close" R5) */
#define IRIS_SNAP  RUNG_R3          /* the last move to black / from black                   */


static struct {
    uint16_t now;                     /* camfx clock, +1 per camfx_tick          */
    uint16_t shk_t, flash_t, zoom_t, iris_t, last_flash;
    int8_t   shk;                     /* profile, -1 none                        */
    uint8_t  flash_color, flash_level, flash_frames;
    uint8_t  zoom_frames, fade, pal_id, pal_level;
    uint8_t  fade_from, fade_frames, pal_from, pal_frames;    /* ramps */
    uint16_t fade_t, pal_t;
    uint8_t  iris_mode, iris_heart, iris_rim, iris_hold;
    int16_t  zoom_q8, iris_cx, iris_cy, iris_r;
    TwSpring px, py;                  /* the punch (TW_SLAM4), Q4                */
} cf;

static uint16_t age_of(uint16_t t) { return (uint16_t)(cf.now - t); }

void camfx_reset(void)
{
    memset(&cf, 0, sizeof(cf));
    cf.shk = -1;
    cf.shk_t = cf.flash_t = cf.zoom_t = cf.iris_t = 0;
    cf.last_flash = (uint16_t)(0u - FLASH_GAP);
    cf.zoom_q8 = 256;
    tw_spring_init(&cf.px, TW_SLAM4, 0);
    tw_spring_init(&cf.py, TW_SLAM4, 0);
}

/* never initialised: the first use resets (the clock starts anywhere) */
static void ensure_init(void)
{
    if (cf.px.a1 == 0) camfx_reset();
}

void camfx_tick(void)
{
    ensure_init();
    cf.now++;
    tw_spring_step(&cf.px);
    tw_spring_step(&cf.py);
    if (cf.shk >= 0 && age_of(cf.shk_t) >= SHAKE[cf.shk].frames) cf.shk = -1;
    if (cf.flash_level && age_of(cf.flash_t) >= cf.flash_frames) cf.flash_level = 0;
    if (cf.zoom_frames && age_of(cf.zoom_t) >= cf.zoom_frames) { cf.zoom_frames = 0; cf.zoom_q8 = 256; }
    if (cf.iris_mode == IRIS_IN && age_of(cf.iris_t) >= IRIS_SNAP + cf.iris_hold + IRIS_CLOSE)
        cf.iris_mode = IRIS_OFF;
}

/* a level ramp's value this frame: from -> to over `frames`, reaching `to`
 * on the ramp's last frame (frames 0 = instant) */
static int ramp(int from, int to, uint16_t t, int frames)
{
    int a = age_of(t) + 1;
    if (frames <= 0 || a >= frames) return to;
    return from + (to - from) * a / frames;
}

/* ---- shake + punch ------------------------------------------------------- */
void camfx_shake(int profile)
{
    ensure_init();
    if (profile < 0 || profile >= SHK_COUNT) return;
    cf.shk = (int8_t)profile;
    cf.shk_t = cf.now;
}

int camfx_shaking(void)
{
    return cf.shk >= 0 && age_of(cf.shk_t) < SHAKE[cf.shk].frames;
}

void camfx_punch(int dx, int dy)
{
    ensure_init();
    tw_spring_punch(&cf.px, (int32_t)dx * 16);
    tw_spring_punch(&cf.py, (int32_t)dy * 16);
}

/* the raw profile sample of frame f, in trailer pixels (240x160) */
void camfx_shake_sample(int profile, int f, int *x, int *y)
{
    const ShakeProf *p;
    int a;
    *x = *y = 0;
    if (profile < 0 || profile >= SHK_COUNT) return;
    p = &SHAKE[profile];
    if (f < 0 || f >= p->frames) return;
    a = p->amp[f];
    switch (p->xmode) {
    case SX_ALT: *x = (f & 1) ? a : -a; break;
    case SX_RAW: *x = a; break;
    default: break;
    }
    switch (p->ymode) {
    case SY_HALF:  *y = (f & 2) ? a / 2 : -(a / 2); break;
    case SY_CONST: *y = (f & 2) ? p->yc : -p->yc; break;
    case SY_RAW:   *y = a; break;
    default: break;
    }
}

/* amplitude in Q4 x the option (0/50/100%) x the screen size relative to
 * the trailer's 240x160 (the PS1's 4/3 and 3/2 of CT Appendix C), rounded
 * to whole pixels away from zero so a 1 px rattle never vanishes */
static int scale_px(int v_q4, int screen, int base, int pct)
{
    int32_t s = (int32_t)v_q4 * screen / base * pct / 100;
    if (s >= 0) return (int)((s + 8) >> 4);
    return -(int)((-s + 8) >> 4);
}

static int shake_pct(void)
{
    int lvl = opt_get(OPT_SCREEN_SHAKE);
    return lvl <= 0 ? 0 : lvl == 1 ? 50 : 100;
}

void camfx_offset(int *dx, int *dy)
{
    int sx = 0, sy = 0, pct = shake_pct();
    int32_t ox, oy;
    if (cf.px.a1 == 0) { if (dx) *dx = 0; if (dy) *dy = 0; return; }
    if (camfx_shaking()) camfx_shake_sample(cf.shk, age_of(cf.shk_t), &sx, &sy);
    ox = (int32_t)sx * 16 + (cf.px.e + cf.px.target);
    oy = (int32_t)sy * 16 + (cf.py.e + cf.py.target);
    if (dx) *dx = scale_px((int)ox, plat_screen_w(), 240, pct);
    if (dy) *dy = scale_px((int)oy, plat_screen_h(), 160, pct);
}

/* ---- zoom ------------------------------------------------------------------ */
void camfx_zoom_snap(int scale_q8, int frames)
{
    ensure_init();
    if (frames <= 0 || scale_q8 <= 0) { cf.zoom_frames = 0; cf.zoom_q8 = 256; return; }
    cf.zoom_q8 = (int16_t)DOOMO_CLAMP(scale_q8, 64, 1024);
    cf.zoom_frames = (uint8_t)DOOMO_MIN(frames, 255);
    cf.zoom_t = cf.now;
}

int camfx_zoom(void) { return cf.zoom_frames ? cf.zoom_q8 : 256; }

/* ---- flash, fade, pulse ----------------------------------------------------- */
static int is_red(uint8_t color)
{
    uint16_t c = doomo_palette[color];
    int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    return r >= 12 && g * 10 <= r * 7 && b * 2 <= r;          /* red through lava orange */
}

void camfx_flash(uint8_t color, int level16, int frames)
{
    ensure_init();
    if (level16 <= 0 || frames <= 0) return;
    if (age_of(cf.last_flash) < FLASH_GAP) return;          /* photosensitivity: < 3 Hz */
    if (level16 > 16) level16 = 16;
    if (opt_get(OPT_REDUCE_FLASH)) {
        if (is_red(color)) color = PAL_BLACK;               /* a darken pulse instead  */
        if (level16 > REDUCE_CAP) level16 = REDUCE_CAP;
    }
    cf.flash_color = color;
    cf.flash_level = (uint8_t)level16;
    cf.flash_frames = (uint8_t)DOOMO_MIN(frames, 255);
    cf.flash_t = cf.last_flash = cf.now;
}

/* the flash's level this frame: a linear decay, 10 over 2 -> 10, 5;
 * 6 over 3 -> 6, 4, 2; 16 over 4 -> 16, 12, 8, 4 (CT sec.3.14) */
int camfx_flash_level(void)
{
    int a;
    if (!cf.flash_level) return 0;
    a = age_of(cf.flash_t);
    if (a >= cf.flash_frames) return 0;
    return cf.flash_level * (cf.flash_frames - a) / cf.flash_frames;
}

void camfx_fade(int level16) { camfx_fade_to(level16, 0); }

void camfx_fade_to(int level16, int frames)
{
    ensure_init();
    cf.fade_from = (uint8_t)camfx_fade_level();
    cf.fade = (uint8_t)DOOMO_CLAMP(level16, 0, 16);
    cf.fade_frames = (uint8_t)DOOMO_CLAMP(frames, 0, 255);
    cf.fade_t = cf.now;
}

int camfx_fade_level(void)
{
    return ramp(cf.fade_from, cf.fade, cf.fade_t, cf.fade_frames);
}

int camfx_blend(uint8_t *color)
{
    int f = camfx_flash_level();
    uint8_t c = cf.flash_color;
    if (!f && camfx_shaking() && shake_pct() == 0 && age_of(cf.shk_t) == 0) {
        f = PULSE_LEVEL;                                      /* OPT_SCREEN_SHAKE 0 */
        c = PAL_BLACK;
    }
    {
        int fd = camfx_fade_level();
        if (fd && (!f || (c == PAL_BLACK && fd > f))) { f = fd; c = PAL_BLACK; }
    }
    if (color) *color = c;
    return f;
}

/* ---- iris -------------------------------------------------------------------- */

static int isqrt_u(uint32_t v)
{
    uint32_t r = 0, bit = 1u << 30;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (int)r;
}

/* The open (visible) spans of row y of an iris of radius r around (cx, cy):
 * returns 0, 1 or 2 spans as half-open [x0, x1) pairs in out[]. The heart's
 * r is its half width: two lobes over a point at cy + r. Shared by every
 * platform's iris (the software default and the GBA's span fill). */
int camfx_iris_row(int y, int cx, int cy, int r, int heart, int out[4])
{
    int dy, s;
    if (r <= 0) return 0;
    if (!heart) {
        dy = y - cy;
        if (dy < 0) dy = -dy;
        if (dy >= r) return 0;
        s = isqrt_u((uint32_t)(r * r - dy * dy));
        out[0] = cx - s; out[1] = cx + s;
        return s > 0;
    } else {
        /* lobes of radius rr at (cx -+ r/2, ly); below the point P at 45
         * degrees on each lobe the sides run straight (tangent) to the tip */
        int rr = r / 2 + r / 12, ly = cy - r / 4, lx = cx - r / 2, rx = cx + r / 2;
        int tip = cy + r, py = ly + rr * 7 / 10, pw = r / 2 + rr * 7 / 10, ls = -1;
        dy = y - ly;
        if (dy > -rr && dy < rr) ls = isqrt_u((uint32_t)(rr * rr - dy * dy));
        if (y < py) {
            if (ls <= 0) return 0;
            if (y >= ly || lx + ls >= rx - ls) { out[0] = lx - ls; out[1] = rx + ls; return 1; }
            out[0] = lx - ls; out[1] = lx + ls; out[2] = rx - ls; out[3] = rx + ls;
            return 2;
        }
        if (y >= tip) return 0;
        s = pw * (tip - y) / (tip - py);
        out[0] = cx - s; out[1] = cx + s;
        if (ls > 0) { out[0] = DOOMO_MIN(out[0], lx - ls); out[1] = DOOMO_MAX(out[1], rx + ls); }
        return out[1] > out[0];
    }
}

/* the radius that uncovers the whole screen around (cx, cy) */
static int iris_full(void)
{
    int w = plat_screen_w(), h = plat_screen_h();
    int dx = DOOMO_MAX(cf.iris_cx, w - cf.iris_cx), dy = DOOMO_MAX(cf.iris_cy, h - cf.iris_cy);
    int r = isqrt_u((uint32_t)(dx * dx + dy * dy)) + 2;
    return cf.iris_heart ? r * 3 / 2 : r;
}

static void iris_set(int mode, int cx, int cy, int r, int heart, uint8_t rim, int hold)
{
    ensure_init();
    cf.iris_mode = (uint8_t)mode;
    cf.iris_cx = (int16_t)cx;
    cf.iris_cy = (int16_t)cy;
    cf.iris_r = (int16_t)DOOMO_MAX(r, 0);
    cf.iris_heart = heart ? 1 : 0;
    cf.iris_rim = rim;
    cf.iris_hold = (uint8_t)DOOMO_CLAMP(hold, 0, 255);
    cf.iris_t = cf.now;
}

void camfx_iris(int cx, int cy, int r, int heart, uint8_t rim)
{
    if (r < 0) { camfx_iris_off(); return; }
    iris_set(IRIS_STATIC, cx, cy, r, heart, rim, 0);
}

void camfx_iris_off(void) { cf.iris_mode = IRIS_OFF; }

/* [IRIS out h]: close onto (cx, cy) down to radius r over R5, hold h frames,
 * then snap to black over R3 and stay black (until camfx_iris_off / reset). */
void camfx_iris_out(int cx, int cy, int r, int heart, int hold)
{
    iris_set(IRIS_OUT, cx, cy, r, heart, 0, hold);
}

/* the reverse: open from black to r over R3, hold, open fully over R5, off */
void camfx_iris_in(int cx, int cy, int r, int heart, int hold)
{
    iris_set(IRIS_IN, cx, cy, r, heart, 0, hold);
}

static int ease_r(int from, int to, int a, int dur)
{
    return tw_lerp(from, to, tw_smooth(tw_t(a, dur)));
}

/* the iris radius this frame, -1 = no iris */
int camfx_iris_radius(void)
{
    int a = age_of(cf.iris_t), full, h = cf.iris_hold;
    switch (cf.iris_mode) {
    case IRIS_STATIC: return cf.iris_r;
    case IRIS_OUT:
        full = iris_full();
        if (a < IRIS_CLOSE) return ease_r(full, cf.iris_r, a + 1, IRIS_CLOSE);
        a -= IRIS_CLOSE;
        if (a < h) return cf.iris_r;
        a -= h;
        return a < IRIS_SNAP ? ease_r(cf.iris_r, 0, a + 1, IRIS_SNAP) : 0;
    case IRIS_IN:
        full = iris_full();
        if (a < IRIS_SNAP) return ease_r(0, cf.iris_r, a + 1, IRIS_SNAP);
        a -= IRIS_SNAP;
        if (a < h) return cf.iris_r;
        a -= h;
        return a < IRIS_CLOSE ? ease_r(cf.iris_r, full, a + 1, IRIS_CLOSE) : -1;
    default: return -1;
    }
}

/* ---- palette lerps (SF sec.15.5) ---------------------------------------------
 * The targets are computed from the master palette, not stored (saves the
 * 2 KB of the SPEC's budget): sepia for flashbacks, crayon for P0 and the
 * confession bleed, hellify for inserts, lilac for the one-frame W13. */
void camfx_pal_lerp(int pal_id, int level16) { camfx_pal_lerp_to(pal_id, level16, 0); }

/* Ramp toward pal_id at level16 over `frames`. A ramp to 0 keeps the
 * current palette while it fades out; a different id starts from 0. */
void camfx_pal_lerp_to(int pal_id, int level16, int frames)
{
    int cur, lvl;
    ensure_init();
    cur = camfx_pal_state(&lvl);
    if (pal_id <= PAL_LERP_NONE || pal_id >= PAL_LERP_COUNT) { pal_id = cur; level16 = 0; }
    if (level16 <= 0 && cur == PAL_LERP_NONE) { cf.pal_id = PAL_LERP_NONE; cf.pal_level = 0; cf.pal_frames = 0; return; }
    if (cur != PAL_LERP_NONE && cur != pal_id && level16 > 0) lvl = 0;
    cf.pal_id = (uint8_t)pal_id;
    cf.pal_from = (uint8_t)lvl;
    cf.pal_level = (uint8_t)DOOMO_CLAMP(level16, 0, 16);
    cf.pal_frames = (uint8_t)DOOMO_CLAMP(frames, 0, 255);
    cf.pal_t = cf.now;
}

static int lerp5(int a, int b, int l16) { return a + (b - a) * l16 / 16; }

static uint16_t pal_target(int pal_id, uint16_t c)
{
    int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    int y = (r * 77 + g * 150 + b * 29) >> 8;                /* luma, 0..31 */
    switch (pal_id) {
    case PAL_SEPIA:                       /* warm brown-cream photograph       */
        r = DOOMO_MIN(31, y + y / 8 + 2); g = y * 7 / 8 + 1; b = y * 9 / 16 + 1;
        break;
    case PAL_CRAYON:                      /* chalky pastel on paper, posterised */
        r = ((r * 5 + 30 * 3) / 8) & ~3; g = ((g * 5 + 28 * 3) / 8) & ~3; b = ((b * 5 + 24 * 3) / 8) & ~3;
        r |= 2; g |= 2; b |= 1;
        break;
    case PAL_HELLIFY:                     /* blood and ember                    */
        r = DOOMO_MIN(31, y * 3 / 2 + 3); g = y / 3; b = y / 5;
        break;
    case PAL_LILAC:                       /* the fountain's one wrong frame     */
        r = (r + 25) / 2; g = (g + 18) / 2; b = (b + 31) / 2;
        break;
    default: break;
    }
    return (uint16_t)(r | (g << 5) | (b << 10));
}

uint16_t camfx_pal_entry(int i, int pal_id, int pal_level, uint8_t color, int level16)
{
    uint16_t c = doomo_palette[i & 255];
    if (pal_id > PAL_LERP_NONE && pal_id < PAL_LERP_COUNT && pal_level > 0) {
        uint16_t t = pal_target(pal_id, c);
        int l = DOOMO_MIN(pal_level, 16);
        c = (uint16_t)(lerp5(c & 31, t & 31, l) | (lerp5((c >> 5) & 31, (t >> 5) & 31, l) << 5) |
                       (lerp5((c >> 10) & 31, (t >> 10) & 31, l) << 10));
    }
    if (level16 > 0) {
        uint16_t t = doomo_palette[color];
        int l = DOOMO_MIN(level16, 16);
        c = (uint16_t)(lerp5(c & 31, t & 31, l) | (lerp5((c >> 5) & 31, (t >> 5) & 31, l) << 5) |
                       (lerp5((c >> 10) & 31, (t >> 10) & 31, l) << 10));
    }
    return c;
}

int camfx_pal_state(int *level16)
{
    int l = ramp(cf.pal_from, cf.pal_level, cf.pal_t, cf.pal_frames);
    if (level16) *level16 = l;
    return l ? cf.pal_id : PAL_LERP_NONE;
}

/* ---- push to the platform ------------------------------------------------------ */
void camfx_apply(void)
{
    int dx, dy, lvl, r;
    uint8_t c;
    if (cf.px.a1 == 0) return;                 /* never used */
    camfx_offset(&dx, &dy);
    if (dx || dy) plat_post_offset(dx, dy);
    if (camfx_zoom() != 256)
        plat_post_scale(camfx_zoom(), camfx_zoom(), plat_screen_w() / 2, plat_screen_h() / 2);
    {
        int pl, pid = camfx_pal_state(&pl);
        if (pid != PAL_LERP_NONE) plat_pal_lerp(pid, pl);
    }
    r = camfx_iris_radius();
    if (r >= 0) plat_post_iris(cf.iris_cx, cf.iris_cy, r, cf.iris_heart, cf.iris_rim);
    lvl = camfx_blend(&c);
    if (lvl > 0) plat_post_blend(c, lvl);
}
