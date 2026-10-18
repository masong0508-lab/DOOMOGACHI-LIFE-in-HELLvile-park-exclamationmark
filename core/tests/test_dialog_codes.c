/* test_dialog_codes.c - the comic-timing dialogue (ui_dialog.c S1 M3;
 * SPEC sec.7 S1 "test_dialog_codes": holds per punctuation mark; {d}
 * appears whole; {punch} cannot be skipped; the pill is drawn only on PILL
 * lines; a 200-character decoded line fits the queue entry). */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/txt.h"
#include "doomo/camfx.h"
#include "../src/ui_internal.h"

static UiDialogDebug dbg(void) { UiDialogDebug d; ui_dialog_debug(&d); return d; }

static void frame(int buttons)
{
    plat_begin_frame();
    doomo_frame((uint32_t)buttons);
    plat_end_frame();
}

static void fresh(int speed)
{
    test_reset();
    scene_set(&SCENE_UI_GALLERY, 0);
    opt_set(OPT_TEXT_SPEED, speed);
}

/* frames until the top line is fully shown (no input) */
static int frames_to_reveal(const char *text, int speed)
{
    int n = 0;
    fresh(speed);
    ui_say(-1, text);
    while (n < 400) {
        UiDialogDebug d;
        frame(0);
        n++;
        d = dbg();
        if (d.text && d.shown >= (int)strlen(d.text)) break;
    }
    return n;
}

TEST(dialog_codes_punctuation_holds)
{
    /* one glyph per tick at speed 1: the holds are the difference */
    int plain = frames_to_reveal("abcdefgh", 1);
    int comma = frames_to_reveal("abc,defg", 1);
    int stop = frames_to_reveal("abc.defg", 1);
    int bang = frames_to_reveal("abc!defg", 1);
    int ask = frames_to_reveal("abc?defg", 1);
    int dots = frames_to_reveal("ab...efg", 1);
    int pause = frames_to_reveal("abcd{p 9}efgh", 1);
    int r8 = frames_to_reveal("abcd{..}efgh", 1);
    CHECK_EQ(comma - plain, 3);
    CHECK_EQ(stop - plain, 6);
    CHECK_EQ(bang - plain, 4);
    CHECK_EQ(ask - plain, 5);
    CHECK_EQ(dots - plain, 15);                    /* ... = 5/5/5 */
    CHECK_EQ(pause - plain, 9);
    CHECK_EQ(r8 - plain, 8);
    CHECK_STR(dbg().text, "abcdefgh");             /* codes are stripped */
}

TEST(dialog_codes_deadpan_appears_whole)
{
    UiDialogDebug d;
    fresh(1);
    ui_say(-1, "It is fine. {d}Everything is completely fine.");
    frame(0); frame(0);
    d = dbg();
    CHECK(d.shown >= 2 && d.shown < 12);           /* the setup types */
    while (!dbg().deadpan) frame(0);
    frame(0);
    d = dbg();
    CHECK_EQ(d.shown, (int)strlen(d.text));        /* the rest appears at once */
    fresh(0);
    ui_say_ex(-1, EXPR_NEUTRAL, DL_DEADPAN, "beep.");
    frame(0);
    CHECK_EQ(dbg().shown, 5);
}

TEST(dialog_codes_punch_cannot_be_skipped)
{
    int n;
    fresh(1);
    ui_say(-1, "{punch}It was the Mayor all along.");
    frame(0);
    frame(BTN_A);                                   /* A does not rush a punchline */
    CHECK(dbg().shown < 6);
    for (n = 0; n < 200 && dbg().shown < (int)strlen(dbg().text); n++) frame(n & 1 ? BTN_A : 0);
    CHECK(ui_dialog_active());
    frame(BTN_A);                                   /* still in the closing hold */
    frame(0);
    frame(BTN_A);
    CHECK(ui_dialog_active());
    for (n = 0; n < 20; n++) frame(0);
    frame(BTN_A);
    frame(0);
    CHECK(!ui_dialog_active());
    /* a normal line: A completes it at once */
    fresh(1);
    ui_say(-1, "A normal line that types slowly.");
    frame(0);
    frame(BTN_A);
    CHECK_EQ(dbg().shown, (int)strlen(dbg().text));
}

TEST(dialog_codes_pill_only_on_pill_lines)
{
    uint32_t before;
    int i;
    fresh(3);
    resident_generate(town_res(1), 3);
    before = dbg().pill_draws;
    ui_say_ex(1, EXPR_HAPPY, 0, "Welcome!");
    for (i = 0; i < 4; i++) frame(0);
    CHECK_EQ(dbg().pill_draws, before);
    CHECK_EQ(dbg().expr, EXPR_HAPPY);
    for (i = 0; i < 30 && ui_dialog_active(); i++) frame(i & 1 ? BTN_A : 0);
    ui_say_ex(1, EXPR_HAPPY, DL_PILL, "Thank you.");
    for (i = 0; i < 4; i++) frame(0);
    CHECK(dbg().pill_draws > before);
}

TEST(dialog_codes_long_lines_fit_the_entry)
{
    char line[260];
    int i;
    fresh(1);
    for (i = 0; i < 199; i++) line[i] = (char)('a' + i % 26);
    line[199] = 0;
    ui_say(-1, line);
    CHECK_EQ((int)strlen(dbg().text), 199);
    /* codes do not count against the 200 characters */
    fresh(1);
    memcpy(line, "{d}{..}", 7);
    for (i = 7; i < 206; i++) line[i] = (char)('a' + i % 26);
    line[206] = 0;
    ui_say(-1, line);
    CHECK_EQ((int)strlen(dbg().text), 199);
    /* story breaks: "/" is a line, "//" a page */
    fresh(3);
    ui_say_ex(-1, 0, 0, "One. / Two. // Three.");
    CHECK_STR(dbg().text, "One.\nTwo.Three.");
    frame(0);
    CHECK_EQ(dbg().shown, 9);                       /* page 1 stops at the break */
    frame(BTN_A);
    frame(0);
    CHECK_EQ(dbg().page, 9);
}

TEST(dialog_codes_sting_and_scratch_drive_the_fx)
{
    int i, shook = 0;
    fresh(1);
    ui_say(-1, "WHAT{!} {r}is that.");
    for (i = 0; i < 12; i++) { frame(0); if (camfx_shaking()) shook = 1; }
    CHECK(shook);
}
