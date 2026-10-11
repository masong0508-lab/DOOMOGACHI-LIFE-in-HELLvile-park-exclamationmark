/* exorcism_scene.c - SCENE_EXORCISM: Father Fuzz's ritual at the Church of
 * St. Cuddles (owner: life). arg = resident id (see exorcism.h).
 *
 *   intro    pastel church, Father Fuzz explains, the host's portrait
 *            twitches on the altar
 *   tools    pick holy water / sage / plush cross / rosary / salt / hymn
 *            book from the inventory, with a live odds preview
 *   phase 1  PRAYER RHYTHM - button prompts slide along a ribbon into a
 *            heart-shaped hit zone, on the beat (EXORCISM_BPM)
 *   phase 2  HOLY WATER - aim a heart reticle and lob flasks at the vape
 *            demon darting around the host's (shaking) portrait
 *   phase 3  TRUE NAME - pick the possessor (a real Hellvile citizen) from
 *            clues about their job, personality, cravings and bio
 *   verdict  the holy power meter fills ... success or failure
 *   result   confetti and hearts, or a snuffed candle; at the third
 *            failure "the exorcisms have failed... their soul is lost",
 *            and only then Father Fuzz offers to Lay them to Rest (with
 *            confirmation).
 *
 * The church rots from sugar pastel to blood red ("dread") as the ritual
 * goes on, and the demon taunts in speech bubbles and dialogue.
 */
#include "exorcism_int.h"
#include "sim_int.h"

enum { ST_INTRO, ST_TOOLS, ST_PRAYER, ST_WATER, ST_NAME, ST_VERDICT, ST_RESULT,
       ST_LOST, ST_FAREWELL, ST_NODEMON, ST_WAIT };

#define NOTE_MAX 20
#define PRAYER_NOTES 16
#define TRAVEL 30                       /* ticks a prompt takes to arrive */
#define WATER_TICKS 600

typedef struct { int16_t tick; uint8_t btn; uint8_t judged; } Note;
typedef struct { int16_t t, x, y, active; } Throw;

static const uint32_t note_btn[6] = { BTN_A, BTN_B, BTN_LEFT, BTN_RIGHT, BTN_UP, BTN_DOWN };
static const char *const note_glyph[6] = { "A", "B", STR_ICON_ARROW_L,
                                           STR_ICON_ARROW_R, STR_ICON_ARROW_U, STR_ICON_ARROW_D };

static struct {
    int host, practice, state, st_t, next_state;
    unsigned items;
    int sel, odds, dread, dread_target, shake;
    /* prayer */
    Note notes[NOTE_MAX];
    int nnotes, pt, score, combo, best_combo, perfects, goods, misses, fb, fb_t, rhythm;
    /* holy water */
    int cx, cy, dx, dy, tx, ty, retarget, flasks, hits, need, demon_flash, water, water_done;
    Throw throws[2];
    int splash_x, splash_y, splash_t, splash_hit;
    /* true name */
    int choices[4], correct, clues, name_ok, picked;
    /* verdict */
    int chance, success, fails_before;
    char taunt[72];
    int taunt_t;
    DoomoLook dummy;
    int confetti_t;
} X;

/* ---------------------------------------------------------------------- */
/* helpers                                                                  */
static int W(void) { return plat_screen_w(); }
static int H(void) { return plat_screen_h(); }
static int BIG(void) { return plat_screen_h() >= 200; }
static int floor_y(void) { return H() * 62 / 100; }

static int fuzz_id(void)
{
    const DoomoResident *f = town_res(ROSTER_ID_FATHER_FUZZ);
    if (f && f->status == RES_ALIVE &&
        strncmp(f->name, roster_name(ROSTER_ID_FATHER_FUZZ), DOOMO_NAME_LEN) == 0)
        return ROSTER_ID_FATHER_FUZZ;
    return -1;
}

static void fuzz_say(const char *fmt, const char *a, const char *b)
{
    char line[200], buf[200];
    doomo_snprintf(line, sizeof(line), fmt, a ? a : "", b ? b : "");
    if (fuzz_id() >= 0) ui_say(fuzz_id(), line);
    else {
        doomo_snprintf(buf, sizeof(buf), "Father Fuzz: %s", line);
        ui_say(-1, buf);
    }
}

static const char *host_name(void)
{
    if (X.practice) return "Practice Plush";
    return town_res(X.host) ? town_res(X.host)->name : "???";
}

static int host_ref(void) { return X.practice ? -1 - X.host : X.host; }

static const char *demon_name(void)
{
    static char buf[DOOMO_NAME_LEN + 2];
    int ch[4], c = exorcism_name_choices(host_ref(), ch);
    const DoomoResident *r = town_res(ch[c]);
    const char *n = (r && r->status != RES_EMPTY) ? r->name : roster_name(ch[c]);
    doomo_snprintf(buf, sizeof(buf), "%s", n && n[0] ? n : "Nameless One");
    return buf;
}

static const DoomoImage *host_image(int expr)
{
    if (X.practice) return figure_image(&X.dummy, X.state == ST_RESULT && X.success ? RES_ALIVE
                                                                                     : RES_POSSESSED, expr);
    return portrait_image(X.host, expr);
}

static void taunt(const char *fmt, const char *a)
{
    doomo_snprintf(X.taunt, sizeof(X.taunt), fmt, a ? a : "");
    X.taunt_t = 70;
}

static void random_taunt(void)
{
    static const char *const t[] = {
        "This body is MINE now. So squishy!",
        "Your prayers tickle, little lamb.",
        "Hehehe! %s can't hear you!",
        "Cozy in here. Smells like cupcakes.",
        "Try harder, Fuzzball!",
        "I'll never leave! NEVER!",
        "%s's dreams taste like candy floss."
    };
    taunt(t[rng_range(&doomo_rng, DOOMO_COUNTOF(t))], host_name());
}

static void goto_state(int s)
{
    X.state = s;
    X.st_t = 0;
    X.sel = 0;
}

static void leave_scene(void)
{
    if (scene_depth() > 1) scene_pop();
    else scene_set(&SCENE_TITLE, 0);
}

/* dialogue callbacks */
static void cb_to_tools(void) { goto_state(ST_TOOLS); }
static void cb_exit(void) { leave_scene(); }

/* ---------------------------------------------------------------------- */
/* setup                                                                    */
static void make_dummy(void)
{
    DoomoResident r;
    resident_default(&r);
    r.look.species = SPECIES_PLUSHIE;
    r.look.skin = 8 + PAINT_CARAMEL;
    r.look.eye_style = EYE_BUTTON;
    r.look.eye_color = PAINT_JET;
    r.look.mouth_style = MOUTH_STITCHED;
    r.look.ears = EARS_BEAR;
    r.look.marking = MARK_STITCHES;
    r.look.marking_color = PAINT_ROSE;
    r.look.hair_style = HAIR_BALD;
    r.look.top = TOP_NONE;
    r.look.accessory = ACC_BOWTIE;
    r.look.accessory_color = PAINT_BUBBLEGUM;
    r.look.cheeks = CHEEK_BLUSH;
    X.dummy = r.look;
}

static void build_notes(void)
{
    DoomoRng rng;
    int i, beat2 = 8, last = -1, rep = 0;        /* half-beats; 4-beat count-in */
    const DoomoResident *r = town_res(X.host);
    rng_seed(&rng, 0xBEA7u ^ (uint32_t)X.host * 7919u ^
                   (r && !X.practice ? (uint32_t)r->exorcism * 131u : 77u));
    X.nnotes = PRAYER_NOTES;
    for (i = 0; i < X.nnotes; i++) {
        int b = rng_range(&rng, 6);
        if (b == last && ++rep >= 2) { b = (b + 1 + rng_range(&rng, 5)) % 6; rep = 0; }
        if (b != last) rep = 0;
        last = b;
        X.notes[i].tick = (int16_t)(beat2 * EXORCISM_TICKS_PER_BEAT / 2);
        X.notes[i].btn = (uint8_t)b;
        X.notes[i].judged = 0;
        beat2 += (i % 4 == 3) ? 3 : (rng_range(&rng, 4) == 0 && (beat2 & 1) == 0) ? 1 : 2;
    }
}

static void exo_enter(int arg)
{
    const DoomoResident *r;
    memset(&X, 0, sizeof(X));
    X.host = arg;
    r = town_res(arg);
    make_dummy();
    sim_ensure_init();
    if (arg < 0 || arg >= DOOMO_MAX_RESIDENTS || !r || r->status == RES_EMPTY) {
        X.practice = 1;
        X.host = arg < 0 ? -1 - arg : arg;        /* host_ref() gives a stable id */
        if (X.host < 0 || X.host >= DOOMO_MAX_RESIDENTS) X.host = 0;
    }
    X.cx = W() / 2;
    X.cy = H() / 2;
    audio_music(SONG_CHURCH);
    ui_dialog_style(WIN_CUTE);
    if (!X.practice && r->status == RES_SOUL_LOST) {
        X.dread = X.dread_target = 15;
        goto_state(ST_LOST);
        return;
    }
    if (!X.practice && r->status != RES_POSSESSED) {
        goto_state(ST_NODEMON);
        if (r->status == RES_DEAD)
            fuzz_say("%s rests in the graveyard now. Let's leave flowers instead.", host_name(), NULL);
        else
            fuzz_say("%s is perfectly fine! No demons here, just good vibes.", host_name(), NULL);
        ui_say_done(cb_exit);
        return;
    }
    goto_state(ST_INTRO);
    X.dread = X.dread_target = 2;
    if (X.practice) {
        fuzz_say("Welcome to St. Cuddles! Let's practise the ritual on this old plush.", NULL, NULL);
        fuzz_say("Don't worry, the demon in it is only a teeny one. Probably.", NULL, NULL);
    } else {
        fuzz_say("Oh my stars... %s has a demon inside! Quick, to the altar!", host_name(), NULL);
        if (exorcism_fails(X.host) > 0)
            fuzz_say("It's dug in deeper since last time. %s more failure%s and their soul is lost.",
                     exorcism_tries_left(X.host) == 1 ? "One" : "Two",
                     exorcism_tries_left(X.host) == 1 ? "" : "s");
    }
    fuzz_say("We need faith, holy water and the demon's TRUE NAME. Pick your tools!", NULL, NULL);
    ui_say_done(cb_to_tools);
}

static void exo_leave(void)
{
    ui_dialog_style(WIN_CUTE);
}

/* ---------------------------------------------------------------------- */
/* phase transitions                                                        */
static void begin_prayer(void)
{
    goto_state(ST_PRAYER);
    build_notes();
    X.pt = -1;
    X.dread_target = 5;
    audio_music(SONG_EXORCISM);
}
static void cb_prayer(void) { begin_prayer(); }

static void begin_water(void)
{
    int i;
    goto_state(ST_WATER);
    X.flasks = 6 + ((X.items & (1u << EXI_HOLY_WATER)) ? 3 : 0);
    X.need = 4 - ((X.items & (1u << EXI_PLUSH_CROSS)) ? 1 : 0);
    X.hits = 0;
    X.water_done = 0;
    X.cx = W() / 2;
    X.cy = H() / 2;
    X.dx = (W() / 2) << 8;
    X.dy = (H() / 2 - 20) << 8;
    X.tx = W() / 2;
    X.ty = H() / 2;
    for (i = 0; i < 2; i++) X.throws[i].active = 0;
    X.dread_target = 8;
}
static void cb_water(void) { begin_water(); }

static void begin_name(void)
{
    goto_state(ST_NAME);
    X.correct = exorcism_name_choices(host_ref(), X.choices);
    X.clues = 1 + (X.rhythm >= 50) + (X.rhythm >= 85) + ((X.items & (1u << EXI_HYMN_BOOK)) != 0);
    if (X.clues > 4) X.clues = 4;
    X.picked = -1;
    X.dread_target = 11;
}
static void cb_name(void) { begin_name(); }

static void begin_verdict(void)
{
    goto_state(ST_VERDICT);
    X.chance = exorcism_chance(X.odds, X.rhythm, X.water, X.name_ok);
}

static void start_ritual(void)
{
    int i;
    if (!X.practice)
        for (i = 0; i < EXI_COUNT; i++)
            if ((X.items & (1u << i)) && !opt_get(OPT_CHEAT_INFINITE_CANDY))
                inv_take(doomo_exorcism_items[i], 1);
    X.odds = X.practice ? exorcism_odds_with(-1, X.items) : exorcism_odds_with(X.host, X.items);
    goto_state(ST_WAIT);
    audio_sfx(SFX_HOLY);
    fuzz_say("Phase one: pray with me! Press each button as it reaches the heart. "
             "Keep to the beat!", NULL, NULL);
    ui_say_done(cb_prayer);
}

/* ---------------------------------------------------------------------- */
/* update                                                                   */
static void update_tools(void)
{
    int rows = EXI_COUNT + 1;
    if (btn_repeat(BTN_UP)) { X.sel = (X.sel + rows - 1) % rows; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_DOWN)) { X.sel = (X.sel + 1) % rows; audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_START)) { start_ritual(); return; }
    if (btn_pressed(BTN_B)) {
        audio_sfx(SFX_CANCEL);
        goto_state(ST_WAIT);
        fuzz_say("Come back when you're ready. Hurry though - demons get comfy fast.", NULL, NULL);
        ui_say_done(cb_exit);
        return;
    }
    if (btn_pressed(BTN_A)) {
        if (X.sel == EXI_COUNT) { start_ritual(); return; }
        if (X.items & (1u << X.sel)) { X.items &= ~(1u << X.sel); audio_sfx(SFX_CANCEL); }
        else if (X.practice || opt_get(OPT_CHEAT_INFINITE_CANDY) ||
                 inv_count(doomo_exorcism_items[X.sel]) > 0) {
            X.items |= 1u << X.sel;
            audio_sfx(SFX_SELECT);
        } else {
            audio_sfx(SFX_ERROR);
            ui_toast("You don't have any! (Try the shop)");
        }
    }
    X.odds = X.practice ? exorcism_odds_with(-1, X.items) : exorcism_odds_with(X.host, X.items);
}

static void judge(int i, int kind)
{
    X.notes[i].judged = (uint8_t)kind;
    X.fb = kind;
    X.fb_t = 0;
    if (kind == 1) { X.score += 100; X.perfects++; X.combo++; audio_sfx(SFX_SPARKLE); }
    else if (kind == 2) { X.score += 60; X.goods++; X.combo++; audio_sfx(SFX_BLIP); }
    else {
        X.misses++;
        X.combo = 0;
        X.shake = 6;
        audio_sfx(SFX_DEMON_ROAR);
        if (rng_range(&doomo_rng, 3) == 0) taunt("MISSED! Hahaha!", NULL);
    }
    if (X.combo > X.best_combo) X.best_combo = X.combo;
}

static void update_prayer(void)
{
    int i, perfect = (X.items & (1u << EXI_SAGE)) ? 4 : 3, good = (X.items & (1u << EXI_SAGE)) ? 8 : 6;
    int last_tick = X.notes[X.nnotes - 1].tick;
    X.pt++;
    if (X.pt >= 0 && X.pt % EXORCISM_TICKS_PER_BEAT == 0 && X.pt <= last_tick &&
        (X.pt / EXORCISM_TICKS_PER_BEAT) % 4 == 0)
        audio_sfx(SFX_BELL);
    if (X.pt == 2 * EXORCISM_TICKS_PER_BEAT * 3) random_taunt();
    /* button presses: the earliest open note in the window */
    for (i = 0; i < 6; i++) {
        int n, best = -1, bd = 999;
        if (!btn_pressed(note_btn[i])) continue;
        for (n = 0; n < X.nnotes; n++) {
            int d = X.pt - X.notes[n].tick;
            if (X.notes[n].judged) continue;
            if (d < -good || d > good) continue;
            if ((d < 0 ? -d : d) < bd) { bd = d < 0 ? -d : d; best = n; }
        }
        if (best < 0) continue;                     /* a stray press is free */
        if (X.notes[best].btn != i) judge(best, 3);
        else judge(best, bd <= perfect ? 1 : 2);
    }
    for (i = 0; i < X.nnotes; i++)
        if (!X.notes[i].judged && X.pt - X.notes[i].tick > good) judge(i, 3);
    if (X.pt > last_tick + EXORCISM_TICKS_PER_BEAT * 2) {
        X.rhythm = X.score / (X.nnotes > 0 ? X.nnotes : 1);
        goto_state(ST_WAIT);
        if (X.rhythm >= 85) fuzz_say("Beautiful! Your faith is blinding!", NULL, NULL);
        else if (X.rhythm >= 50) fuzz_say("Good, good! The demon is wobbling!", NULL, NULL);
        else fuzz_say("Oh dear, a bit off-key... but it's rattled!", NULL, NULL);
        fuzz_say("Phase two: holy water! Aim with the D-pad, throw with " STR_ICON_A "!", NULL, NULL);
        ui_say_done(cb_water);
    }
}

static void water_geom(int *hx, int *hy, int *hw, int *hh)
{
    *hx = W() / 2;
    *hy = H() / 2 + 6;
    *hw = PORTRAIT_W * (BIG() ? 2 : 1) + 10;
    *hh = PORTRAIT_H * (BIG() ? 2 : 1) + 10;
}

static void end_water(void)
{
    X.water = X.hits >= X.need ? 100 : X.hits * 100 / (X.need > 0 ? X.need : 1);
    X.water_done = 1;
    goto_state(ST_WAIT);
    if (X.hits >= X.need) fuzz_say("It's soaked and shivering! Now for the true name!", NULL, NULL);
    else fuzz_say("Drat, it's still slippery... no matter. Its NAME, quickly!", NULL, NULL);
    ui_say_done(cb_name);
}

static void update_water(void)
{
    int i, hx, hy, hw, hh, spd = BIG() ? 4 : 3, slow = (X.items & (1u << EXI_SALT)) != 0;
    water_geom(&hx, &hy, &hw, &hh);
    if (X.water_done) return;
    if (btn_down(BTN_LEFT)) X.cx -= spd;
    if (btn_down(BTN_RIGHT)) X.cx += spd;
    if (btn_down(BTN_UP)) X.cy -= spd;
    if (btn_down(BTN_DOWN)) X.cy += spd;
    X.cx = DOOMO_CLAMP(X.cx, 8, W() - 8);
    X.cy = DOOMO_CLAMP(X.cy, 16, H() - 12);
    /* the demon darts around the host */
    if (--X.retarget <= 0 || (((X.dx >> 8) - X.tx) * ((X.dx >> 8) - X.tx) +
                              ((X.dy >> 8) - X.ty) * ((X.dy >> 8) - X.ty)) < 16) {
        X.tx = hx - hw / 2 - 30 + rng_range(&doomo_rng, hw + 60);
        X.ty = hy - hh / 2 - 6 + rng_range(&doomo_rng, hh + 12);
        X.retarget = (slow ? 50 : 30) + rng_range(&doomo_rng, 30);
    }
    {
        int div = slow ? 16 : 10;
        if (X.st_t % 90 > 70) div = slow ? 9 : 5;                /* a sudden dash */
        X.dx += (((X.tx << 8) - X.dx) / div);
        X.dy += (((X.ty << 8) - X.dy) / div);
    }
    if (X.demon_flash > 0) X.demon_flash--;
    if (X.st_t % 200 == 100) random_taunt();
    /* throwing */
    if (btn_pressed(BTN_A) && X.flasks > 0) {
        for (i = 0; i < 2; i++)
            if (!X.throws[i].active) {
                X.throws[i].active = 1;
                X.throws[i].t = 0;
                X.throws[i].x = (int16_t)X.cx;
                X.throws[i].y = (int16_t)X.cy;
                X.flasks--;
                audio_sfx(SFX_CURSOR);
                break;
            }
    }
    for (i = 0; i < 2; i++) {
        Throw *th = &X.throws[i];
        if (!th->active) continue;
        if (++th->t >= 12) {
            int ddx = th->x - (X.dx >> 8), ddy = th->y - (X.dy >> 8), r = BIG() ? 18 : 15;
            th->active = 0;
            X.splash_x = th->x;
            X.splash_y = th->y;
            X.splash_t = 1;
            X.splash_hit = ddx * ddx + ddy * ddy <= r * r;
            if (X.splash_hit) {
                X.hits++;
                X.demon_flash = 14;
                X.shake = 5;
                audio_sfx(SFX_DEMON_PAIN);
                taunt(X.hits >= X.need ? "AAARGH! I'M MELTING!" : "EEK! COLD! COLD!", NULL);
                X.tx = (X.dx >> 8) + (ddx > 0 ? -30 : 30);
                X.retarget = 20;
            } else {
                audio_sfx(SFX_SPLASH);
                if (rng_range(&doomo_rng, 3) == 0) taunt("Missed me, missed me!", NULL);
            }
        }
    }
    if (X.splash_t > 0 && ++X.splash_t > 16) X.splash_t = 0;
    if (X.hits >= X.need || X.st_t >= WATER_TICKS ||
        (X.flasks == 0 && !X.throws[0].active && !X.throws[1].active && X.splash_t == 0))
        end_water();
}

static void update_name(void)
{
    if (X.picked >= 0) {
        if (X.st_t > X.picked + 50) begin_verdict();
        return;
    }
    if (btn_repeat(BTN_LEFT) || btn_repeat(BTN_RIGHT)) { X.sel ^= 1; audio_sfx(SFX_CURSOR); }
    if (btn_repeat(BTN_UP) || btn_repeat(BTN_DOWN)) { X.sel ^= 2; audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_A)) {
        X.name_ok = X.sel == X.correct;
        X.picked = X.st_t;
        X.shake = 6;
        if (X.name_ok) {
            audio_sfx(SFX_DEMON_PAIN);
            taunt("NOOO! How do you know my NAME?!", NULL);
        } else {
            audio_sfx(SFX_DEMON_ROAR);
            taunt("WRONG! Hahahaha!", NULL);
        }
    }
}

static void cb_lost_confirm(int yes)
{
    if (yes && !X.practice && sim_can_lay_to_rest(X.host)) {
        sim_lay_to_rest(X.host, DEATH_LAID_TO_REST);
        audio_sfx(SFX_BELL);
        goto_state(ST_FAREWELL);
        X.dread_target = 6;
    } else {
        leave_scene();
    }
}

static void cb_lost_choice(int c)
{
    char q[96];
    if (c == 0) {
        doomo_snprintf(q, sizeof(q), "Lay %s to rest forever? This cannot be undone.", host_name());
        ui_confirm(q, cb_lost_confirm);
    } else {
        leave_scene();
    }
}

static void lost_talk(void)
{
    static const char *const opts[2] = { "Lay to rest", "Not yet..." };
    fuzz_say("I'm so sorry. The exorcisms have failed... %s's soul is lost.", host_name(), NULL);
    fuzz_say("There is one last kindness. We could lay %s to rest, and let them find peace.",
             host_name(), NULL);
    if (fuzz_id() >= 0) ui_choice(fuzz_id(), "What should we do?", opts, 2, cb_lost_choice);
    else ui_choice(-1, "Father Fuzz: What should we do?", opts, 2, cb_lost_choice);
}

static void exo_update(void)
{
    X.st_t++;
    if (X.shake > 0 && (X.st_t & 1)) X.shake--;
    if (X.taunt_t > 0) X.taunt_t--;
    if (X.fb_t < 60) X.fb_t++;
    if (X.st_t % 5 == 0 && X.dread != X.dread_target) X.dread += X.dread < X.dread_target ? 1 : -1;
    switch (X.state) {
    case ST_TOOLS: update_tools(); break;
    case ST_PRAYER: update_prayer(); break;
    case ST_WATER: update_water(); break;
    case ST_NAME: update_name(); break;
    case ST_VERDICT:
        if (X.st_t == 75) {
            X.success = rng_range(&doomo_rng, 100) < X.chance;
            if (!X.practice) {
                X.fails_before = exorcism_fails(X.host);
                exorcism_apply(X.host, X.success);
            }
            goto_state(ST_RESULT);
            X.dread_target = X.success ? 0 : 12;
            if (X.success) {
                X.dread = 3;
                audio_sfx(SFX_DEMON_DIE);
                audio_music(SONG_VICTORY);
            } else {
                audio_sfx(SFX_SAD);
                X.shake = 8;
            }
        }
        break;
    case ST_RESULT:
        if (X.st_t == 20 && X.success) audio_sfx(SFX_CONFETTI);
        if (!X.success && !X.practice && town_res(X.host)->status == RES_SOUL_LOST) {
            /* the moment it all goes wrong: hold, then the lights go out */
            if (X.st_t == 110) { audio_sfx(SFX_BELL); X.dread_target = 15; }
            if (X.st_t >= 170) { goto_state(ST_LOST); audio_music(SONG_SAD); }
            break;
        }
        if (X.st_t > 30 && btn_pressed(BTN_A | BTN_B | BTN_START)) {
            audio_sfx(SFX_SELECT);
            leave_scene();
        }
        break;
    case ST_LOST:
        if (X.st_t == 1) { audio_music(SONG_SAD); X.dread_target = 15; }
        if (X.st_t == 20 || X.st_t == 50 || X.st_t == 80) audio_sfx(SFX_BELL);
        if (X.st_t == 110) lost_talk();
        if (X.st_t > 120 && btn_pressed(BTN_B)) leave_scene();
        if (X.st_t > 120 && btn_pressed(BTN_A)) lost_talk();
        break;
    case ST_FAREWELL:
        if (X.st_t > 40 && btn_pressed(BTN_A | BTN_B | BTN_START)) leave_scene();
        break;
    default: break;
    }
}

/* ---------------------------------------------------------------------- */
/* drawing                                                                  */
static void draw_host_at(int cx, int cy, int scale, int expr, uint32_t t, int *w, int *h)
{
    const DoomoImage *img = host_image(expr);
    exo_host_frame(cx, cy, img, scale, X.shake + (X.dread >= 8 && (t & 31) < 3 ? 1 : 0),
                   X.dread, t, w, h);
}

static void draw_lane(uint32_t t)
{
    int w = W(), lane_h = BIG() ? 26 : 20, ly = H() - lane_h - (BIG() ? 10 : 6);
    int hx = BIG() ? 52 : 36, i, spawn_x = w - 10;
    int beat = X.pt >= 0 ? X.pt % EXORCISM_TICKS_PER_BEAT : 0;
    int pulse = beat < 3 ? 3 - beat : 0;
    uint8_t ribbon = X.dread < 8 ? PAL_RAMP(RAMP_PASTEL_PINK, 5) : PAL_RAMP(RAMP_VAPE, 3);
    /* ribbon */
    plat_shade_rect(4, ly - 3, w - 8, lane_h + 6, 9);
    ui_round_rect(6, ly, w - 12, lane_h, lane_h / 2, ui_darker(ribbon));
    ui_round_rect(7, ly + 1, w - 14, lane_h - 2, lane_h / 2 - 1, ribbon);
    plat_rect(12, ly + 2, w - 24, 1, ui_lighter(ribbon));
    /* beat ticks along the ribbon */
    for (i = 0; i < 8; i++) {
        int bx = hx + ((i * EXORCISM_TICKS_PER_BEAT - beat + EXORCISM_TICKS_PER_BEAT) %
                       (8 * EXORCISM_TICKS_PER_BEAT)) * (spawn_x - hx) / TRAVEL;
        if (bx > hx && bx < w - 10) plat_rect(bx, ly + lane_h / 2 - 2, 1, 4, ui_darker(ribbon));
    }
    /* the heart hit zone, thumping on the beat */
    ui_heart(hx, ly + lane_h / 2, (BIG() ? 11 : 8) + pulse, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    ui_heart(hx, ly + lane_h / 2, (BIG() ? 6 : 4), PAL_PAINT(PAINT_BUBBLEGUM, 2));
    /* prompts */
    for (i = 0; i < X.nnotes; i++) {
        const Note *n = &X.notes[i];
        int dt = n->tick - X.pt, x, y = ly + lane_h / 2, r = BIG() ? 9 : 7;
        uint8_t c;
        if (dt > TRAVEL + 4 || (n->judged && n->judged != 3 && dt < -4)) continue;
        if (n->judged == 3 && dt < -20) continue;
        x = hx + dt * (spawn_x - hx) / TRAVEL;
        if (x > w + r) continue;
        c = n->btn == 0 ? PAL_PAINT(PAINT_ROSE, 1) : n->btn == 1 ? PAL_PAINT(PAINT_MINT, 1)
                                                                : PAL_PAINT(PAINT_LAVENDER, 1);
        if (n->judged == 3) { c = PAL_GRAY; y += (-dt) / 2; }
        if (n->judged == 1 || n->judged == 2) {
            ui_sparkle(x, y, (-dt) * 3, PAL_UI_GOLD);
            continue;
        }
        ui_fill_circle(x, y, r + 1, ui_darker(c));
        ui_fill_circle(x, y, r, c);
        plat_rect(x - r / 2, y - r + 2, r / 2 + 1, 1, ui_lighter(c));
        ui_text_outline(x - 2, y - 3, note_glyph[n->btn], PAL_WHITE, ui_darker(c));
    }
    /* feedback pop */
    if (X.fb && X.fb_t < 24) {
        static const char *const fbt[4] = { "", "PERFECT!", "GOOD", "MISS..." };
        int sc = ui_pop_scale(X.fb_t);
        int fy = ly - 14 - X.fb_t / 3;
        uint8_t fc = X.fb == 1 ? PAL_UI_GOLD : X.fb == 2 ? PAL_UI_MINT : PAL_GRAY_L;
        if (sc > 200 || X.fb_t > 6)
            ui_text_outline(hx - ui_text_width(fbt[X.fb]) / 2 + 8, fy, fbt[X.fb], fc, PAL_BLACK);
    }
    if (X.combo >= 3) {
        char b[24];
        doomo_snprintf(b, sizeof(b), "%d combo!", X.combo);
        ui_text_outline(w - ui_text_width(b) - 12, ly - 11, b, PAL_UI_GOLD, PAL_RAMP(RAMP_PASTEL_PINK, 1));
    }
    if (X.pt < 4 * EXORCISM_TICKS_PER_BEAT && X.pt >= 0) {
        static const char *const cnt[4] = { "4", "3", "2", "1" };
        int k = X.pt / EXORCISM_TICKS_PER_BEAT;
        ui_text_big(w / 2 - 6, ly - 34, cnt[k], 2, PAL_WHITE, PAL_RAMP(RAMP_PASTEL_PINK, 1));
    }
    (void)t;
}

static void draw_faith_meter(int x, int y, int h, int value)
{
    int fill = h * DOOMO_CLAMP(value, 0, 100) / 100;
    ui_round_rect(x - 1, y - 1, 9, h + 2, 3, PAL_RAMP(RAMP_CANDY, 1));
    plat_rect(x + 1, y + 1, 5, h - 2, PAL_RAMP(RAMP_VAPE, 1));
    plat_rect(x + 1, y + h - 1 - fill, 5, fill, PAL_UI_GOLD);
    plat_rect(x + 1, y + h - 1 - fill, 2, fill, PAL_RAMP(RAMP_CANDY, 7));
    ui_heart(x + 3, y - 5, 3, PAL_PAINT(PAINT_BUBBLEGUM, 1));
}

static void draw_tools(uint32_t t)
{
    int w = BIG() ? 180 : 150, rowh = BIG() ? 15 : 12, h = (EXI_COUNT + 1) * rowh + (BIG() ? 50 : 44);
    int x = W() - w - (BIG() ? 14 : 4), y = BIG() ? 18 : 4, i;
    char buf[48];
    if (!ui_window_pop(x, y, w, h, WIN_CREAM, (int)X.st_t)) return;
    ui_text(x + 8, y + 5, "Ritual tools", ui_style(WIN_CREAM)->title);
    for (i = 0; i <= EXI_COUNT; i++) {
        int ry = y + 18 + i * rowh, sel = X.sel == i;
        if (sel) {
            ui_round_rect(x + 4, ry - 2, w - 8, rowh, 4, ui_style(WIN_CREAM)->accent);
            ui_cursor(x + 6, ry);
        }
        if (i == EXI_COUNT) {
            ui_text(x + 18, ry + 1, STR_ICON_STAR " Begin the ritual!", sel ? PAL_WHITE : PAL_UI_PINK_D);
            continue;
        }
        {
            int it = doomo_exorcism_items[i], on = (X.items >> i) & 1;
            int have = X.practice ? 9 : inv_count(it);
            buf[0] = (char)doomo_items[it].icon;
            buf[1] = 0;
            ui_text(x + 18, ry + 1, buf, sel ? PAL_WHITE : PAL_UI_PURPLE);
            ui_text(x + 27, ry + 1, doomo_items[it].name, sel ? PAL_WHITE : (have ? PAL_GRAY_D : PAL_GRAY));
            doomo_snprintf(buf, sizeof(buf), "x%d", have);
            ui_text(x + w - 18 - ui_text_width(buf), ry + 1, buf, sel ? PAL_WHITE : PAL_GRAY);
            ui_round_rect(x + w - 15, ry, 9, 9, 2, on ? PAL_UI_PINK_D : PAL_GRAY);
            ui_round_rect(x + w - 14, ry + 1, 7, 7, 2, on ? PAL_UI_PINK : PAL_WHITE);
            if (on) ui_text(x + w - 14, ry + 1, STR_ICON_CHECK, PAL_WHITE);
        }
    }
    /* help + odds */
    {
        int oy = y + 18 + (EXI_COUNT + 1) * rowh + 2;
        const char *help = X.sel < EXI_COUNT ? doomo_exorcism_item_help[X.sel] : "Father Fuzz is ready.";
        ui_text(x + 8, oy, help, PAL_GRAY_D);
        doomo_snprintf(buf, sizeof(buf), "Odds %d%%", X.odds);
        ui_text(x + 8, oy + 11, buf, X.odds >= 60 ? PAL_UI_MINT_D : X.odds >= 35 ? PAL_RAMP(RAMP_CANDY, 2) : PAL_UI_RED);
        ui_bar(x + 62, oy + 12, w - 72, 6, X.odds, 100,
               X.odds >= 60 ? PAL_UI_MINT : X.odds >= 35 ? PAL_UI_GOLD : PAL_UI_RED);
    }
    (void)t;
}

static void draw_water_hud(uint32_t t)
{
    int i, x = BIG() ? 34 : 24, y = BIG() ? 10 : 6, left = WATER_TICKS - X.st_t;
    char b[32];
    plat_shade_rect(x - 4, y - 3, W() - 2 * x + 8, 14, 9);
    for (i = 0; i < X.flasks && i < 12; i++) {
        int fx = x + i * 9;
        ui_fill_circle(fx + 3, y + 5, 3, PAL_PAINT(PAINT_SKY, 1));
        plat_rect(fx + 2, y, 3, 3, PAL_PAINT(PAINT_WHITE, 1));
        plat_rect(fx + 2, y + 4, 1, 1, PAL_WHITE);
    }
    for (i = 0; i < X.need; i++)
        ui_heart(W() - x - i * 11, y + 4, 4, i < X.hits ? PAL_UI_GOLD : PAL_GRAY);
    if (left < 0) left = 0;
    doomo_snprintf(b, sizeof(b), "%d", left / 30);
    ui_text_outline(W() / 2 - 6, y + 1, b, left < 150 ? PAL_UI_RED : PAL_WHITE, PAL_BLACK);
    (void)t;
}

static void draw_reticle(int x, int y, uint32_t t)
{
    int r = (BIG() ? 9 : 7) + ui_wave((int)t, 20, 1);
    plat_line(x - r - 3, y, x - r + 2, y, PAL_WHITE);
    plat_line(x + r - 2, y, x + r + 3, y, PAL_WHITE);
    plat_line(x, y - r - 3, x, y - r + 2, PAL_WHITE);
    plat_line(x, y + r - 2, x, y + r + 3, PAL_WHITE);
    ui_heart_flat(x, y, 3, PAL_PAINT(PAINT_BUBBLEGUM, 1));
    plat_rect(x - r, y - r, 1, 1, PAL_UI_PINK);
    plat_rect(x + r, y - r, 1, 1, PAL_UI_PINK);
    plat_rect(x - r, y + r, 1, 1, PAL_UI_PINK);
    plat_rect(x + r, y + r, 1, 1, PAL_UI_PINK);
}

static void name_geom(int *px, int *py, int *pw, int *ph, int *hx, int *hy)
{
    int big = BIG();
    *px = big ? 8 : 4;
    *py = big ? 6 : 3;
    *pw = W() - *px * 2;
    *ph = big ? 4 * 12 + 18 : 4 * 10 + 15;
    *hx = big ? 58 : 36;
    *hy = *py + *ph + (big ? 46 : 36);
}

static void draw_name_ui(uint32_t t)
{
    int big = BIG(), i, px, py, pw, ph, hx, hy;
    int gx, bw, bh = big ? 22 : 18, by;
    char buf[80];
    name_geom(&px, &py, &pw, &ph, &hx, &hy);
    gx = hx + (big ? 44 : 34);
    bw = (W() - gx - (big ? 12 : 6) - 4) / 2;
    by = hy - (big ? 26 : 20);
    ui_window(px, py, pw, ph, WIN_DARK);
    ui_text(px + 6, py + 4, STR_ICON_PENTA " Clues about the demon", PAL_UI_GOLD);
    for (i = 0; i < 4; i++) {
        int cy = py + 15 + i * (big ? 12 : 10);
        if (i < X.clues) {
            char line[64];
            int k, maxc = (pw - 12) / FONT_ADVANCE;
            exorcism_clue(host_ref(), i, buf, sizeof(buf));
            for (k = 0; k < maxc && k < 63 && buf[k]; k++) line[k] = buf[k];
            line[k] = 0;
            if (buf[k] && k > 3) { line[k - 1] = '.'; line[k - 2] = '.'; }
            ui_text(px + 6, cy, line, PAL_WHITE);
        } else {
            ui_text(px + 6, cy, "??? (pray better for more clues)", PAL_GRAY);
        }
    }
    for (i = 0; i < 4; i++) {
        int bx = gx + (i & 1) * (bw + 4), yy = by + (i >> 1) * (bh + 5);
        const DoomoResident *r = town_res(X.choices[i]);
        const char *n = (r && r->status != RES_EMPTY) ? r->name : roster_name(X.choices[i]);
        int sel = X.sel == i, shown = X.picked >= 0, lift = sel && !shown ? 1 + (int)((t / 8) & 1) : 0;
        uint8_t fill = sel ? PAL_UI_PURPLE : PAL_RAMP(RAMP_VAPE, 2);
        if (shown && i == X.correct) fill = PAL_UI_MINT_D;
        else if (shown && sel) fill = PAL_UI_RED;
        plat_shade_rect(bx + 2, yy + 3, bw, bh, 10);
        ui_round_rect(bx, yy - lift, bw, bh, 6, ui_darker(fill));
        ui_round_rect(bx + 1, yy + 1 - lift, bw - 2, bh - 2, 5, fill);
        plat_rect(bx + 6, yy + 2 - lift, bw - 12, 1, ui_lighter(fill));
        ui_text_center(bx + bw / 2, yy + bh / 2 - 4 - lift, n && n[0] ? n : "???", PAL_WHITE);
        if (sel && !shown) ui_cursor(bx - 4, yy + bh / 2 - 4);
    }
}

static void draw_verdict(uint32_t t)
{
    int cx = W() / 2, cy = H() / 2 + 4, size = BIG() ? 30 : 22;
    int shown = X.st_t < 45 ? X.chance * X.st_t / 45 : X.chance;
    int gold = DOOMO_MAX(2, size * shown / 100), i;
    char b[32];
    plat_shade_rect(0, 0, W(), H(), 8);
    /* the holy heart swells with the power you gathered */
    ui_heart(cx, cy, size + 3, PAL_RAMP(RAMP_VAPE, 0));
    ui_heart(cx, cy, size + 1, PAL_RAMP(RAMP_VAPE, 2));
    ui_heart(cx, cy, gold + (X.st_t >= 45 ? ui_wave((int)t, 16, 1) : 0), PAL_UI_GOLD);
    for (i = 0; i < 6; i++) {
        int a = (int)t * 30 + i * (DOOMO_ANG_FULL / 6), rr = size + 10;
        ui_sparkle(cx + ((fx_cos(a) * rr) >> 16), cy + ((fx_sin(a) * rr) >> 16),
                   (int)((t / 2 + (uint32_t)i * 3) & 15), PAL_UI_GOLD);
    }
    doomo_snprintf(b, sizeof(b), "%d%%", shown);
    ui_text_big(cx - ui_text_big_width(b, 2) / 2, cy - 8, b, 2, PAL_WHITE, PAL_RAMP(RAMP_CANDY, 1));
    ui_text_outline(cx - ui_text_width("Holy power...") / 2, cy + size + 10, "Holy power...",
                    PAL_WHITE, PAL_RAMP(RAMP_VAPE, 0));
}

static void draw_confetti(uint32_t t)
{
    int i;
    for (i = 0; i < 40; i++) {
        uint32_t h = doomo_hash32((uint32_t)i * 977u);
        int x = (int)(h % (uint32_t)W()) + ui_wave((int)t + i * 7, 40, 4);
        int y = (int)(((h >> 8) % 60) + t * (1 + (h >> 20) % 3)) % (H() + 20) - 10;
        static const uint8_t cols[5] = { PAL_UI_PINK, PAL_UI_MINT, PAL_UI_GOLD, PAL_UI_PURPLE, PAL_WHITE };
        if (i % 6 == 0) ui_heart(x, y, 2, PAL_PAINT(PAINT_BUBBLEGUM, 1));
        else plat_rect(x, y, 2 + (i & 1), 2, cols[i % 5]);
    }
}

static void draw_grave(uint32_t t)
{
    int cx = W() / 2, by = H() * 3 / 4, gw = BIG() ? 60 : 44, gh = BIG() ? 70 : 52, i;
    char b[48];
    plat_rect(0, 0, W(), H(), PAL_RAMP(RAMP_SKY, 1));
    for (i = 0; i < 30; i++) {
        uint32_t h = doomo_hash32((uint32_t)i * 4099u);
        ui_sparkle((int)(h % (uint32_t)W()), (int)((h >> 10) % (uint32_t)(by - 20)),
                   (int)((t / 4 + h) & 15), PAL_WHITE);
    }
    ui_fill_circle(W() - 40, 30, 14, PAL_RAMP(RAMP_CANDY, 7));
    ui_fill_circle(W() - 34, 26, 12, PAL_RAMP(RAMP_SKY, 1));
    plat_rect(0, by, W(), H() - by, PAL_RAMP(RAMP_GRASS, 2));
    plat_rect(0, by, W(), 2, PAL_RAMP(RAMP_GRASS, 4));
    /* the headstone */
    ui_round_rect(cx - gw / 2 - 1, by - gh - 1, gw + 2, gh + 4, gw / 2, PAL_RAMP(RAMP_STONE, 2));
    ui_round_rect(cx - gw / 2, by - gh, gw, gh + 2, gw / 2 - 1, PAL_RAMP(RAMP_STONE, 5));
    plat_rect(cx - gw / 2 + 4, by - gh + 10, 2, gh - 14, PAL_RAMP(RAMP_STONE, 6));
    ui_heart(cx, by - gh + 14, 4, PAL_PAINT(PAINT_ROSE, 1));
    ui_text_center(cx, by - gh / 2, host_name(), PAL_RAMP(RAMP_STONE, 1));
    /* flowers */
    for (i = -3; i <= 3; i++) {
        int fx = cx + i * 9, fy = by + 4 + (i & 1) * 3;
        plat_rect(fx, fy, 1, 5, PAL_RAMP(RAMP_GRASS, 5));
        ui_fill_circle(fx, fy, 2, (i & 1) ? PAL_PAINT(PAINT_BUBBLEGUM, 1) : PAL_PAINT(PAINT_LEMON, 1));
        plat_rect(fx, fy, 1, 1, PAL_WHITE);
    }
    /* a little wisp floating up and away */
    {
        int wy = by - gh - 10 - (int)(t % 90) / 2, wx = cx + ui_wave((int)t, 50, 6);
        ui_fill_circle(wx, wy, 4, PAL_PAINT(PAINT_ECTOPLASM, 2));
        ui_fill_circle(wx, wy + 4, 2, PAL_PAINT(PAINT_ECTOPLASM, 1));
        plat_rect(wx - 2, wy - 1, 1, 2, PAL_RAMP(RAMP_SKY, 0));
        plat_rect(wx + 1, wy - 1, 1, 2, PAL_RAMP(RAMP_SKY, 0));
    }
    doomo_snprintf(b, sizeof(b), "Rest well, %s.", host_name());
    ui_text_outline(cx - ui_text_width(b) / 2, BIG() ? 20 : 10, b, PAL_WHITE, PAL_RAMP(RAMP_SKY, 0));
    if (X.st_t > 40) ui_hint(STR_ICON_A " Leave flowers and go", WIN_DARK);
}

static void exo_draw(void)
{
    uint32_t t = doomo_frame_count();
    int fy = floor_y(), big = BIG(), hw, hh, w = W(), h = H();
    int fuzz_x = big ? 62 : 44, fuzz_foot = fy + (big ? 52 : 40);
    char b[96];

    if (X.state == ST_FAREWELL) { draw_grave(t); return; }
    exo_church(X.dread, t, fy);

    switch (X.state) {
    case ST_INTRO: case ST_TOOLS: case ST_NODEMON: case ST_WAIT: {
        int hx = (X.state == ST_TOOLS) ? (big ? 92 : 64) : w / 2;
        int hy = fy - (big ? 44 : 30);
        if (X.state == ST_WAIT && (X.water_done || X.rhythm)) hx = w / 2;
        draw_host_at(hx, hy, 1, EXPR_POSSESSED, t, &hw, &hh);
        exo_candles(hx, hy + hh / 2 + 4, X.practice ? 3 : exorcism_tries_left(X.host), 3, t, big);
        if (X.state != ST_TOOLS) exo_fuzz(fuzz_x, fuzz_foot, t, EXPR_HAPPY);
        if (X.state != ST_NODEMON)
            exo_demon(X.state == ST_TOOLS ? hx - hw / 2 - 2 : hx + hw / 2 + 4, hy - hh / 2 + 4, t, 0, 1);
        if (X.state == ST_TOOLS) draw_tools(t);
        break;
    }
    case ST_PRAYER: {
        int hy = fy - (big ? 46 : 34);
        draw_host_at(w / 2, hy, 1, EXPR_POSSESSED, t, &hw, &hh);
        exo_candles(w / 2, hy + hh / 2 + 3, X.practice ? 3 : exorcism_tries_left(X.host), 3, t, big);
        exo_fuzz(fuzz_x, fuzz_foot, t, EXPR_TALK);
        exo_demon(w / 2 + hw / 2 + 10, hy - hh / 2 + ui_wave((int)t, 40, 4), t, X.shake > 3 ? X.shake : 0, 1);
        draw_faith_meter(w - (big ? 28 : 22), big ? 26 : 16, big ? 70 : 46,
                         X.pt > 0 ? (X.score / DOOMO_MAX(1, X.perfects + X.goods + X.misses)) : 0);
        draw_lane(t);
        if (X.taunt_t > 0)
            exo_bubble(w / 2 + hw / 2 - (big ? 10 : 30), big ? 6 : 2, big ? 120 : 96, X.taunt,
                       w / 2 + hw / 2 + 10, hy - hh / 2 - 6, 1);
        break;
    }
    case ST_WATER: {
        int hx, hy, i;
        water_geom(&hx, &hy, &hw, &hh);
        draw_host_at(hx, hy, big ? 2 : 1, EXPR_POSSESSED, t, &hw, &hh);
        exo_demon(X.dx >> 8, X.dy >> 8, t, X.demon_flash, 1);
        for (i = 0; i < 2; i++) {
            const Throw *th = &X.throws[i];
            int u, x, y, arc;
            if (!th->active) continue;
            u = th->t * 256 / 12;
            x = w / 2 + (th->x - w / 2) * u / 256;
            y = h + 4 + (th->y - h - 4) * u / 256;
            arc = (big ? 50 : 34) * u * (256 - u) / 65536 * 4;
            ui_fill_circle(x, y - arc, 3, PAL_PAINT(PAINT_SKY, 1));
            plat_rect(x - 1, y - arc - 5, 3, 3, PAL_PAINT(PAINT_WHITE, 1));
            plat_rect(x - 1, y - arc - 1, 1, 1, PAL_WHITE);
        }
        if (X.splash_t > 0) {
            const DoomoImage *sp = sprite_image(SPR_HOLY_SPLASH, X.splash_t / 4);
            if (sp) plat_image(X.splash_x - sp->w / 2, X.splash_y - sp->h / 2, sp, 1, 0, 0);
            if (X.splash_hit && X.splash_t < 12)
                ui_text_outline(X.splash_x - 12, X.splash_y - 18 - X.splash_t, "HIT!", PAL_UI_GOLD, PAL_BLACK);
        }
        if (!X.water_done) draw_reticle(X.cx, X.cy, t);
        draw_water_hud(t);
        if (X.taunt_t > 0)
            exo_bubble((X.dx >> 8) - 50, DOOMO_MAX(18, (X.dy >> 8) - 40), 100, X.taunt,
                       X.dx >> 8, (X.dy >> 8) - 8, 1);
        break;
    }
    case ST_NAME: {
        int px, py, pw, ph, hx, hy;
        name_geom(&px, &py, &pw, &ph, &hx, &hy);
        draw_host_at(hx, hy, 1, EXPR_POSSESSED, t, &hw, &hh);
        exo_demon(hx + hw / 2 - 2, hy - hh / 2 + 4 + ui_wave((int)t, 30, 3), t,
                  X.picked >= 0 && X.name_ok ? X.st_t - X.picked : 0, 1);
        draw_name_ui(t);
        if (X.taunt_t > 0)
            exo_bubble(hx + hw / 2 + 6, h - (big ? 34 : 26), big ? 150 : 130, X.taunt,
                       hx + hw / 2, hy - hh / 2 + 10, 1);
        else if (X.picked < 0)
            exo_bubble(hx + hw / 2 + 6, h - (big ? 34 : 26), big ? 150 : 130,
                       "You'll NEVER guess my true name!", hx + hw / 2, hy - hh / 2 + 10, 1);
        break;
    }
    case ST_VERDICT: {
        int hy = fy - (big ? 48 : 34);
        draw_host_at(w / 2, hy, 1, EXPR_POSSESSED, t, &hw, &hh);
        draw_verdict(t);
        break;
    }
    case ST_RESULT: {
        int hy = fy - (big ? 48 : 34);
        int expr = X.success ? EXPR_HAPPY : EXPR_POSSESSED;
        int tries = X.practice ? 3 : exorcism_tries_left(X.host);
        draw_host_at(w / 2, hy, big ? 2 : 1, expr, t, &hw, &hh);
        exo_candles(w / 2, hy + hh / 2 + 4, tries, 3, t, big);
        if (X.success) {
            exo_fuzz(fuzz_x, fuzz_foot, t, EXPR_HAPPY);
            if (X.st_t < 24) {
                const DoomoImage *p = sprite_image(SPR_PUFF, X.st_t / 6);
                if (p) plat_image(w / 2 + hw / 2 - p->w / 2, hy - hh / 2 - p->h / 2, p, 2, 0, 0);
            }
            draw_confetti(t);
            ui_text_big(w / 2 - ui_text_big_width("CAST OUT!", 2) / 2, big ? 8 : 4, "CAST OUT!", 2,
                        PAL_UI_GOLD, PAL_RAMP(RAMP_PASTEL_PINK, 1));
            doomo_snprintf(b, sizeof(b), "%s fled from %s!", demon_name(), host_name());
            ui_text_outline(w / 2 - ui_text_width(b) / 2, big ? 30 : 22, b, PAL_WHITE, PAL_RAMP(RAMP_PASTEL_PINK, 1));
        } else {
            exo_demon(w / 2 + hw / 2 + 6, hy - hh / 2 + ui_wave((int)t, 20, 3), t, 0, 1);
            ui_text_big(w / 2 - ui_text_big_width("IT HOLDS ON...", 2) / 2, big ? 8 : 4, "IT HOLDS ON...", 2,
                        PAL_UI_RED, PAL_BLACK);
            if (X.practice) doomo_snprintf(b, sizeof(b), "Just practice! Try again anytime.");
            else if (tries > 0) doomo_snprintf(b, sizeof(b), "%d %s left before the soul is lost.",
                                               tries, tries == 1 ? "try" : "tries");
            else doomo_snprintf(b, sizeof(b), "The last candle goes out...");
            ui_text_outline(w / 2 - ui_text_width(b) / 2, big ? 30 : 22, b, PAL_WHITE, PAL_BLACK);
        }
        {
            char s[64];
            doomo_snprintf(s, sizeof(s), "Prayer %d%%  Water %d%%  Name %s  Odds %d%%",
                           X.rhythm, X.water, X.name_ok ? STR_ICON_CHECK : STR_ICON_X, X.chance);
            if (!big) doomo_snprintf(s, sizeof(s), "Pray %d%% Water %d%% Name %s",
                                     X.rhythm, X.water, X.name_ok ? STR_ICON_CHECK : STR_ICON_X);
            plat_shade_rect(0, h - (big ? 34 : 26), w, 11, 9);
            ui_text_center(w / 2, h - (big ? 32 : 24), s, PAL_WHITE);
        }
        if (X.st_t > 30 && !(!X.success && !X.practice && town_res(X.host)->status == RES_SOUL_LOST))
            ui_hint(STR_ICON_A " Done", X.success ? WIN_CUTE : WIN_HELL);
        if (!X.success && X.st_t > 110) {
            int lv = DOOMO_MIN(14, (X.st_t - 110) / 4);
            plat_shade_rect(0, 0, w, h, lv);
        }
        break;
    }
    case ST_LOST: {
        int hy = fy - (big ? 48 : 34), lv;
        draw_host_at(w / 2, hy, big ? 2 : 1, EXPR_SAD, t, &hw, &hh);
        exo_candles(w / 2, hy + hh / 2 + 4, 0, 3, t, big);
        lv = X.st_t < 30 ? 14 - X.st_t / 4 : 7;
        plat_shade_rect(0, 0, w, h, DOOMO_MAX(lv, 7));
        if (X.st_t > 25) {
            ui_text_outline(w / 2 - ui_text_width("The exorcisms have failed...") / 2, big ? 14 : 6,
                            "The exorcisms have failed...", PAL_GRAY_L, PAL_BLACK);
        }
        if (X.st_t > 60) {
            doomo_snprintf(b, sizeof(b), "%s's soul is lost.", host_name());
            ui_text_big(w / 2 - ui_text_big_width(b, big ? 2 : 1) / 2, big ? 30 : 18, b, big ? 2 : 1,
                        PAL_WHITE, PAL_UI_RED_D);
        }
        if (X.st_t > 120) ui_hint(STR_ICON_A " Talk to Father Fuzz  " STR_ICON_B " Leave", WIN_DARK);
        break;
    }
    default: break;
    }
}

const DoomoScene SCENE_EXORCISM = { "EXORCISM", exo_enter, exo_leave, exo_update, exo_draw, 0 };
