/* dream.c - SCENE_DREAM: peek into a sleeping citizen's dream (owner:
 * concert; drawing in dream_gfx.c).
 *
 *   peek       a cosy night bedroom: the sleeper snoozes under a quilt in
 *              their favourite colour, Zzz float up, the moon twinkles
 *   enter      little thought bubbles rise and one swells to fill the
 *              screen
 *   dream      three short surreal vignettes (16 templates: raining food
 *              that grows teeth, the crush as a giant plush, falling UP
 *              into the vape cloud, a parade of their fears in party
 *              hats ...) mixing their likes, fears, friends and Hellvile
 *              horrors; each narrated in one caption line whose twist
 *              types out as the picture turns from cute to creepy
 *   nightmare  rarely (more often for corrupted, scared or Hellvile
 *              sleepers, always for the possessed and for a citizen with
 *              a nightmare problem) the last vignette goes red: wake them
 *              up (a hug, a bit tired) or let them sleep (corruption +6)
 *   after      back in the bedroom, the result
 *
 * The plan is deterministic per sleeper + day. Every vignette seen goes in
 * the dream journal (unlock bits), which unlocks creator parts.
 *
 * --- S2: the scripted dreams (SPEC sec.7 S2, D1; arg DREAM_ARG_SCRIPTED(n),
 * story_vm.h) --- the story's own dreams: YOU dream, and the Peek is forced
 * (no tiptoeing away; it drifts in by itself). The vignettes are the row's,
 * the captions are the lines the script stashed before the push (txt_stash
 * 0..2; empty = the vignette's own caption, stash 3 = the wake-up line),
 * and they arrive as floating letters, one glyph after another rising into
 * place. No stats change and the journal is not written: it is the plot's.
 * --- end S2 ---
 */
#include "concert_int.h"
#include "doomo/story_vm.h"
#include "doomo/txt.h"

enum { DS_PEEK, DS_ENTER, DS_DREAM, DS_WIPE, DS_CHOICE, DS_AFTER };

#define VIG_TICKS 180
#define ENTER_TICKS 46
#define WIPE_TICKS 18

static struct {
    int state, st_t, id, nightmare, vi, choice, outcome, prev_song, problem, was_awake;
    int scripted;                    /* S2: 1 + the scripted dream row, 0 = a Peek */
    uint8_t plan[DREAM_SHOW];
    DreamCast cast;
    char cap[112];
    int split;
    uint32_t t;
} D;

const char *const dream_vignette_names[DREAM_VIGNETTES] = {
    "Snack Rain", "The Giant Plush", "Falling Up", "Fear Parade", "Picnic", "Starry Hobby",
    "Cloud Concert", "Wobbly Teeth", "Hell University", "The Big Spoon", "Fluffy Wings",
    "Tea Under the Sea", "The Mirror", "Giant Doomogachi", "Endless Doors", "Demon Conga"
};

/* ====================================================================== */
/* the cast of a dream                                                      */
static int alive_id(int id)
{
    const DoomoResident *r = town_res(id);
    return r && r->status != RES_EMPTY && r->status != RES_DEAD;
}

void dream_cast(int id, DreamCast *c)
{
    const DoomoResident *r = town_res(id);
    uint16_t rel[20];
    int n, i;
    memset(c, 0, sizeof(*c));
    c->dreamer = (int16_t)id;
    c->friend_id = c->crush = c->rival = -1;
    if (!r) return;
    c->food = (uint8_t)(r->like_food % FOOD_COUNT);
    c->hate_food = (uint8_t)(r->hate_food % FOOD_COUNT);
    c->hobby = (uint8_t)(r->like_hobby % HOBBY_COUNT);
    c->fear = (uint8_t)(r->fear % FEAR_COUNT);
    c->color = (uint8_t)(r->like_color % PAL_PAINT_COUNT);
    c->music = (uint8_t)(r->like_music % MUSIC_COUNT);
    c->edgy = (uint8_t)(id < DOOMO_MAX_RESIDENTS && (RES_DISTRICT(id) || (r->flags & RF_HELLBORN)));
    c->possessed = (uint8_t)(r->status == RES_POSSESSED);
    n = rel_list(id, rel, DOOMO_COUNTOF(rel));
    for (i = 0; i < n; i++) {
        int o = rel[i], v = rel_value(id, o);
        if (o == id || !alive_id(o)) continue;
        if (c->crush < 0 && rel_has_crush(id, o)) c->crush = (int16_t)o;
        else if (c->friend_id < 0 && v > 15) c->friend_id = (int16_t)o;
        if (v < -15) c->rival = (int16_t)o;
    }
    if (c->crush < 0) {
        int p = sim_partner_of(id);
        if (p >= 0 && alive_id(p)) c->crush = (int16_t)p;
    }
}

int dream_plan(int id, uint32_t day, uint8_t *out, int force_nightmare)
{
    DoomoRng rng;
    const DoomoResident *r = town_res(id);
    int k, chance, w[DREAM_VIGNETTES], total = 0, i;
    DreamCast c;
    dream_cast(id, &c);
    rng_seed(&rng, doomo_hash32((uint32_t)id * 7741u + day * 2246822519u + 0xD7EAu));
    for (i = 0; i < DREAM_VIGNETTES; i++) w[i] = 10;
    if (c.crush >= 0 || c.friend_id >= 0) w[1] = 18;
    if (c.friend_id >= 0) w[4] = 14;
    if (c.rival >= 0) w[9] = 18;
    if (c.edgy) { w[15] += 8; w[8] += 6; w[2] += 4; }
    else { w[0] += 4; w[6] += 4; w[10] += 4; }
    for (k = 0; k < DREAM_SHOW; k++) {
        int pick, acc = 0;
        total = 0;
        for (i = 0; i < DREAM_VIGNETTES; i++) total += w[i];
        pick = rng_range(&rng, total);
        for (i = 0; i < DREAM_VIGNETTES; i++) {
            acc += w[i];
            if (pick < acc) break;
        }
        if (i >= DREAM_VIGNETTES) i = DREAM_VIGNETTES - 1;
        out[k] = (uint8_t)i;
        w[i] = 0;                                 /* no repeats */
    }
    if (force_nightmare || c.possessed) return 1;
    chance = 8;
    if (r) {
        chance += r->stat[STAT_CORRUPTION] / 8;
        if (r->stat[STAT_SANITY] < 40) chance += 10;
    }
    if (c.edgy) chance += 6;
    return rng_range(&rng, 100) < chance;
}

/* ====================================================================== */
/* captions                                                                 */
typedef struct { const char *cute, *twist; } DreamLine;
static const DreamLine dream_lines[DREAM_VIGNETTES] = {
    { "It's raining %f over HaHaland!", "...they're smiling. They have TEETH." },
    { "%C is a giant, squishy plush!", "The stitches are coming loose..." },
    { "%n is falling UP into a purple cloud.", "It smells like grape. It's breathing." },
    { "A parade of %x in tiny party hats!", "They're marching... toward %n." },
    { "A %c picnic with %F! So cozy.", "The ants are imps. They want the %f." },
    { "%n does %b among the stars.", "The stars are watching. Closely." },
    { "%n sings %m to a flock of sheep!", "The sheep have too many eyes." },
    { "%n's teeth fall out... they're %f!", "...and they keep growing back." },
    { "Surprise exam at Hell University!", "Question 1: what is your TRUE name?" },
    { "%R chases %n with a giant spoon!", "The spoon is getting closer. Hungry." },
    { "%n flies over HaHaland on fluffy wings!", "The wings are bat wings now. Uh oh." },
    { "An underwater tea party! Glub glub.", "The fish have teeth. And opinions." },
    { "%n waves at the mirror.", "The reflection waves first." },
    { "The Doomogachi is as big as a house!", "It's hungry. It's looking at %n." },
    { "Endless doors... each smells of %h.", "Something knocks. From the inside." },
    { "The Wall opens! Demons do the conga!", "%n is in the conga line. Forever." }
};

static char low_c(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

static int put(char *buf, int n, int cap, const char *s, int lower)
{
    while (*s && n < cap - 1) buf[n++] = lower ? low_c(*s++) : *s++;
    return n;
}

static int expand(const char *t, const DreamCast *c, char *buf, int n, int cap)
{
    while (*t && n < cap - 1) {
        if (*t != '%') { buf[n++] = *t++; continue; }
        t++;
        switch (*t) {
        case 'n': n = put(buf, n, cap, concert_name(c->dreamer), 0); break;
        case 'f': n = put(buf, n, cap, doomo_food_names[c->food], 1); break;
        case 'h': n = put(buf, n, cap, doomo_food_names[c->hate_food], 1); break;
        case 'x': n = put(buf, n, cap, doomo_fear_names[c->fear], 1); break;
        case 'b': n = put(buf, n, cap, doomo_hobby_names[c->hobby], 1); break;
        case 'm': n = put(buf, n, cap, doomo_music_names[c->music], 0); break;
        case 'c': n = put(buf, n, cap, doomo_paint_names[c->color], 1); break;
        case 'F': n = put(buf, n, cap, c->friend_id >= 0 ? concert_name(c->friend_id) : "a teddy bear", 0); break;
        case 'C':
            n = put(buf, n, cap, c->crush >= 0 ? concert_name(c->crush) :
                                 c->friend_id >= 0 ? concert_name(c->friend_id) : "Your Teddy", 0);
            break;
        case 'R':
            if (c->rival >= 0) n = put(buf, n, cap, concert_name(c->rival), 0);
            else { n = put(buf, n, cap, "A giant ", 0); n = put(buf, n, cap, doomo_food_names[c->hate_food], 1); }
            break;
        default: break;
        }
        if (*t) t++;
    }
    buf[n] = 0;
    return n;
}

int dream_caption(int v, const DreamCast *c, int nightmare, char *buf, int cap)
{
    int n, split;
    v = DOOMO_CLAMP(v, 0, DREAM_VIGNETTES - 1);
    n = expand(dream_lines[v].cute, c, buf, 0, cap);
    split = n;
    if (n < cap - 1) buf[n++] = ' ';
    if (nightmare && c->possessed) {
        n = put(buf, n, cap, sim_possessor_name(c->dreamer)[0] ? sim_possessor_name(c->dreamer) : "Something", 0);
        n = put(buf, n, cap, " whispers: \"MINE.\"", 0);
        buf[n] = 0;
    } else if (nightmare) {
        n = expand(dream_lines[v].twist, c, buf, n, cap);
        n = put(buf, n, cap, " AAAH!", 0);
        buf[n] = 0;
    } else {
        expand(dream_lines[v].twist, c, buf, n, cap);
    }
    return split;
}

/* ====================================================================== */
/* journal (raw unlock bits 944..959)                                       */
int dream_journal_has(int v)
{
    return v >= 0 && v < DREAM_VIGNETTES && unlock_has(UL_DREAM_JOURNAL + v);
}

int dream_journal_mark(int v)
{
    int id = UL_DREAM_JOURNAL + v;
    if (v < 0 || v >= DREAM_VIGNETTES || unlock_has(id)) return 0;
    doomo_town.unlocks[id >> 3] |= (uint8_t)(1u << (id & 7));   /* no toast: not an unlockable */
    return 1;
}

int dream_journal_count(void)
{
    int v, n = 0;
    for (v = 0; v < DREAM_VIGNETTES; v++) n += dream_journal_has(v);
    return n;
}

/* --- S2: the scripted dreams (rows: DREAM_SCRIPTED_*; vignette ids as
 * dream_vignette_names; frames in one table) --- */
static const struct { uint8_t vig[DREAM_SHOW], nightmare; } scripted_rows[DREAM_SCRIPTED_COUNT] = {
    { { 4, 7, 14 }, 0 },     /* D1 "Teeth Like Muffins": the Mingle, muffin teeth, the door */
    { { 2, 6, 12 }, 0 },     /* D2 "The Ninth Cloud" (S3): falling up, the cloud, the mirror */
};
static const struct { uint8_t peek_hold, letter_gap, letter_rise; } SD = { 75, 2, 10 };
/* --- end S2 --- */

/* ====================================================================== */
/* scene                                                                    */
static void goto_state(int s) { D.state = s; D.st_t = 0; }

static void leave(void)
{
    if (scene_depth() > 1) scene_pop();
    else scene_set(&SCENE_TITLE, 0);
}

static int creep_of(int t, int nightmare_vig)
{
    int start = nightmare_vig ? 60 : 82, c = t < start ? 0 : (t - start) / (nightmare_vig ? 3 : 4);
    return c > 16 ? 16 : c;
}

static void start_vignette(int i)
{
    D.vi = i;
    D.split = dream_caption(D.plan[i], &D.cast, D.nightmare && i == DREAM_SHOW - 1, D.cap, (int)sizeof(D.cap));
    if (D.scripted) {                               /* S2: the script's line, if it stashed one */
        const char *l = txt_stash_get(i);
        if (l && l[0]) {
            doomo_snprintf(D.cap, sizeof(D.cap), "%s", l);
            D.split = (int)strlen(D.cap);
        }
        if (D.nightmare && i == DREAM_SHOW - 1) audio_music(SONG_OMINOUS);
        goto_state(DS_DREAM);
        return;
    }
    if (dream_journal_mark(D.plan[i])) {
        int n = dream_journal_count();
        if (n >= 8) unlock_grant(UL_PART_SLEEPY_EYES);
        if (n >= DREAM_VIGNETTES) unlock_grant(UL_PART_WAVY_HAIR);
    }
    if (D.nightmare && i == DREAM_SHOW - 1) audio_music(SONG_OMINOUS);
    goto_state(DS_DREAM);
}

static void dream_enter(int arg)
{
    const DoomoResident *r;
    int id, force = 0, p;
    memset(&D, 0, sizeof(D));
    D.prev_song = audio_music_current();
    if (arg >= DREAM_ARG_SCRIPTED(0) && arg < DREAM_ARG_SCRIPTED(DREAM_SCRIPTED_COUNT)) {   /* S2 */
        int k = arg - DREAM_ARG_SCRIPTED(0), i;
        D.scripted = 1 + k;
        D.id = DOOMO_PLAYER_ID;
        dream_cast(D.id, &D.cast);
        for (i = 0; i < DREAM_SHOW; i++) D.plan[i] = scripted_rows[k].vig[i];
        D.nightmare = scripted_rows[k].nightmare;
        audio_music(SONG_HAHALAND_NIGHT);
        goto_state(DS_PEEK);
        return;
    }
    if (arg <= DREAM_ARG_NIGHTMARE) { force = 1; id = concert_demo_resident(DREAM_ARG_NIGHTMARE - arg); }
    else if (arg < 0) id = concert_demo_resident(-1 - arg);
    else id = arg;
    D.id = id;
    r = town_res(id);
    if (!r || r->status == RES_EMPTY || r->status == RES_DEAD) {
        ui_toast("Nobody is sleeping here.");
        D.id = -1;
        goto_state(DS_AFTER);
        D.outcome = 3;
        return;
    }
    dream_cast(id, &D.cast);
    p = sim_problem_for(id);
    D.problem = (p >= 0 && sim_problem(p)->kind == PROB_NIGHTMARE) ? 1 : 0;
    D.nightmare = dream_plan(id, doomo_town.day, D.plan, force || D.problem);
    D.was_awake = r->activity != ACT_SLEEPING;
    audio_music(SONG_HAHALAND_NIGHT);
    goto_state(DS_PEEK);
}

static void dream_leave(void)
{
    if (D.prev_song > SONG_NONE) audio_music(D.prev_song);
    else concert_restore_music();
}

static void finish(int outcome)
{
    DoomoResident *r = D.scripted ? NULL : town_res(D.id);   /* S2: the plot's dream changes nothing */
    D.outcome = outcome;
    if (r) {
        if (outcome == 0) {                      /* sweet dreams */
            r->stat[STAT_HAPPY] = (uint8_t)DOOMO_MIN(100, r->stat[STAT_HAPPY] + 3);
            r->stat[STAT_SANITY] = (uint8_t)DOOMO_MIN(100, r->stat[STAT_SANITY] + 3);
        } else if (outcome == 1) {               /* woken from a nightmare */
            r->stat[STAT_ENERGY] = (uint8_t)DOOMO_MAX(0, r->stat[STAT_ENERGY] - 10);
            r->stat[STAT_SANITY] = (uint8_t)DOOMO_MIN(100, r->stat[STAT_SANITY] + 5);
            r->stat[STAT_SOCIAL] = (uint8_t)DOOMO_MIN(100, r->stat[STAT_SOCIAL] + 5);
            if (r->level < 99) r->level++;
            if (D.problem) {
                int p = sim_problem_for(D.id);
                if (p >= 0 && sim_problem(p)->kind == PROB_NIGHTMARE) sim_problem_resolve(p);
            }
            unlock_grant(UL_PART_MESSY_HAIR);
        } else {                                 /* left in the nightmare */
            r->stat[STAT_SANITY] = (uint8_t)DOOMO_MAX(0, r->stat[STAT_SANITY] - 5);
            sim_corrupt(D.id, 6);
        }
    }
    audio_music(SONG_HAHALAND_NIGHT);
    goto_state(DS_AFTER);
}

static void dream_update(void)
{
    D.t++;
    D.st_t++;
    switch (D.state) {
    case DS_PEEK:
        if (D.scripted) {                         /* S2: a forced Peek */
            if ((btn_pressed(BTN_A) && D.st_t > 8) || D.st_t >= SD.peek_hold) {
                audio_sfx(SFX_BUBBLE);
                goto_state(DS_ENTER);
            }
            break;
        }
        if (btn_pressed(BTN_B)) { audio_sfx(SFX_CANCEL); leave(); return; }
        if (btn_pressed(BTN_A) && D.st_t > 8) {
            audio_sfx(SFX_BUBBLE);
            unlock_grant(UL_PART_DROOPY_EYES);
            unlock_grant(UL_STAGE_DREAMLAND);
            goto_state(DS_ENTER);
        }
        break;
    case DS_ENTER:
        if (D.st_t >= ENTER_TICKS) start_vignette(0);
        break;
    case DS_DREAM: {
        int last = D.vi == DREAM_SHOW - 1;
        if (D.st_t == 1) audio_sfx(SFX_SPARKLE);
        if (D.nightmare && last && D.st_t == 100) { audio_sfx(SFX_DEMON_ROAR); unlock_grant(UL_PART_WAVY_MOUTH); }
        if (btn_pressed(BTN_B) && !D.scripted) { audio_sfx(SFX_CANCEL); finish(0); return; }
        if ((btn_pressed(BTN_A) && D.st_t > 30) || D.st_t >= VIG_TICKS) {
            if (!last) goto_state(DS_WIPE);
            else if (D.nightmare) {
                D.choice = 0;
                unlock_grant(UL_PART_WAVY_MOUTH);
                goto_state(DS_CHOICE);
            }
            else finish(0);
        }
        break; }
    case DS_WIPE:
        if (D.st_t >= WIPE_TICKS) start_vignette(D.vi + 1);
        break;
    case DS_CHOICE:
        if (btn_pressed(BTN_LEFT) || btn_pressed(BTN_RIGHT) || btn_pressed(BTN_UP) || btn_pressed(BTN_DOWN)) {
            D.choice ^= 1;
            audio_sfx(SFX_CURSOR);
        }
        if (btn_pressed(BTN_A) && D.st_t > 12) {
            audio_sfx(D.choice == 0 ? SFX_HAPPY : SFX_POSSESS);
            finish(D.choice == 0 ? 1 : 2);
        }
        break;
    case DS_AFTER:
        if ((btn_pressed(BTN_A) || btn_pressed(BTN_B)) && D.st_t > 15) leave();
        break;
    }
}

/* ====================================================================== */
/* drawing                                                                  */
static void caption_box(const char *text, int split, int t, int creep, int nightmare, int above_hint)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), big = sh >= 200;
    int w = sw - (big ? 40 : 16), x = (sw - w) / 2, lines, h, y;
    int len = (int)strlen(text), shown;
    char buf[112];
    uint8_t bg = creep >= 12 ? (nightmare ? PAL_RAMP(RAMP_BLOOD, 0) : PAL_RAMP(RAMP_VAPE, 0)) : PAL_RAMP(RAMP_VAPE, 7);
    uint8_t ink = creep >= 12 ? PAL_WHITE : PAL_RAMP(RAMP_VAPE, 0);
    uint8_t ol = creep >= 12 ? (nightmare ? PAL_RAMP(RAMP_BLOOD, 4) : PAL_RAMP(RAMP_VAPE, 3)) : PAL_RAMP(RAMP_VAPE, 4);
    int k;
    /* the box fits the whole line (it grows upwards) */
    lines = ui_text_wrap(x + 8, 0, w - 16, text, ink, 0);
    if (lines < 1) lines = 1;
    h = lines * FONT_LINE + 9;
    y = sh - h - (above_hint ? 17 : (big ? 10 : 5));
    /* the cute half types first, the twist only once the dream turns */
    shown = t < 8 ? 0 : (t - 8) * 3 / 2;
    if (shown > split) shown = split;
    if (creep > 0) shown = split + 1 + (creep * (len - split)) / 10;
    if (shown > len) shown = len;
    memcpy(buf, text, (size_t)shown);
    buf[shown] = 0;
    /* a cloud-shaped box */
    for (k = 0; k < w; k += 14) ui_fill_circle(x + 7 + k, y + 1, 7, ol);
    ui_round_rect(x - 1, y - 1, w + 2, h + 2, 8, ol);
    for (k = 0; k < w; k += 14) ui_fill_circle(x + 7 + k, y + 2, 6, bg);
    ui_round_rect(x, y, w, h, 7, bg);
    ui_text_wrap(x + 8, y + 5, w - 16, buf, ink, 1);
    if (shown >= len && ((t / 10) & 1)) plat_text(x + w - 12, y + h - 10, STR_ICON_ARROW_D, ink);
}

/* S2: floating letters - each glyph rises into place SD.letter_gap frames
 * after the one before it, then bobs; wrapped like ui_text_wrap */
static void floating_caption(const char *text, int t, int creep, int nightmare)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), big = sh >= 200;
    int w = sw - (big ? 40 : 16), x0 = (sw - w) / 2 + 8, lines, y, k = 0, line = 0, off = 0;
    uint8_t ink = creep >= 12 ? (nightmare ? PAL_RAMP(RAMP_BLOOD, 5) : PAL_WHITE) : PAL_RAMP(RAMP_VAPE, 7);
    uint8_t ol = PAL_RAMP(RAMP_VAPE, 0);
    lines = ui_text_wrap(x0, 0, w - 16, text, ink, 0);
    if (lines < 1) lines = 1;
    y = sh - lines * FONT_LINE - (big ? 30 : 20);
    while (text[off] && line < lines) {
        int n = ui_wrap_skip(text + off, w - 16, 1), i, cx = x0;
        if (n <= 0) break;
        for (i = 0; i < n; i++, k++) {
            char g[2];
            int age = t - k * SD.letter_gap, dy;
            if (age < 0) break;
            dy = age < SD.letter_rise ? (SD.letter_rise - age) * (SD.letter_rise - age) / 4 : 0;
            dy += ui_wave(t + k * 7, 64, 1);
            g[0] = text[off + i];
            g[1] = 0;
            if (g[0] != ' ' && g[0] != '\n') ui_text_outline(cx, y + line * FONT_LINE + dy, g, ink, ol);
            cx += FONT_ADVANCE;
        }
        off += n;
        line++;
    }
}

static void bedroom(int expr, int shake)
{
    dream_draw_bedroom(D.id, &D.cast, expr, D.t, shake);
}

static void dream_draw(void)
{
    int sw = plat_screen_w(), sh = plat_screen_h(), big = sh >= 200;
    char buf[96];
    switch (D.state) {
    case DS_PEEK:
        bedroom(EXPR_SLEEPY, 0);
        if (D.scripted) {                         /* S2: no choice: you are already falling asleep */
            floating_caption("You drift off... Zzz...", D.st_t, 0, 0);
            break;
        }
        doomo_snprintf(buf, sizeof(buf), D.was_awake ? "%s yawns and drifts off to sleep..."
                                                     : "%s is fast asleep. Zzz...", concert_name(D.id));
        caption_box(buf, (int)strlen(buf), 200, 0, 0, 1);
        ui_hint(STR_ICON_A " Peek into the dream  " STR_ICON_B " Tiptoe away", WIN_DARK);
        break;
    case DS_ENTER:
        bedroom(EXPR_SLEEPY, 0);
        dream_draw_bubble_in(D.id, D.st_t, ENTER_TICKS, D.t);
        break;
    case DS_DREAM: case DS_WIPE: case DS_CHOICE: {
        int last = D.vi == DREAM_SHOW - 1, nm = D.nightmare && last;
        int t = D.state == DS_DREAM ? D.st_t : VIG_TICKS, creep = creep_of(t, nm);
        if (D.state == DS_CHOICE) creep = 16;
        dream_draw_sky(creep, nm, D.t);
        dream_draw_vignette(D.plan[D.vi], &D.cast, t, creep, nm);
        dream_draw_frame(creep, nm, D.t);
        if (D.state == DS_WIPE) {
            /* a swirl of sparkles closes over the dream */
            int r = (sw + sh) * D.st_t / (WIPE_TICKS * 2), k;
            plat_shade_rect(0, 0, sw, sh, 15 - D.st_t * 15 / WIPE_TICKS);
            for (k = 0; k < 8; k++)
                ui_sparkle(sw / 2 + (int)((fx_cos(k * 512 + D.st_t * 90) * r) >> 16),
                           sh / 2 + (int)((fx_sin(k * 512 + D.st_t * 90) * r) >> 17), (D.st_t + k) & 15,
                           PAL_PAINT(PAINT_LEMON, 2));
        } else if (D.state == DS_DREAM) {
            if (D.scripted) floating_caption(D.cap, t, creep, nm);    /* S2 */
            else caption_box(D.cap, D.split, t, creep, nm, 0);
        }
        /* dream journal badge (not in the plot's own dreams) */
        if (!D.scripted) {
            int n = dream_journal_count();
            doomo_snprintf(buf, sizeof(buf), STR_ICON_ZZZ " %d/%d", n, DREAM_VIGNETTES);
            ui_text_outline(sw - ui_text_width(buf) - 6, big ? 8 : 5, buf, PAL_WHITE, PAL_RAMP(RAMP_VAPE, 0));
            ui_text_outline(6, big ? 8 : 5, dream_vignette_names[D.plan[D.vi]], PAL_WHITE, PAL_RAMP(RAMP_VAPE, 0));
        }
        if (D.state == DS_CHOICE) {
            int y = sh / 2 - (big ? 30 : 24), bw = big ? 104 : 88, gap = big ? 12 : 8;
            int tw = ui_text_big_width("NIGHTMARE!", 2);
            plat_shade_rect(0, 0, sw, sh, 9);
            ui_text_big(sw / 2 - tw / 2 + ((D.t & 2) ? 1 : -1), y, "NIGHTMARE!", 2, PAL_RAMP(RAMP_BLOOD, 5),
                        PAL_RAMP(RAMP_BLOOD, 0));
            doomo_snprintf(buf, sizeof(buf), "%s is having a nightmare!", concert_name(D.id));
            ui_text_center(sw / 2, y + 22, buf, PAL_WHITE);
            ui_button(sw / 2 - bw - gap / 2, y + 38, bw, "Wake them up", D.choice == 0, WIN_CUTE);
            ui_button(sw / 2 + gap / 2, y + 38, bw, "Let them sleep", D.choice == 1, WIN_HELL);
            ui_text_center(sw / 2, y + 60, D.choice == 0 ? "A hug and a glass of milk." :
                           "They'll sleep it off... probably.", PAL_GRAY_L);
        }
        break; }
    case DS_AFTER: {
        static const char *const lines[4] = {
            "%s smiles in their sleep. Sweet dreams!",
            "%s wakes up with a gasp! You give them a big hug.",
            "%s tosses and turns... the nightmare sinks in. (Corruption +6)",
            "The bed is empty..."
        };
        int expr = D.outcome == 1 ? EXPR_SHOCK : D.outcome == 2 ? EXPR_SICK : EXPR_SLEEPY;
        if (D.id >= 0) bedroom(expr, D.outcome == 2 && D.st_t < 40);
        else ui_backdrop(WIN_DARK, D.t);
        if (D.scripted) {                         /* S2: the wake-up line (stash 3) */
            const char *l = txt_stash_get(3);
            floating_caption(l && l[0] ? l : "You wake up. The room is very quiet.", D.st_t, 0, 0);
            ui_hint(STR_ICON_A " Wake up", WIN_DARK);
            break;
        }
        doomo_snprintf(buf, sizeof(buf), lines[DOOMO_CLAMP(D.outcome, 0, 3)], concert_name(D.id));
        caption_box(buf, (int)strlen(buf), 200, 0, 0, 1);
        if (D.outcome == 1) {
            int k;
            for (k = 0; k < 5; k++)
                ui_heart(sw / 2 + ui_wave((int)D.t + k * 30, 120, sw / 4), sh / 2 - 10 - (int)((D.t + (uint32_t)k * 13) % 40),
                         3, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        }
        ui_hint(STR_ICON_A " Done", WIN_DARK);
        break; }
    }
}

const DoomoScene SCENE_DREAM = { "DREAM", dream_enter, dream_leave, dream_update, dream_draw, 0 };
