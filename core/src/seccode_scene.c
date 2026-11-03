/* seccode_scene.c - SCENE_SECURITY_CODE: the adults' security code entry (U5; scenes.h).
 * Strong definition of the weak placeholder in weak_scenes.c. arg = SECCODE_SET or SECCODE_ASK
 * (options_store.h):
 *   SET  enter four digits, then the same four again; a match stores the code (the first-boot
 *        screen pushes this after the player picks Mature Censored or On). B on the first digit
 *        skips it (no code set).
 *   ASK  enter the existing code (E11 pushes this before Mature turns on). Three wrong tries in a
 *        row lock it for the session; seccode_check() keeps the count. Nothing set = passes.
 * UP / DOWN change the digit, A or RIGHT go to the next, LEFT / B go back, START or A on the last
 * digit confirms. The outcome is seccode_scene_result(); the scene pops itself. The digits are
 * never kept past the check: they are wiped on leave. State: 24 B. */
#include <string.h>
#include "doomo/doomo.h"
#include "doomo/ui.h"
#include "doomo/audio.h"
#include "doomo/input.h"
#include "doomo/options_store.h"
#include "doomo/platform.h"

enum { MSG_NONE, MSG_MISMATCH, MSG_WRONG, MSG_LOCKED };

static struct {
    uint8_t mode, step, pos, msg;
    int16_t t, msg_t;
    char d[SECCODE_LEN + 1], first[SECCODE_LEN + 1];
} C;
static uint8_t s_result;

int seccode_scene_result(void) { return s_result; }

static void clear_digits(void)
{
    memset(C.d, '0', SECCODE_LEN);
    C.d[SECCODE_LEN] = 0;
    C.pos = 0;
}

static void finish(int ok)
{
    s_result = (uint8_t)ok;
    memset(&C, 0, sizeof C);              /* the digits never outlive the scene */
    scene_pop();
}

static void sc_enter(int arg)
{
    memset(&C, 0, sizeof C);
    s_result = 0;
    C.mode = (uint8_t)(arg == SECCODE_ASK ? SECCODE_ASK : SECCODE_SET);
    clear_digits();
    if (C.mode == SECCODE_ASK && seccode_is_set() && seccode_tries_left() == 0) {
        C.msg = MSG_LOCKED;
        C.msg_t = 90;
    }
}

static void submit(void)
{
    int r;
    if (C.mode == SECCODE_SET) {
        if (C.step == 0) {
            memcpy(C.first, C.d, sizeof C.first);
            clear_digits();
            C.step = 1;
            C.msg = MSG_NONE;
            audio_sfx(SFX_SELECT);
        } else if (memcmp(C.first, C.d, SECCODE_LEN) == 0 && seccode_set(C.d)) {
            audio_sfx(SFX_SPARKLE);
            finish(1);
        } else {
            clear_digits();
            C.step = 0;
            C.msg = MSG_MISMATCH;
            C.msg_t = 90;
            audio_sfx(SFX_ERROR);
        }
        return;
    }
    r = seccode_check(C.d);
    if (r == 1) { audio_sfx(SFX_SELECT); finish(1); return; }
    clear_digits();
    C.msg = (r < 0 || seccode_tries_left() == 0) ? MSG_LOCKED : MSG_WRONG;   /* the last try used = locked */
    C.msg_t = 90;
    audio_sfx(SFX_ERROR);
}

static void sc_update(void)
{
    C.t++;
    if (C.mode == SECCODE_ASK && !seccode_is_set()) { finish(1); return; }     /* nothing to ask */
    if (C.msg_t > 0 && --C.msg_t == 0) {
        if (C.msg == MSG_LOCKED) { finish(0); return; }
        C.msg = MSG_NONE;
    }
    if (C.msg == MSG_LOCKED) return;
    if (btn_pressed(BTN_UP) || btn_pressed(BTN_DOWN)) {
        int up = btn_pressed(BTN_UP);
        C.d[C.pos] = (char)('0' + ((C.d[C.pos] - '0') + (up ? 1 : 9)) % 10);
        audio_sfx(SFX_CURSOR);
    }
    if (btn_pressed(BTN_RIGHT) && C.pos < SECCODE_LEN - 1) { C.pos++; audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_LEFT) && C.pos > 0) { C.pos--; audio_sfx(SFX_CURSOR); }
    if (btn_pressed(BTN_A)) {
        if (C.pos < SECCODE_LEN - 1) { C.pos++; audio_sfx(SFX_CURSOR); }
        else submit();
    } else if (btn_pressed(BTN_START)) {
        submit();
    } else if (btn_pressed(BTN_B)) {
        if (C.pos > 0) { C.pos--; audio_sfx(SFX_CURSOR); }
        else if (C.mode == SECCODE_SET && C.step == 1) { clear_digits(); C.step = 0; audio_sfx(SFX_CANCEL); }
        else { audio_sfx(SFX_CANCEL); finish(0); }       /* SET: skip the code; ASK: never mind */
    }
}

static void sc_draw(void)
{
    const UiStyle *st = ui_style(WIN_CUTE);
    int sw = plat_screen_w(), sh = plat_screen_h(), i, y, x0, cell = 22;
    const char *title, *sub;
    char one[2];
    ui_backdrop(WIN_CUTE, (uint32_t)doomo_frame_count());
    ui_window(12, sh / 2 - 54, sw - 24, 108, WIN_CUTE);
    y = sh / 2 - 48;
    title = C.mode == SECCODE_ASK ? "Grown-ups' code" : "Set a grown-ups' code";
    ui_text_center(sw / 2, y, title, st->text);
    if (C.mode == SECCODE_ASK) sub = "Enter the code to go on.";
    else sub = C.step == 0 ? "Pick four digits." : "Enter them once more.";
    ui_text_center(sw / 2, y + 14, sub, st->text_dim);
    x0 = sw / 2 - (SECCODE_LEN * cell) / 2 + cell / 2;
    one[1] = 0;
    for (i = 0; i < SECCODE_LEN; i++) {
        int cx = x0 + i * cell;
        one[0] = i < C.pos ? '*' : i == C.pos ? C.d[i] : '-';
        ui_text_center(cx, y + 36, one, i == C.pos ? st->text : st->text_dim);
        if (i == C.pos) plat_rect(cx - 6, y + 48, 12, 1, st->text);
    }
    if (C.msg == MSG_MISMATCH) ui_text_center(sw / 2, y + 62, "They did not match.", st->text);
    else if (C.msg == MSG_WRONG) ui_text_center(sw / 2, y + 62, "Not that one. Try again.", st->text);
    else if (C.msg == MSG_LOCKED) ui_text_center(sw / 2, y + 62, "Locked. Ask again later.", st->text);
    else ui_text_center(sw / 2, y + 62, "UP / DOWN: digit", st->text_dim);
    if (C.msg == MSG_NONE) {
        ui_text_center(sw / 2, y + 76, C.mode == SECCODE_SET && C.step == 0 ? "B: skip the code" : "B: back", st->text_dim);
        if ((C.t >> 4) & 1) ui_text_center(sw / 2, y + 90, STR_ICON_A " Next", st->text_dim);
    }
}

const DoomoScene SCENE_SECURITY_CODE = { "SECURITY_CODE", sc_enter, NULL, sc_update, sc_draw, 0 };
