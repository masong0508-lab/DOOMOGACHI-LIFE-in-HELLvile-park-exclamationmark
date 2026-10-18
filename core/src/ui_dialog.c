/* ui_dialog.c - SCENE_DIALOG: Tomodachi-style typewriter dialogue with the
 * speaker's portrait, a name plate, voice babble, multiple-choice menus
 * and a queue (owner: foundation; the comic-timing upgrade is S1 M3,
 * docs/design/story/SPEC.md sec.7 S1, story_final.md sec.15.6,
 * comic_timing.md sec.6.2).
 *
 *   ui_say(id, "Hi!"); ui_say(id, "Want cake?");
 *   ui_choice(id, "Pick one:", opts, 3, on_pick);   // queued after
 *   ui_say_done(on_finished);                         // after the last
 *   ui_say_ex(id, EXPR_SHOCK, DL_SHOUT, "WHAT.{..} / Who ate it?");
 *
 * Long lines are paged automatically. A / B complete the line, then
 * advance. Callbacks run after the dialogue box has closed (when the
 * queue is empty), so they may push scenes. Every line is copied into its
 * 200-character queue entry, so nothing resident points into a caller's
 * strings (the PS1 overlays, SPEC C6).
 *
 * BEAT CODES (parsed when a line is queued, stripped from the text):
 *   {p N} pause N frames   {.} {..} {...} pause R5 / R8 / R13
 *   {d}  deadpan for the rest of the line: it appears whole, no hop, no
 *        babble, the portrait clock holds, the music ducks to 0.5
 *   {!}  a sting: SHK_POW, a "!" bang over the speaker, a 1-frame flash
 *   {big}word{/}  the word pops whole (TW_POP_BUBBLE)
 *   {w}  the rest of the line waves   {r} record scratch (tape-stop 5,
 *        14 frames of silence, back on the beat)   {cut N} a 4-frame smash
 *        flash (the host scene draws insert N; ui_dialog_insert())
 *   {punch} a punchline: A cannot rush it, its holds are unskippable
 * ui_say_ex() lines (story text) also take "/" as a line break and "//"
 * as a page break, and their DL_* flags (txt.h): DEADPAN, PUNCH, PILL (the
 * name pill: drawn only on PILL lines, popping in with TW_POP_BUBBLE),
 * SHOUT (the portrait stretches, then jellies), HEART, GURGLE (the belly's
 * secondary portrait slot and voice). Legacy ui_say() lines keep the
 * name plate on every portrait line.
 *
 * PUNCTUATION HOLDS (CT sec.6.2): ',' 3, '.' 6, '!' 4 (the box hops), '?'
 * 5 (the portrait tilts), '...' 5/5/5, and a line break after '...' R8.
 * Each glyph hops in with TW_HOP1, scaled by OPT_UI_BOUNCE. A new line
 * starts with a talk bob of the portrait; possessed speakers twitch.
 */
#include "ui_internal.h"
#include "doomo/tween.h"
#include "doomo/beat.h"
#include "doomo/camfx.h"
#include "doomo/txt.h"

#define DLG_Q 8
#define DLG_TEXT 200
#define DLG_OPTS 6
#define DLG_OPT_LEN 28
#define DLG_OPT_SLOTS 2        /* choices queued at once (each 168 B)  */
#define DLG_CODES 10
#define HOP_RING 16            /* glyphs that may still be hopping       */

enum { DK_SAY, DK_CHOICE };
/* beat codes, by text position */
enum { C_PAUSE = 1, C_DEAD, C_STING, C_BIG, C_ENDBIG, C_WAVE, C_SCRATCH, C_CUT, C_PAGE };
/* entry flags (beyond DL_*, stored separately) */
enum { EF_LEGACY = 1, EF_PUNCH = 2 };

typedef struct { uint8_t pos, op, arg; } DlgCode;

typedef struct {
    int16_t speaker;
    uint8_t kind, nopts, expr, flags, eflags, ncode;
    int8_t  opt_slot;
    DlgCode code[DLG_CODES];
    char text[DLG_TEXT];
    void (*choice_done)(int choice);
    void (*say_done)(void);
} DlgEntry;

static DlgEntry dq[DLG_Q];
static char d_opts[DLG_OPT_SLOTS][DLG_OPTS][DLG_OPT_LEN];
static uint8_t d_opt_used;             /* bit per options slot              */
static int dq_n;
static int d_page;                     /* byte offset of the current page */
static int d_shown;                    /* chars shown on the page, x2 */
static int d_line_t;                   /* ticks since the page started */
static int d_sel;
static int d_style = WIN_CUTE;
static uint8_t d_open;
/* the comic typewriter */
static uint8_t d_hold;                 /* frames before the next glyph       */
static uint8_t d_dead, d_wave_from, d_big_from, d_big_to, d_box_hop, d_tilt, d_resume;
static int d_applied;                  /* codes before this text pos ran     */
static uint8_t d_punch_held;           /* the punch's closing hold started   */
static int16_t d_bang;                 /* frames of the "!" bang (-1 none)    */
static uint8_t d_insert, d_insert_t;
static uint16_t d_rev[HOP_RING];       /* d_line_t when glyph i appeared     */
static uint8_t d_pill_seen;            /* the pill has popped in this run    */
static uint32_t d_pill_draws;          /* tests                              */
static uint16_t d_serial;              /* bumps with every line shown        */

void ui_dialog_style(int style) { d_style = style; }
int ui_dialog_active(void) { return d_open; }
int ui_dialog_queued(void) { return dq_n; }

static void copy_str(char *dst, const char *src, int cap)
{
    int i;
    if (!src) src = "";
    for (i = 0; i < cap - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

/* ---- parsing the beat codes ------------------------------------------- */
/* {keyword}: op and argument; a keyword ending in ' ' takes a number */
static const struct { char kw[7]; uint8_t op, arg; } code_kw[] = {
    { "p ", C_PAUSE, 0 }, { "...", C_PAUSE, RUNG_R13 }, { "..", C_PAUSE, RUNG_R8 },
    { ".", C_PAUSE, RUNG_R5 }, { "d", C_DEAD, 0 }, { "!", C_STING, 0 }, { "big", C_BIG, 0 },
    { "/", C_ENDBIG, 0 }, { "w", C_WAVE, 0 }, { "r", C_SCRATCH, 0 }, { "punch", 0, 1 },
    { "cut ", C_CUT, 0 }
};

static void add_code(DlgEntry *e, int pos, int op, int arg)
{
    if (e->ncode >= DLG_CODES) return;
    e->code[e->ncode].pos = (uint8_t)DOOMO_MIN(pos, 255);
    e->code[e->ncode].op = (uint8_t)op;
    e->code[e->ncode].arg = (uint8_t)DOOMO_CLAMP(arg, 0, 255);
    e->ncode++;
}

/* A {code} at src: add it, return the characters it spans (0 = not one). */
static int parse_code(DlgEntry *e, const char *src, int pos)
{
    unsigned k;
    for (k = 0; k < sizeof(code_kw) / sizeof(code_kw[0]); k++) {
        const char *q = src + 1, *w = code_kw[k].kw;
        int arg = code_kw[k].arg;
        while (*w && *q == *w) { q++; w++; }
        if (*w) continue;
        if (q[-1] == ' ') for (arg = 0; *q >= '0' && *q <= '9'; q++) arg = arg * 10 + (*q - '0');
        if (*q != '}') continue;
        if (code_kw[k].op) add_code(e, pos, code_kw[k].op, arg);
        else e->eflags |= EF_PUNCH;
        return (int)(q - src) + 1;
    }
    return 0;
}

/* Copy src into e->text, stripping {codes}. story: '/' is a line break,
 * '//' a page break. */
static void parse_text(DlgEntry *e, const char *src, int story)
{
    int o = 0, n;
    if (!src) src = "";
    while (*src && o < DLG_TEXT - 1) {
        if (*src == '{' && (n = parse_code(e, src, o)) != 0) { src += n; continue; }
        if (story && *src == '/') {
            while (o > 0 && e->text[o - 1] == ' ') o--;
            if (src[1] == '/') { add_code(e, o, C_PAGE, 0); src++; }
            else e->text[o++] = '\n';
            src++;
            while (*src == ' ') src++;
            continue;
        }
        e->text[o++] = *src++;
    }
    e->text[o] = 0;
}

static DlgEntry *enqueue(int speaker, int kind, const char *text)
{
    DlgEntry *e;
    if (!d_open) { dq_n = 0; d_opt_used = 0; }        /* lines left by a scene reset */
    if (dq_n >= DLG_Q) {
        plat_log("ui_say: dialogue queue full, dropping a line");
        return NULL;
    }
    e = &dq[dq_n++];
    memset(e, 0, sizeof(*e));
    e->speaker = (int16_t)speaker;
    e->kind = (uint8_t)kind;
    e->expr = EXPR_NEUTRAL;
    e->eflags = EF_LEGACY;
    e->opt_slot = -1;
    parse_text(e, text, 0);
    if (!d_open) scene_push(&SCENE_DIALOG, 0);
    return e;
}

void ui_say(int speaker, const char *text)
{
    enqueue(speaker, DK_SAY, text);
}

void ui_say_ex(int speaker, int expr, int flags, const char *text)
{
    DlgEntry *e = enqueue(speaker, DK_SAY, NULL);
    if (!e) return;
    e->eflags = 0;
    e->expr = (uint8_t)(expr >= 0 && expr < EXPR_COUNT ? expr : EXPR_NEUTRAL);
    e->flags = (uint8_t)flags;
    e->ncode = 0;
    parse_text(e, text, 1);
    if (flags & DL_PUNCH) e->eflags |= EF_PUNCH;
    if (flags & DL_DEADPAN) add_code(e, 0, C_DEAD, 0);
}

void ui_say_done(void (*done)(void))
{
    if (dq_n > 0) dq[dq_n - 1].say_done = done;
    else if (done) done();
}

void ui_choice(int speaker, const char *question, const char *const *options,
               int count, void (*done)(int choice))
{
    DlgEntry *e = enqueue(speaker, DK_CHOICE, question);
    int i, slot;
    if (!e) return;
    for (slot = 0; slot < DLG_OPT_SLOTS && (d_opt_used & (1 << slot)); slot++) {}
    if (slot >= DLG_OPT_SLOTS) {
        plat_log("ui_choice: too many choices queued, dropping one");
        dq_n--;
        return;
    }
    d_opt_used |= (uint8_t)(1 << slot);
    e->opt_slot = (int8_t)slot;
    if (count > DLG_OPTS) count = DLG_OPTS;
    if (count < 1) count = 1;
    e->nopts = (uint8_t)count;
    for (i = 0; i < count; i++) copy_str(d_opts[slot][i], options ? options[i] : "OK", DLG_OPT_LEN);
    e->choice_done = done;
}

static const char *opt_text(const DlgEntry *e, int i)
{
    return e->opt_slot >= 0 ? d_opts[(int)e->opt_slot][i] : "";
}

/* ---------------------------------------------------------------------- */
static int has_portrait(int speaker)
{
    const DoomoResident *r;
    if (speaker < 0) return 0;
    r = town_res(speaker);
    return r && r->status != RES_EMPTY;
}

/* Geometry of the box for the current screen. */
typedef struct { int x, y, w, h, tx, tw, lines; } DlgBox;

static void dlg_box(const DlgEntry *e, DlgBox *b)
{
    int sw = plat_screen_w(), sh = plat_screen_h();
    b->lines = sh >= 200 ? 4 : 3;
    b->h = b->lines * FONT_LINE + 13;
    b->x = 4;
    b->w = sw - 8;
    b->y = sh - b->h - 4;
    if (has_portrait(e->speaker)) { b->tx = b->x + 60; b->tw = b->w - 70; }
    else { b->tx = b->x + 10; b->tw = b->w - 20; }
}

/* the next page break at or after the page start, -1 if none */
static int next_page_code(const DlgEntry *e)
{
    int i, best = -1;
    for (i = 0; i < e->ncode; i++)
        if (e->code[i].op == C_PAGE && e->code[i].pos > d_page &&
            (best < 0 || e->code[i].pos < best)) best = e->code[i].pos;
    return best;
}

static int page_len(const DlgEntry *e, const DlgBox *b)
{
    int n = ui_wrap_skip(e->text + d_page, b->tw, b->lines), pg = next_page_code(e);
    if (pg >= 0 && pg - d_page < n) n = pg - d_page;
    return n;
}

static int page_is_last(const DlgEntry *e, const DlgBox *b)
{
    return e->text[d_page + page_len(e, b)] == 0;
}

static void reset_page(int offset)
{
    d_page = offset;
    d_shown = 0;
    d_line_t = 0;
    d_sel = 0;
    d_hold = 0;
    d_wave_from = d_big_from = d_big_to = 0xFF;
    d_box_hop = d_tilt = 0;
    d_bang = -1;
    d_applied = offset;
    d_punch_held = 0;
    if (offset == 0) d_dead = 0;
}

static void dlg_enter(int arg)
{
    (void)arg;
    d_open = 1;
    d_pill_seen = 0;
    reset_page(0);
}

static void dlg_leave(void) { d_open = 0; }

static void advance(int choice)
{
    DlgEntry done = dq[0];
    int i;
    if (done.opt_slot >= 0) d_opt_used &= (uint8_t)~(1 << done.opt_slot);
    for (i = 1; i < dq_n; i++) dq[i - 1] = dq[i];
    dq_n--;
    reset_page(0);
    if (d_dead) audio_music_duck(256, 0);
    d_dead = 0;
    d_serial++;
    if (dq_n == 0) scene_pop();
    if (done.kind == DK_CHOICE && done.choice_done) done.choice_done(choice);
    if (done.say_done) done.say_done();
}

/* Apply the codes at text position pos (page-relative glyph about to show). */
static void apply_codes(DlgEntry *e, int pos)
{
    int i;
    if (pos < d_applied) return;                        /* once per position */
    d_applied = pos + 1;
    for (i = 0; i < e->ncode; i++) {
        const DlgCode *c = &e->code[i];
        if (c->pos != pos) continue;
        switch (c->op) {
        case C_PAUSE: if (!d_dead) d_hold = (uint8_t)DOOMO_MIN(d_hold + c->arg, 255); break;
        case C_DEAD:
            if (!d_dead) audio_music_duck(128, 30000);
            d_dead = 1;
            break;
        case C_STING:
            camfx_shake(SHK_POW);
            camfx_flash(PAL_WHITE, 5, 1);
            d_bang = 0;
            break;
        case C_BIG: {
            int j;
            d_big_from = (uint8_t)(pos - d_page);
            d_big_to = (uint8_t)DOOMO_MIN(strlen(e->text) - (size_t)d_page, 255u);
            for (j = 0; j < e->ncode; j++)
                if (e->code[j].op == C_ENDBIG && e->code[j].pos >= pos) {
                    d_big_to = (uint8_t)(e->code[j].pos - d_page);
                    break;
                }
            break; }
        case C_WAVE: d_wave_from = (uint8_t)(pos - d_page); break;
        case C_SCRATCH:
            audio_sfx_synth(SFX_SCRATCH);
            audio_tapestop(5);
            d_resume = 5 + 14;
            break;
        case C_CUT:
            d_insert = c->arg;
            d_insert_t = 4;
            camfx_flash(PAL_WHITE, 10, 1);
            audio_music_mute(4);
            break;
        default: break;
        }
    }
}

/* the hold after revealing glyph i of the page (CT sec.6.2) */
static int punct_hold(const DlgEntry *e, int i, int len)
{
    const char *p = e->text + d_page;
    char c = p[i];
    if (i + 1 >= len) return 0;                         /* nothing left to wait for */
    switch (c) {
    case ',': return 3;
    case '!': d_box_hop = 3; return 4;
    case '?': d_tilt = 5; return 5;
    case '.':
        if (i > 0 && p[i - 1] == '.' && p[i + 1] != '.') {     /* the end of a "..." */
            const char *q = p + i + 1;
            while (*q == ' ') q++;
            return *q == '\n' ? RUNG_R8 : 5;
        }
        if (p[i + 1] == '.') return 5;                  /* inside a "..."    */
        return 6;
    default: return 0;
    }
}

static int line_punch(const DlgEntry *e) { return (e->eflags & EF_PUNCH) != 0; }

/* reveal one glyph; returns 0 when the page is fully shown */
static int reveal_one(DlgEntry *e, int len)
{
    int i = d_shown / 2, h;
    if (i >= len) return 0;
    apply_codes(e, d_page + i);
    if (d_hold) return 1;                               /* a {p N} landed first */
    d_rev[i & (HOP_RING - 1)] = (uint16_t)d_line_t;
    d_shown = (i + 1) * 2;
    if (d_big_from != 0xFF && i >= d_big_from && i < d_big_to) {   /* the word pops whole */
        int j;
        for (j = i + 1; j < d_big_to && j < len; j++) d_rev[j & (HOP_RING - 1)] = (uint16_t)d_line_t;
        d_shown = DOOMO_MIN(d_big_to, len) * 2;
        i = d_shown / 2 - 1;
    }
    h = punct_hold(e, i, len);
    if (!d_dead) d_hold = (uint8_t)DOOMO_MIN(d_hold + h, 255);
    return 1;
}

static void babble(DlgEntry *e, int from, int to, int len)
{
    static const uint8_t narrator[VOICE_COUNT] = { 16, 18, 10, 16, 8 };
    const DoomoResident *r;
    const uint8_t *voice;
    int i;
    if (!opt_get(OPT_VOICE_BABBLE) || d_dead) return;
    r = has_portrait(e->speaker) ? town_res(e->speaker) : NULL;
    voice = (e->flags & DL_GURGLE) ? audio_voice_preset(VOICE_GURGLE) : r ? r->voice : narrator;
    if (!voice) voice = narrator;
    for (i = from; i < to && i < len; i++) {
        char c = e->text[d_page + i];
        if (c != ' ' && c != '\n') audio_voice_char(voice, c);
    }
}

static void dlg_update(void)
{
    DlgEntry *e;
    DlgBox b;
    int len, speed, before, steps, punch, a;
    if (dq_n == 0) { scene_pop(); return; }
    e = &dq[0];
    dlg_box(e, &b);
    len = page_len(e, &b);
    punch = line_punch(e);
    d_line_t++;
    if (d_box_hop) d_box_hop--;
    if (d_tilt) d_tilt--;
    if (d_bang >= 0 && ++d_bang > 12) d_bang = -1;
    if (d_insert_t) d_insert_t--;
    if (d_resume && --d_resume == 0) audio_music_resume_on_beat();
    a = btn_pressed(BTN_A | BTN_B);
    /* typewriter */
    speed = opt_get(OPT_TEXT_SPEED);
    before = d_shown / 2;
    if (before < len) apply_codes(e, d_page + before);  /* {d}, {p N} land before the glyph */
    if (d_dead || (speed >= 3 && !punch)) {
        while (d_shown / 2 < len) {                     /* appears whole */
            int was = d_shown;
            d_hold = 0;
            reveal_one(e, len);
            if (d_shown == was) break;
        }
        d_hold = 0;
    } else {
        steps = speed == 0 ? (d_line_t & 1) : speed == 1 ? 1 : speed == 2 ? 2 : 4;
        if (d_hold) { d_hold--; steps = 0; }            /* a hold of n costs n frames */
        while (steps-- > 0 && !d_hold && reveal_one(e, len)) {}
    }
    if (punch && d_shown / 2 >= len && !d_punch_held) {  /* the punch holds R13, unskippable */
        d_punch_held = 1;
        d_hold = RUNG_R13;
    }
    if (d_shown > len * 2) d_shown = len * 2;
    babble(e, before, d_shown / 2, len);
    if (d_shown / 2 < len) {
        if (a && !punch) {                              /* A completes the line */
            int guard = DLG_TEXT;
            while (d_shown / 2 < len && guard-- > 0) { d_hold = 0; reveal_one(e, len); }
            d_hold = 0;
        }
        return;
    }
    if (punch && d_hold) { d_hold--; return; }          /* the punch hold is the joke */
    if (!page_is_last(e, &b)) {
        if (a) {
            audio_sfx(SFX_BLIP);
            reset_page(d_page + len);
            while (e->text[d_page] == ' ' || e->text[d_page] == '\n') d_page++;
        }
        return;
    }
    if (e->kind == DK_CHOICE) {
        if (btn_repeat(BTN_UP)) { d_sel = (d_sel + e->nopts - 1) % e->nopts; audio_sfx(SFX_CURSOR); }
        if (btn_repeat(BTN_DOWN)) { d_sel = (d_sel + 1) % e->nopts; audio_sfx(SFX_CURSOR); }
        if (btn_pressed(BTN_A)) { audio_sfx(SFX_SELECT); advance(d_sel); }
        return;
    }
    if (a) {
        audio_sfx(SFX_BLIP);
        advance(0);
    }
}

/* ---- drawing ------------------------------------------------------------ */
/* The portrait: ui_portrait's frame and image, the image squashed (Q8,
 * anchored at its feet) through plat_image_xform while it squashes. */
static void dlg_portrait(int x, int y, int id, int expr, int sx, int sy)
{
    const DoomoImage *img;
    ui_portrait(x, y, id, 1, expr);
    if (sx == 256 && sy == 256) return;
    img = portrait_image(id, expr);
    if (!img) return;
    plat_rect(x + 3, y + 3, img->w, img->h, ui_style(WIN_MINT)->fill_hi);
    plat_image_xform(x + 3 + img->w / 2, y + 3 + img->h, img, sx, sy, 0, 0);
}

/* Glyph g's vertical offset (the hop, the wave, the {big} pop) and colour. */
static int glyph_dy(int g, int shown, uint8_t *c, uint8_t accent)
{
    int dy = 0, age = d_line_t - d_rev[g & (HOP_RING - 1)];
    int fresh = !d_dead && g >= shown - HOP_RING && age >= 0 && age < 6;
    if (fresh) dy = tw_tab(&TW_HOP1, age) * tw_bounce_amount() / 256;
    if (d_wave_from != 0xFF && g >= d_wave_from) dy += ui_wave(d_line_t + g * 3, 16, 1);
    if (d_big_from != 0xFF && g >= d_big_from && g < d_big_to) {
        dy -= (tw_tab(&TW_POP_BUBBLE, fresh ? age : 9) - 256) / 24;
        *c = accent;
    }
    return dy;
}

/* The page, line by line (ui.c's wrap), in runs of glyphs that share an
 * offset and a colour: settled text is one ui_text per line. */
static void dlg_text(const DlgEntry *e, const DlgBox *b, int by, uint8_t col, uint8_t accent)
{
    const char *p = e->text + d_page;
    char run[64];
    int shown = d_shown / 2, pos = 0, l;
    for (l = 0; l < b->lines + 1 && pos < shown && p[pos]; l++) {
        int adv = ui_wrap_skip(p + pos, b->tw, 1), n = adv, k = 0;
        if (adv <= 0) break;
        while (n > 0 && (p[pos + n - 1] == ' ' || p[pos + n - 1] == '\n')) n--;
        if (n > shown - pos) n = shown - pos;
        if (n > 63) n = 63;
        while (k < n) {
            uint8_t c = col, c2;
            int dy = glyph_dy(pos + k, shown, &c, accent), j = k + 1;
            while (j < n && (c2 = col, glyph_dy(pos + j, shown, &c2, accent) == dy) && c2 == c) j++;
            memcpy(run, p + pos + k, (size_t)(j - k));
            run[j - k] = 0;
            ui_text(b->tx + k * FONT_ADVANCE, by + l * FONT_LINE + dy, run, c);
            k = j;
        }
        pos += adv;
    }
}

static void dlg_draw(void)
{
    static const int16_t jelly[7] = { 270, 248, 262, 252, 258, 254, 256 };
    DlgEntry *e;
    DlgBox b;
    const UiStyle *st;
    int t = (int)scene_ticks(), len, slide, portrait, pill;
    if (dq_n == 0) return;
    e = &dq[0];
    dlg_box(e, &b);
    len = page_len(e, &b);
    portrait = has_portrait(e->speaker);
    /* the box slides up with a little overshoot when the dialogue opens */
    slide = t < 8 ? (8 - t) * (8 - t) / 2 - (t >= 5 ? 2 : 0) : 0;
    b.y += slide;
    if (d_box_hop) b.y += d_box_hop == 3 ? -2 : d_box_hop == 2 ? 1 : 0;   /* '!' hops the box */
    ui_window(b.x, b.y, b.w, b.h, portrait ? d_style : WIN_CREAM);
    st = ui_style(portrait ? d_style : WIN_CREAM);
    dlg_text(e, &b, b.y + 8, st->text, st->accent);

    pill = (e->eflags & EF_LEGACY) || (e->flags & DL_PILL);
    if (portrait) {
        const DoomoResident *r = town_res(e->speaker);
        int talking = d_shown / 2 < len;
        int clock = d_dead ? 0 : d_line_t;
        int expr = talking && (clock / 4) % 2 ? EXPR_TALK : e->expr;
        int bob = d_dead ? 0 : talking ? ((d_line_t / 4) % 2) : (ui_wave(t, 60, 1) > 0);
        int sx = 256, sy = 256, px = b.x + 2;
        int nw = font_text_width(r->name) + 14;
        if (r->status == RES_POSSESSED) {
            expr = EXPR_POSSESSED;
            if (!d_dead && (d_line_t % 7) < 3) px += 1;                 /* TWITCH */
        }
        if (d_page == 0 && d_line_t < 2 && !(e->eflags & EF_LEGACY)) { sx = 271; sy = 241; }  /* talk bob */
        if ((e->flags & DL_SHOUT) && d_page == 0) {
            if (d_line_t < 2) { sx = 236; sy = 282; }
            else if (d_line_t - 2 < 7) { sx = jelly[d_line_t - 2]; sy = 512 - sx; }
        }
        if (d_tilt) px += (d_tilt & 2) ? 1 : -1;                       /* '?' tilts */
        sx = 256 + (sx - 256) * tw_bounce_amount() / 256;
        sy = 256 + (sy - 256) * tw_bounce_amount() / 256;
        dlg_portrait(px, b.y - 22 - bob, e->speaker, expr, sx, sy);
        if (e->flags & DL_GURGLE) {                    /* GURGLE's slot: the belly talks */
            ui_fill_circle(b.x + 50, b.y + 8, 9, PAL_RAMP(RAMP_BLOOD, 4));
            ui_fill_ellipse(b.x + 50, b.y + 11, 4, talking && (d_line_t & 2) ? 2 : 1, PAL_RAMP(RAMP_BLOOD, 0));
        }
        if (d_bang >= 0) {
            int s = tw_tab(&TW_POP_BUBBLE, d_bang);
            ui_text_outline(b.x + 26, b.y - 34 - (s - 256) / 16, "!", PAL_UI_GOLD, PAL_BLACK);
        }
        /* name plate (legacy lines) / the name pill (PILL lines) */
        if (pill) {
            int pop = 256, w2, x2;
            if (!(e->eflags & EF_LEGACY)) {
                if (!d_pill_seen && d_line_t == 1) d_pill_seen = 1;
                pop = d_page == 0 ? tw_tab(&TW_POP_BUBBLE, d_line_t) : 256;
                d_pill_draws++;
            }
            w2 = nw * pop / 256;
            x2 = b.x + 60 + (nw - w2) / 2;
            if (w2 > 6) {
                ui_pill_shadow(x2 + 1, b.y - 8, w2, 12, x2, b.y - 10, w2, 12);
                ui_pill(x2, b.y - 10, w2, 12, st->title_bg, st->outline, ui_lighter(st->title_bg));
                if (pop >= 230) ui_text_shadow(b.x + 67, b.y - 8, r->name, st->title, st->outline);
            }
        }
    }

    if (d_shown / 2 >= len && !(line_punch(e) && d_hold)) {
        if (!page_is_last(e, &b) || e->kind == DK_SAY) {
            int bx = b.x + b.w - 12, by = b.y + b.h - 10 + (ui_wave(t, 18, 2) > 0);
            ui_heart(bx, by, 3, st->accent);
        }
        if (page_is_last(e, &b) && e->kind == DK_CHOICE) {
            int i, ow = 0, cw, ch, cx, cy;
            for (i = 0; i < e->nopts; i++)
                ow = DOOMO_MAX(ow, font_text_width(opt_text(e, i)));
            cw = ow + 28;
            ch = e->nopts * UI_LIST_ROW_H + 10;
            cx = b.x + b.w - cw - 2;
            cy = b.y - ch - 4;
            if (cy < 2) cy = 2;
            if (ui_window_pop(cx, cy, cw, ch, d_style, d_line_t - len / 2)) {
                const UiStyle *cs = ui_style(d_style);
                for (i = 0; i < e->nopts; i++) {
                    int ry = cy + 5 + i * UI_LIST_ROW_H;
                    if (i == d_sel) {
                        ui_pill(cx + 13, ry, cw - 18, UI_LIST_ROW_H - 1, cs->accent_d,
                                ui_darker(cs->accent_d), ui_lighter(cs->accent_d));
                        ui_cursor(cx + 4, ry + 2);
                    }
                    ui_text(cx + 17, ry + 2, opt_text(e, i), cs->text);
                }
            }
        }
    }
}

/* ---- S1 hooks for hosts and tests ------------------------------------------- */
int ui_dialog_insert(int *age)
{
    if (!d_insert_t) return -1;
    if (age) *age = 4 - d_insert_t;
    return d_insert;
}

int ui_dialog_top(UiDialogTop *o)
{
    if (!d_open || !dq_n) return 0;
    o->flags = dq[0].flags;
    o->shown = d_shown / 2;
    o->serial = (uint16_t)(d_serial + d_page);
    return 1;
}

#if !defined(DOOMO_PLATFORM_GBA) && !defined(DOOMO_PLATFORM_PS1)
void ui_dialog_debug(UiDialogDebug *o)
{
    memset(o, 0, sizeof(*o));
    if (!dq_n) return;
    o->text = dq[0].text;
    o->shown = d_shown / 2;
    o->page = d_page;
    o->hold = d_hold;
    o->deadpan = d_dead;
    o->punch = line_punch(&dq[0]);
    o->pill_draws = d_pill_draws;
    o->flags = dq[0].flags;
    o->expr = dq[0].expr;
}
#endif

const DoomoScene SCENE_DIALOG = { "DIALOG", dlg_enter, dlg_leave, dlg_update, dlg_draw, 1 };
