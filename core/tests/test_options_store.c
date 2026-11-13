/* test_options_store.c - U0: the options with ids 64 and up, the 'OPT2' chunk and the machine config
 * record (core/src/options_store.c). The test binary supplies a fake config store through strong
 * plat_config_* / plat_features / plat_option_changed, switched by g_cfg_on. */
#include "test.h"
#include "doomo/doomo.h"
#include "doomo/options_store.h"
#include "doomo/plat_ext.h"
#include "doomo/ending.h"
#include "doomo/family.h"
#include "doomo/save_ext.h"
#include "doomo/save_budget.h"
#include "doomo/tips.h"

static int g_cfg_on;                      /* the fake platform keeps a config record */
static int g_motor;                       /* U2: ... and a rumble motor */
static uint8_t g_cfg[PLAT_CONFIG_MAX];
static int g_cfg_len, g_cfg_writes, g_changed_opt = -1, g_changed_val;

uint32_t plat_features(void) { return (g_cfg_on ? PLATF_CONFIG : 0) | (g_motor ? PLATF_RUMBLE : 0); }
int plat_config_read(uint8_t *dst, uint32_t cap)
{
    uint32_t n = (uint32_t)g_cfg_len < cap ? (uint32_t)g_cfg_len : cap;
    memcpy(dst, g_cfg, n);
    return (int)n;
}
int plat_config_write(const uint8_t *src, uint32_t len)
{
    if (!g_cfg_on || len > PLAT_CONFIG_MAX) return 0;
    memcpy(g_cfg, src, len);
    g_cfg_len = (int)len;
    g_cfg_writes++;
    return 1;
}
void plat_option_changed(int opt, int value) { g_changed_opt = opt; g_changed_val = value; }

static void fresh(int cfg_on)
{
    test_reset();
    g_cfg_on = cfg_on;
    g_motor = 0;
    seccode_clear();                    /* U5: no code left over from an earlier test */
    memset(g_cfg, 0, sizeof(g_cfg));
    g_cfg_len = 0;
    g_cfg_writes = 0;
    g_changed_opt = -1;
    opt_defaults();
}

TEST(u0_ids_defaults_and_clamp)
{
    fresh(0);
    CHECK_EQ(OPT2_FIRST, 64);
    CHECK_EQ(opt_get(OPT_MATURE), MATURE_OFF);
    CHECK_EQ(opt_mature(), MATURE_OFF);
    CHECK_EQ(opt_get(OPT_LOSS), 1);
    CHECK_EQ(opt_get(OPT_RUMBLE), 1);
    opt_set(OPT_MATURE, MATURE_ON);
    CHECK_EQ(opt_mature(), MATURE_ON);
    opt_set(OPT_MATURE, 99);                           /* clamped to the last value */
    CHECK_EQ(opt_get(OPT_MATURE), MATURE_LEVELS - 1);
    opt_set(OPT_MATURE, -5);
    CHECK_EQ(opt_get(OPT_MATURE), 0);
    CHECK_EQ(opt_get(OPT2_END), 0);                    /* unknown id: 0, no crash */
    opt_set(OPT2_END, 1);
    opt_defaults();
    CHECK_EQ(opt_get(OPT_MATURE), MATURE_OFF);
    CHECK_EQ(opt_get(OPT_LOSS), 1);
}

TEST(u0_loss_drives_the_family)
{
    fresh(0);
    CHECK_EQ(family_loss_enabled(), 1);
    opt_set(OPT_LOSS, 0);
    CHECK_EQ(family_loss_enabled(), 0);
    opt_set(OPT_LOSS, 1);
    CHECK_EQ(family_loss_enabled(), 1);
    opt_set(OPT_LOSS, 0);
    opt_defaults();
    CHECK_EQ(family_loss_enabled(), 1);
}

TEST(u0_opt2_chunk_round_trip)
{
    const DoomoSaveExt *e = doomo_save_ext_opt2;
    uint8_t buf[SAVE_EXT_MAX_OPT2];
    CHECK(e != 0);
    CHECK_EQ_U(e->tag, SAVE_EXT_TAG_OPT2);
    CHECK(e->max_size <= SAVE_EXT_MAX_OPT2 && e->size() <= e->max_size);
    fresh(0);                                         /* no machine record: the chunk carries everything */
    opt_set(OPT_MATURE, MATURE_CENSORED);
    opt_set(OPT_LOSS, 0);
    opt_set(OPT_RUMBLE, 0);
    CHECK_EQ(e->write(buf, sizeof(buf)), e->size());
    opt_defaults();
    CHECK(e->read(buf, e->size(), e->version));        /* CL4: version 2, the tips' seen word first */
    CHECK_EQ(opt_get(OPT_MATURE), MATURE_CENSORED);
    CHECK_EQ(opt_get(OPT_LOSS), 0);
    CHECK_EQ(opt_get(OPT_RUMBLE), 0);
    CHECK_EQ(family_loss_enabled(), 0);
    /* a corrupt byte falls back to the default */
    buf[4] = 200;
    CHECK(e->read(buf, e->size(), e->version));
    CHECK_EQ(opt_get(OPT_MATURE), MATURE_OFF);
    /* a short chunk (an older version) leaves the rest alone */
    opt_set(OPT_RUMBLE, 1);
    CHECK(e->read(buf, 1, 1));
    CHECK_EQ(opt_get(OPT_RUMBLE), 1);
    /* (STKOPT) an old chunk (4 values, no Stick row) loads with the default Turn, not a leftover Strafe */
    {
        uint8_t old[1 + 4 + 4] = { 0, 0, 0, 0, MATURE_OFF, 1, 1, 1 };
        opt_set(OPT_STICK_MODE, STICK_STRAFE);
        CHECK(e->read(old, 8, 2));
        CHECK_EQ(opt_get(OPT_STICK_MODE), STICK_TURN);
        opt_set(OPT_STICK_MODE, STICK_STRAFE);
        CHECK(e->write(buf, sizeof(buf)) == e->size());
        opt_defaults();
        CHECK(e->read(buf, e->size(), e->version));      /* a new chunk keeps it */
        CHECK_EQ(opt_get(OPT_STICK_MODE), STICK_STRAFE);
        e->reset();
        CHECK_EQ(opt_get(OPT_STICK_MODE), STICK_TURN);   /* a save with no chunk */
    }
    e->reset();                                       /* an old save: loss to default */
    CHECK_EQ(opt_get(OPT_LOSS), 1);
}

TEST(cl4_seen_flags_ride_in_the_opt2_chunk)
{
    const DoomoSaveExt *e = doomo_save_ext_opt2;
    uint8_t buf[SAVE_EXT_MAX_OPT2];
    uint32_t n;
    fresh(0);                                         /* no record: the chunk carries the flags */
    tips_session_reset();
    CHECK_EQ(e->version, 2);
    CHECK(e->size() <= e->max_size);
    tip_seen_merge((1u << TIP_ISLAND) | (1u << TIP_STUDIO));
    opt_set(OPT_TIPS, 0);
    n = e->write(buf, sizeof(buf));
    CHECK_EQ(n, e->size());
    CHECK_EQ(buf[0], 1u << TIP_ISLAND);                     /* the seen word first, little endian */
    CHECK_EQ(buf[1], 1u << (TIP_STUDIO - 8));
    tips_session_reset();
    opt_set(OPT_TIPS, 1);
    CHECK(e->read(buf, n, 2));
    CHECK(tip_seen(TIP_ISLAND) && tip_seen(TIP_STUDIO) && !tip_seen(TIP_CARE));
    CHECK_EQ(opt_get(OPT_TIPS), 0);                         /* no record here: the chunk carries it */
    /* merging never forgets: a save without the flags keeps the ones already seen */
    memset(buf, 0, 4);
    CHECK(e->read(buf, n, 2));
    CHECK(tip_seen(TIP_ISLAND));
    /* a version-1 chunk (values only) still reads */
    {
        uint8_t v1[3] = { MATURE_OFF, 0, 1 };
        CHECK(e->read(v1, 3, 1));
        CHECK_EQ(opt_get(OPT_LOSS), 0);
        CHECK_EQ(opt_get(OPT_TIPS), 0);                     /* not in it: unchanged */
        CHECK(tip_seen(TIP_ISLAND));
    }
    opt_defaults();
}

TEST(u0_machine_record_written_and_read)
{
    fresh(1);
    opt_set(OPT_MATURE, MATURE_ON);
    CHECK_EQ(g_cfg_writes, 0);                       /* a change is not written at once ... */
    CHECK(opt_config_flush());                       /* ... but once, when flushed */
    CHECK_EQ(g_cfg_writes, 1);
    CHECK(!opt_config_flush());                      /* nothing changed since: nothing written */
    CHECK_EQ(g_cfg_writes, 1);
    CHECK_EQ(g_cfg_len, (int)PLAT_CONFIG_MAX);
    CHECK(g_cfg[0] == 'D' && g_cfg[1] == 'O' && g_cfg[2] == OPT_CONFIG_VERSION);
    CHECK_EQ(g_cfg[4 + 5], MATURE_ON);
    opt_set(OPT_LANGUAGE, LANG_ES);
    opt_set(OPT_MUSIC_VOL, 3);
    opt_set(OPT_RUMBLE, 0);
    CHECK(opt_config_flush());
    CHECK_EQ(g_cfg_writes, 2);
    CHECK_EQ(g_cfg[4], LANG_ES);
    CHECK_EQ(g_cfg[5], 3);
    CHECK_EQ(g_cfg[10], 0);
    /* a new boot: defaults, then the record */
    opt_defaults();
    CHECK_EQ(opt_get(OPT_MATURE), MATURE_OFF);
    CHECK(opt_config_load());
    CHECK_EQ(opt_get(OPT_MATURE), MATURE_ON);
    CHECK_EQ(opt_get(OPT_LANGUAGE), LANG_ES);
    CHECK_EQ(opt_get(OPT_MUSIC_VOL), 3);
    CHECK_EQ(opt_get(OPT_RUMBLE), 0);
    /* the chunk of a save does not override a machine-wide setting when a record exists */
    {
        uint8_t buf[SAVE_EXT_MAX_OPT2];
        const DoomoSaveExt *e = doomo_save_ext_opt2;
        opt_set(OPT_MATURE, MATURE_OFF);
        e->write(buf, sizeof(buf));
        opt_set(OPT_MATURE, MATURE_ON);
        e->read(buf, e->size(), 1);
        CHECK_EQ(opt_get(OPT_MATURE), MATURE_ON);
    }
    opt_set(OPT_LANGUAGE, LANG_EN);
}

TEST(u0_record_validation_and_security_bytes)
{
    int i;
    fresh(1);
    for (i = 32; i < 48; i++) g_cfg[i] = (uint8_t)(0xA0 + i);   /* the adults' code */
    g_cfg[0] = 'D'; g_cfg[1] = 'O'; g_cfg[2] = OPT_CONFIG_VERSION;
    g_cfg_len = 48;
    CHECK(!opt_config_load());                                   /* boot: no settings, the bytes kept */
    CHECK(!seccode_is_set());                                    /* not a valid code block */
    opt_set(OPT_MATURE, MATURE_ON);
    CHECK(opt_config_flush());                                   /* writes: keeps 32..47 */
    for (i = 32; i < 48; i++) CHECK_EQ(g_cfg[i], 0xA0 + i);
    /* a record with a flipped byte is ignored */
    opt_defaults();
    g_cfg[9] ^= 1;
    CHECK(!opt_config_load());
    CHECK_EQ(opt_get(OPT_MATURE), MATURE_OFF);
    /* wrong magic, wrong version, too short */
    g_cfg[9] ^= 1;
    g_cfg[2] = 99;
    CHECK(!opt_config_load());
    g_cfg[2] = OPT_CONFIG_VERSION;
    g_cfg_len = 10;
    CHECK(!opt_config_load());
    /* no PLATF_CONFIG: load is a no-op and nothing is written */
    g_cfg_on = 0;
    g_cfg_writes = 0;
    opt_set(OPT_MATURE, MATURE_ON);
    CHECK(!opt_config_flush());
    CHECK_EQ(g_cfg_writes, 0);
    CHECK(!opt_config_load());
}

TEST(u0_platform_is_told)
{
    fresh(0);
    opt_set(OPT_3D_FAST, 1);
    CHECK_EQ(g_changed_opt, OPT_3D_FAST);
    CHECK_EQ(g_changed_val, 1);
    opt_set(OPT_RUMBLE, 0);
    CHECK_EQ(g_changed_opt, OPT_RUMBLE);
    CHECK_EQ(g_changed_val, 0);
}

/* ---- U5: the first-boot language picker ---- */
static void frame(uint32_t b) { plat_begin_frame(); doomo_frame(b); plat_end_frame(); }

TEST(u5_first_boot_only_where_a_record_is_kept_and_missing)
{
    fresh(0);
    CHECK(!opt_config_load());
    CHECK(!opt_config_first_boot());               /* no record kept here (native): never asks */
    fresh(1);
    CHECK(!opt_config_load());
    CHECK(opt_config_first_boot());                /* kept, but empty: first boot */
    opt_config_save();
    CHECK(opt_config_load());
    CHECK(!opt_config_first_boot());               /* the record exists now */
}

static void idle(int n) { while (n-- > 0) frame(0); }
static void press(uint32_t b) { frame(b); frame(0); }

/* type four digits into SCENE_SECURITY_CODE: UP d times on each digit, A to move on / confirm */
static void enter_code(const char *c)
{
    int i, k;
    for (i = 0; i < 4; i++) {
        for (k = 0; k < c[i] - '0'; k++) press(BTN_UP);
        press(BTN_A);
    }
}

static void picker_to_mature(void)
{
    fresh(1);
    CHECK(!opt_config_load());
    scene_set(&SCENE_LANG_PICK, 0);
    idle(10);
    CHECK(scene_top() == &SCENE_LANG_PICK);
    press(BTN_A);                                  /* keep English: on to the Mature question */
    idle(10);
}

TEST(u5_picker_applies_the_choice_saves_and_hands_to_the_title)
{
    int i;
    fresh(1);
    CHECK(!scene_is_reserved_stub(&SCENE_LANG_PICK));
    CHECK(!opt_config_load());
    scene_set(&SCENE_LANG_PICK, 0);
    for (i = 0; i < 10; i++) frame(0);
    CHECK(scene_top() == &SCENE_LANG_PICK);
    frame(BTN_DOWN);                               /* English -> the next ready language */
    frame(0);
    CHECK_EQ(opt_get(OPT_LANGUAGE), LANG_ES);
    g_cfg_writes = 0;
    press(BTN_A);                                  /* language kept: the Mature question follows */
    CHECK(scene_top() == &SCENE_LANG_PICK);
    CHECK(opt_config_first_boot());                /* not done yet */
    idle(10);
    press(BTN_A);                                  /* Off, the default: no code, on to the title */
    frame(0);
    CHECK(scene_top() == &SCENE_TITLE);
    CHECK(g_cfg_writes >= 1);
    CHECK(!opt_config_first_boot());
    CHECK_EQ(opt_mature(), MATURE_OFF);
    CHECK(!seccode_is_set());
    CHECK(opt_config_load());                      /* the record keeps Spanish */
    CHECK_EQ(opt_get(OPT_LANGUAGE), LANG_ES);
    opt_set(OPT_LANGUAGE, LANG_EN);
}

TEST(u5_picker_mature_on_asks_a_grown_up_for_a_code)
{
    picker_to_mature();
    CHECK(scene_top() == &SCENE_LANG_PICK);
    press(BTN_DOWN);
    press(BTN_DOWN);
    press(BTN_DOWN);                               /* stops at On */
    press(BTN_A);
    CHECK(scene_top() == &SCENE_SECURITY_CODE);
    enter_code("1204");                            /* choose ... */
    CHECK(scene_top() == &SCENE_SECURITY_CODE);    /* ... and confirm */
    enter_code("1204");
    idle(2);
    CHECK(scene_top() == &SCENE_TITLE);
    CHECK_EQ(seccode_scene_result(), 1);
    CHECK_EQ(opt_mature(), MATURE_ON);
    CHECK(seccode_is_set());
    CHECK_EQ(seccode_check("1204"), 1);
    CHECK_EQ(seccode_check("1205"), 0);
    CHECK(!opt_config_first_boot());
}

TEST(u5_picker_censored_also_needs_a_code_and_a_mismatch_starts_over)
{
    picker_to_mature();
    press(BTN_DOWN);                               /* Censored */
    press(BTN_A);
    CHECK(scene_top() == &SCENE_SECURITY_CODE);
    enter_code("0001");
    enter_code("0002");                            /* does not match: the scene stays and starts over */
    CHECK(scene_top() == &SCENE_SECURITY_CODE);
    CHECK(!seccode_is_set());
    enter_code("0003");
    enter_code("0003");
    idle(2);
    CHECK(scene_top() == &SCENE_TITLE);
    CHECK_EQ(opt_mature(), MATURE_CENSORED);
    CHECK_EQ(seccode_check("0003"), 1);
}

/* ---- E11: Mature on the options screen --------------------------------------------- */
static void open_game_options(void)
{
    scene_set(&SCENE_OPTIONS, OPTPAGE_GAME);
    idle(4);
    CHECK(scene_top() == &SCENE_OPTIONS);
    press(BTN_UP);                                 /* wraps to "Reset page" ... */
    press(BTN_UP);                                 /* ... then the last row: Mature */
}

TEST(e11_options_mature_without_a_code_only_asks_for_18)
{
    fresh(0);
    open_game_options();
    press(BTN_A);                                  /* Off -> Censored: the 18+ question */
    CHECK(scene_top() == &SCENE_CONFIRM);
    CHECK_EQ(opt_mature(), MATURE_OFF);            /* nothing moves until yes */
    idle(2);
    press(BTN_A);                                  /* the default answer is No */
    idle(2);
    CHECK(scene_top() == &SCENE_OPTIONS);
    CHECK_EQ(opt_mature(), MATURE_OFF);
    press(BTN_A);
    idle(2);
    press(BTN_LEFT);                               /* Yes */
    press(BTN_A);
    idle(2);
    CHECK(scene_top() == &SCENE_OPTIONS);
    CHECK_EQ(opt_mature(), MATURE_CENSORED);
    press(BTN_LEFT);                               /* lowering never asks */
    CHECK(scene_top() == &SCENE_OPTIONS);
    CHECK_EQ(opt_mature(), MATURE_OFF);
}

TEST(e11_options_mature_with_a_code_asks_the_code_then_18)
{
    fresh(0);
    CHECK(seccode_set("4321"));
    open_game_options();
    press(BTN_A);
    CHECK(scene_top() == &SCENE_SECURITY_CODE);    /* the code first */
    enter_code("1111");                            /* wrong: asked again, nothing changes */
    CHECK(scene_top() == &SCENE_SECURITY_CODE);
    CHECK_EQ(opt_mature(), MATURE_OFF);
    enter_code("4321");
    idle(2);
    CHECK(scene_top() == &SCENE_CONFIRM);          /* then the 18+ question */
    idle(2);
    press(BTN_LEFT);
    press(BTN_A);
    idle(2);
    CHECK(scene_top() == &SCENE_OPTIONS);
    CHECK_EQ(opt_mature(), MATURE_CENSORED);
}

TEST(e11_options_mature_backing_out_of_the_code_changes_nothing)
{
    fresh(0);
    CHECK(seccode_set("4321"));
    open_game_options();
    press(BTN_A);
    CHECK(scene_top() == &SCENE_SECURITY_CODE);
    press(BTN_B);                                  /* never mind */
    idle(3);
    CHECK(scene_top() == &SCENE_OPTIONS);
    CHECK_EQ(opt_mature(), MATURE_OFF);
}

TEST(u5_picker_skipping_the_code_leaves_mature_off)
{
    picker_to_mature();
    press(BTN_DOWN);
    press(BTN_DOWN);                               /* On */
    press(BTN_A);
    CHECK(scene_top() == &SCENE_SECURITY_CODE);
    press(BTN_B);                                  /* B on the first digit: no code */
    idle(2);
    CHECK_EQ(seccode_scene_result(), 0);
    CHECK(scene_top() == &SCENE_LANG_PICK);        /* asked again, back on Off, nothing stuck */
    CHECK(!seccode_is_set());
    CHECK_EQ(opt_mature(), MATURE_OFF);
    idle(10);
    press(BTN_A);
    idle(2);
    CHECK(scene_top() == &SCENE_TITLE);
    CHECK_EQ(opt_mature(), MATURE_OFF);
    CHECK(!opt_config_first_boot());
}

TEST(u5_picker_b_on_the_mature_screen_goes_back_to_the_language)
{
    picker_to_mature();
    press(BTN_B);
    idle(10);
    frame(BTN_DOWN);                               /* the language list answers again */
    frame(0);
    CHECK_EQ(opt_get(OPT_LANGUAGE), LANG_ES);
    opt_set(OPT_LANGUAGE, LANG_EN);
    CHECK(opt_config_first_boot());
}

/* ---- U2: the Quick page, the Loss and Rumble rows, presets ---- */
static void open_quick(void)
{
    scene_set(&SCENE_OPTIONS, 0);                  /* callers that pass 0 land on Quick */
    idle(4);
    CHECK(scene_top() == &SCENE_OPTIONS);
}

static void apply_preset(int right_presses)
{
    int i;
    for (i = 0; i < 6; i++) press(BTN_DOWN);       /* Language, Music, Sound, Text, Difficulty, Gore, Preset */
    for (i = 0; i < right_presses; i++) press(BTN_RIGHT);
    press(BTN_A);                                  /* "Apply this preset?" */
    CHECK(scene_top() == &SCENE_CONFIRM);
    idle(2);
    press(BTN_LEFT);                               /* Yes */
    press(BTN_A);
    idle(2);
    CHECK(scene_top() == &SCENE_OPTIONS);
}

TEST(u2_quick_page_is_first_and_music_is_one_press_down)
{
    fresh(0);
    CHECK_EQ(OPTPAGE_QUICK, 0);
    open_quick();
    CHECK_EQ(opt_get(OPT_MUSIC_VOL), 7);
    press(BTN_DOWN);                               /* Language is the first row, Music the second */
    press(BTN_LEFT);
    CHECK_EQ(opt_get(OPT_MUSIC_VOL), 6);
    CHECK_EQ(opt_get(OPT_LANGUAGE), LANG_EN);
}

TEST(u2_loss_row_ends_the_life_sim_page)
{
    fresh(0);
    scene_set(&SCENE_OPTIONS, OPTPAGE_SIM);
    idle(4);
    CHECK_EQ(opt_get(OPT_LOSS), 1);
    press(BTN_UP);                                 /* wraps to "Reset page" */
    press(BTN_UP);                                 /* the last option: Loss */
    press(BTN_LEFT);
    CHECK_EQ(opt_get(OPT_LOSS), 0);
    press(BTN_RIGHT);
    CHECK_EQ(opt_get(OPT_LOSS), 1);
}

TEST(u2_rumble_row_shows_only_with_a_motor)
{
    fresh(0);
    scene_set(&SCENE_OPTIONS, OPTPAGE_CONTROLS);
    idle(4);
    press(BTN_UP); press(BTN_UP);
    press(BTN_LEFT);                               /* no motor: the last row is Turn buttons */
    CHECK_EQ(opt_get(OPT_RUMBLE), 1);
    CHECK_EQ(opt_get(OPT_TURN_SWAP), 1);
    fresh(0);
    g_motor = 1;
    scene_set(&SCENE_OPTIONS, OPTPAGE_CONTROLS);
    idle(4);
    press(BTN_UP); press(BTN_UP);
    press(BTN_LEFT);
    CHECK_EQ(opt_get(OPT_RUMBLE), 0);
    CHECK_EQ(opt_get(OPT_TURN_SWAP), 0);
    CHECK_EQ(g_changed_opt, OPT_RUMBLE);
}

TEST(u2_preset_comfort_changes_play_settings_but_not_the_machine_ones)
{
    fresh(0);
    opt_set(OPT_MUSIC_VOL, 3);
    opt_set(OPT_LANGUAGE, LANG_ES);
    opt_set(OPT_MATURE, MATURE_CENSORED);
    opt_set(OPT_MONSTER_MODE, 1);
    opt_set(OPT_CHEAT_DISCO, 1);
    opt_set(OPT_EFFECTS, 0);
    opt_set(OPT_FOV, 2);                           /* an ordinary option: back to its default first */
    open_quick();
    apply_preset(1);                               /* Comfort */
    CHECK_EQ(opt_get(OPT_DIFFICULTY), 0);
    CHECK_EQ(opt_get(OPT_AIM_ASSIST), 3);
    CHECK_EQ(opt_get(OPT_FOV), 1);
    CHECK_EQ(opt_get(OPT_MUSIC_VOL), 3);
    CHECK_EQ(opt_get(OPT_LANGUAGE), LANG_ES);
    CHECK_EQ(opt_mature(), MATURE_CENSORED);
    CHECK_EQ(opt_get(OPT_MONSTER_MODE), 1);
    CHECK_EQ(opt_get(OPT_CHEAT_DISCO), 1);
    CHECK_EQ(opt_get(OPT_EFFECTS), 0);             /* the Performance page is never touched */
}

TEST(u2_preset_hardcore_then_cozy_flips_loss_and_default_resets)
{
    fresh(0);
    open_quick();
    apply_preset(4);                               /* Hardcore */
    CHECK_EQ(opt_get(OPT_DIFFICULTY), 3);
    CHECK_EQ(opt_get(OPT_LOSS), 1);
    CHECK_EQ(opt_get(OPT_GORE), GORE_BRUTAL);
    press(BTN_A);                                  /* the row still says Hardcore */
    CHECK(scene_top() == &SCENE_CONFIRM);
    idle(2);
    press(BTN_A);                                  /* the default answer is No: nothing changes */
    idle(2);
    CHECK_EQ(opt_get(OPT_DIFFICULTY), 3);
    press(BTN_RIGHT);                              /* Max accessibility */
    press(BTN_RIGHT);                              /* wraps to Default */
    press(BTN_A);
    idle(2);
    press(BTN_LEFT);
    press(BTN_A);
    idle(2);
    CHECK_EQ(opt_get(OPT_DIFFICULTY), 1);
    CHECK_EQ(opt_get(OPT_GORE), GORE_CUTE);
    CHECK_EQ(opt_get(OPT_LOSS), 1);                /* Default has Loss on */
}

/* ---- U5: the adults' security code ---- */
TEST(u5_code_set_check_and_the_tries)
{
    int i;
    fresh(1);
    CHECK(!seccode_is_set());
    CHECK_EQ(seccode_check("1234"), 1);            /* nothing set: nothing to ask */
    CHECK_EQ(seccode_set("12"), 0);                /* not four digits */
    CHECK_EQ(seccode_set("12a4"), 0);
    CHECK_EQ(seccode_set("12345"), 0);
    CHECK_EQ(seccode_set(NULL), 0);
    CHECK(!seccode_is_set());
    CHECK_EQ(seccode_set("4821"), 1);
    CHECK(seccode_is_set());
    CHECK_EQ(seccode_tries_left(), 3);
    CHECK_EQ(seccode_check("4821"), 1);
    CHECK_EQ(seccode_check("0000"), 0);
    CHECK_EQ(seccode_tries_left(), 2);
    CHECK_EQ(seccode_check("4821"), 1);            /* a right code clears the count */
    CHECK_EQ(seccode_tries_left(), 3);
    for (i = 0; i < 3; i++) CHECK_EQ(seccode_check("0000"), 0);
    CHECK_EQ(seccode_tries_left(), 0);
    CHECK_EQ(seccode_check("4821"), -1);           /* locked, even for the right code */
    /* a restart: a locked machine grants one try per boot */
    CHECK(opt_config_load());
    CHECK_EQ(seccode_tries_left(), 1);
    CHECK_EQ(seccode_check("0000"), 0);
    CHECK_EQ(seccode_tries_left(), 0);
    CHECK(opt_config_load());
    CHECK_EQ(seccode_check("4821"), 1);            /* the right code gets back in and clears it */
    CHECK_EQ(seccode_tries_left(), 3);
    seccode_clear();
    CHECK(!seccode_is_set());
    CHECK_EQ(seccode_check("anything"), 1);
}

static int has_text(const uint8_t *b, int n, const char *t)
{
    int i, k = (int)strlen(t);
    for (i = 0; i + k <= n; i++) if (memcmp(b + i, t, (size_t)k) == 0) return 1;
    return 0;
}

TEST(u5_code_is_stored_salted_in_the_record_and_survives_a_restart)
{
    uint8_t saved[PLAT_CONFIG_MAX], hash1[4];
    fresh(1);
    opt_config_save();
    seccode_set("4821");
    CHECK_EQ(g_cfg_len, (int)PLAT_CONFIG_MAX);
    CHECK_EQ(g_cfg[32], 1);                        /* version */
    CHECK_EQ(g_cfg[33] & 1, 1);                    /* a code is set */
    CHECK(!has_text(g_cfg, (int)sizeof g_cfg, "4821"));    /* the digits themselves are never kept */
    memcpy(hash1, g_cfg + 40, 4);
    seccode_set("4821");                           /* the same code again: a new salt, a new hash */
    CHECK(memcmp(hash1, g_cfg + 40, 4) != 0);
    opt_set(OPT_MATURE, MATURE_ON);
    CHECK(opt_config_flush());                     /* a settings write keeps the code */
    CHECK_EQ(g_cfg[33] & 1, 1);
    memcpy(saved, g_cfg, sizeof saved);
    seccode_clear();                               /* forget it in RAM and in the record ... */
    CHECK(!seccode_is_set());
    CHECK_EQ(g_cfg[33], 0);
    memcpy(g_cfg, saved, sizeof saved);            /* ... and put the old record back: a restart */
    CHECK(opt_config_load());
    CHECK(seccode_is_set());
    CHECK_EQ(seccode_check("4821"), 1);
    CHECK_EQ(seccode_check("4822"), 0);
    /* a flipped byte in the block: the code is gone, the settings stay */
    memcpy(g_cfg, saved, sizeof saved);
    g_cfg[41] ^= 1;
    CHECK(opt_config_load());
    CHECK(!seccode_is_set());
    CHECK_EQ(opt_get(OPT_MATURE), MATURE_ON);
}

TEST(u5_code_works_for_the_session_where_no_record_is_kept)
{
    fresh(0);
    CHECK_EQ(seccode_set("7777"), 1);
    CHECK(seccode_is_set());
    CHECK_EQ(seccode_check("7777"), 1);
    CHECK_EQ(seccode_check("7770"), 0);
    CHECK_EQ(g_cfg_writes, 0);
}

TEST(u5_code_scene_asks_and_reports)
{
    int i;
    fresh(1);
    scene_set(&SCENE_TITLE, 0);
    scene_push(&SCENE_SECURITY_CODE, SECCODE_ASK);
    idle(2);
    CHECK(scene_top() == &SCENE_TITLE);            /* no code set: nothing to ask, passes at once */
    CHECK_EQ(seccode_scene_result(), 1);
    seccode_set("4821");
    scene_push(&SCENE_SECURITY_CODE, SECCODE_ASK);
    enter_code("4820");
    CHECK(scene_top() == &SCENE_SECURITY_CODE);    /* wrong: stays, a try is used */
    CHECK_EQ(seccode_tries_left(), 2);
    enter_code("4821");
    idle(2);
    CHECK(scene_top() == &SCENE_TITLE);
    CHECK_EQ(seccode_scene_result(), 1);
    /* three wrong in a row: locked, and the scene closes itself with result 0 */
    scene_push(&SCENE_SECURITY_CODE, SECCODE_ASK);
    for (i = 0; i < 3; i++) enter_code("0000");
    CHECK(scene_top() == &SCENE_SECURITY_CODE);
    idle(100);
    CHECK(scene_top() == &SCENE_TITLE);
    CHECK_EQ(seccode_scene_result(), 0);
    scene_push(&SCENE_SECURITY_CODE, SECCODE_ASK);   /* still locked this boot: closes again without a try */
    idle(100);
    CHECK(scene_top() == &SCENE_TITLE);
    CHECK_EQ(seccode_scene_result(), 0);
    /* B backs out of an ask (after a restart's one try) */
    CHECK(opt_config_load());
    scene_push(&SCENE_SECURITY_CODE, SECCODE_ASK);
    press(BTN_B);
    idle(2);
    CHECK(scene_top() == &SCENE_TITLE);
    CHECK_EQ(seccode_scene_result(), 0);
    CHECK_EQ(seccode_tries_left(), 1);             /* backing out used no try */
}

TEST(cl4_seen_flags_and_tips_in_the_machine_record)
{
    fresh(1);
    tips_session_reset();
    opt_set(OPT_TIPS, 0);                              /* a machine-wide option: written when flushed */
    CHECK(opt_config_flush());
    CHECK_EQ(g_cfg[11], 1);                            /* tips + 1 */
    tip_seen_merge((1u << TIP_ISLAND) | (1u << TIP_LIBRARY));
    CHECK(!tips_dirty());                              /* merged flags are not "new" */
    CHECK(opt_config_save());
    CHECK_EQ(g_cfg[12], 1u << TIP_ISLAND);
    CHECK_EQ(g_cfg[13], 1u << (TIP_LIBRARY - 8));
    CHECK_EQ(g_cfg[14] | g_cfg[15] | g_cfg[16] | g_cfg[29], 0);
    /* a new boot: the record brings both back */
    tips_session_reset();
    opt_defaults();
    CHECK_EQ(opt_get(OPT_TIPS), 1);
    CHECK(opt_config_load());
    CHECK_EQ(opt_get(OPT_TIPS), 0);
    CHECK(tip_seen(TIP_ISLAND) && tip_seen(TIP_LIBRARY) && !tip_seen(TIP_CARE));
    /* an older record (byte 11 still 0) keeps the default: On */
    g_cfg[11] = 0;
    {
        uint16_t c = 0xFFFFu;
        int i, b;
        for (i = 0; i < 30; i++) {
            c ^= (uint16_t)((uint16_t)g_cfg[i] << 8);
            for (b = 0; b < 8; b++) c = (uint16_t)((c & 0x8000u) ? ((c << 1) ^ 0x1021) : (c << 1));
        }
        g_cfg[30] = (uint8_t)c; g_cfg[31] = (uint8_t)(c >> 8);
    }
    opt_defaults();
    CHECK(opt_config_load());
    CHECK_EQ(opt_get(OPT_TIPS), 1);
    /* Show all tips again: written once the Options are left (opt_config_flush), not at once */
    g_cfg_writes = 0;
    tips_reset();
    CHECK_EQ(g_cfg_writes, 0);
    CHECK(opt_config_flush());
    CHECK_EQ(g_cfg_writes, 1);
    CHECK_EQ(g_cfg[12] | g_cfg[13], 0);
    CHECK(!tips_dirty());
    /* a seen tip is written by the next save (opt_config_flush), not when its card closes */
    tips_test_force(1);
    opt_set(OPT_TIPS, 1);
    scene_set(&SCENE_OPTIONS, 0);
    idle(2);
    CHECK_EQ(tip_once(TIP_CARE), 1);
    idle(20);
    g_cfg_writes = 0;
    press(BTN_A);
    CHECK(tip_seen(TIP_CARE));
    CHECK_EQ(g_cfg_writes, 0);
    CHECK(tips_dirty());
    g_cfg_writes = 0;
    CHECK_EQ(opt_config_flush(), 1);
    CHECK_EQ(g_cfg_writes, 1);
    CHECK(!tips_dirty());
    CHECK_EQ(opt_config_flush(), 0);                   /* nothing new: no card write */
    CHECK_EQ(g_cfg_writes, 1);
    /* the record wins over a save's chunk for the switch, but the flags still merge */
    {
        uint8_t buf[SAVE_EXT_MAX_OPT2];
        const DoomoSaveExt *e = doomo_save_ext_opt2;
        opt_set(OPT_TIPS, 1);
        tip_seen_merge(1u << TIP_PET);
        e->write(buf, sizeof(buf));
        tips_session_reset();
        opt_set(OPT_TIPS, 0);
        CHECK(e->read(buf, e->size(), e->version));
        CHECK_EQ(opt_get(OPT_TIPS), 0);
        CHECK(tip_seen(TIP_PET));
    }
    tips_session_reset();
}

/* ---- Refresh: the Options froze the game on every volume step ----
 * Each step wrote (and first read) the machine record: on the PS1 a memory card read and write, over
 * four seconds per press. Values now take effect at once and the record is written once, when the
 * Options are left, and only if something changed. */
static void options_visit_to_row(int row)
{
    int i;
    scene_set(&SCENE_TITLE, 0);
    idle(5);
    scene_push(&SCENE_OPTIONS, OPTPAGE_QUICK);
    idle(5);
    CHECK(scene_top() == &SCENE_OPTIONS);
    for (i = 0; i < row; i++) press(BTN_DOWN);
}

TEST(refresh_options_write_the_record_once_per_visit_with_changes)
{
    int i, v0;
    fresh(1);
    CHECK(!opt_config_load());
    opt_config_save();                             /* a record exists (not a first boot) */
    options_visit_to_row(1);                       /* Quick page: Language, Music, ... */
    g_cfg_writes = 0;
    v0 = opt_get(OPT_MUSIC_VOL);
    for (i = 0; i < 5; i++) press(BTN_LEFT);
    CHECK_EQ(opt_get(OPT_MUSIC_VOL), v0 - 5);      /* takes effect at once */
    for (i = 0; i < 5; i++) press(BTN_RIGHT);
    press(BTN_RIGHT);
    CHECK_EQ(opt_get(OPT_MUSIC_VOL), v0 + 1);
    for (i = 0; i < 5; i++) press(BTN_DOWN);       /* the Preset row: A asks first (a modal over the Options) */
    press(BTN_A);
    idle(2);
    CHECK(scene_top() == &SCENE_CONFIRM);
    press(BTN_B);                                  /* no: closing the modal writes nothing */
    idle(2);
    CHECK(scene_top() == &SCENE_OPTIONS);
    CHECK_EQ(g_cfg_writes, 0);                     /* ... and nothing is written while in the Options */
    press(BTN_B);
    idle(2);
    CHECK(scene_top() == &SCENE_TITLE);
    CHECK_EQ(g_cfg_writes, 1);                     /* leaving: one write */
    CHECK_EQ(g_cfg[5], v0 + 1);
    idle(10);
    scene_set(&SCENE_TITLE, 0);
    CHECK_EQ(g_cfg_writes, 1);                     /* never again for the same change */
    /* the new boot reads it */
    opt_defaults();
    CHECK(opt_config_load());
    CHECK_EQ(opt_get(OPT_MUSIC_VOL), v0 + 1);
}

TEST(refresh_options_visit_without_changes_writes_nothing)
{
    fresh(1);
    CHECK(!opt_config_load());
    opt_config_save();
    options_visit_to_row(1);
    g_cfg_writes = 0;
    press(BTN_DOWN);
    press(BTN_UP);
    press(BTN_R);                                  /* another page and back */
    press(BTN_L);
    press(BTN_B);
    idle(2);
    CHECK(scene_top() == &SCENE_TITLE);
    CHECK_EQ(g_cfg_writes, 0);
    /* a world option (Loss) is not in the record either */
    scene_push(&SCENE_OPTIONS, OPTPAGE_SIM);
    idle(2);
    opt_set(OPT_LOSS, 0);
    press(BTN_B);
    idle(2);
    CHECK_EQ(g_cfg_writes, 0);
}

TEST(refresh_a_change_outside_the_options_is_written_when_its_scene_is_left)
{
    fresh(1);
    CHECK(!opt_config_load());
    opt_config_save();
    scene_set(&SCENE_TITLE, 0);
    idle(2);
    g_cfg_writes = 0;
    CHECK(pause_music_nudge(-1));                  /* a pause menu's music row */
    CHECK(pause_music_nudge(-1));
    idle(30);
    CHECK_EQ(g_cfg_writes, 0);
    scene_set(&SCENE_TITLE, 0);                    /* back to the title (or any scene change) */
    CHECK_EQ(g_cfg_writes, 1);
    CHECK_EQ(g_cfg[5], opt_get(OPT_MUSIC_VOL));
    /* opt_config_flush (save_game calls it) writes a pending change too */
    opt_set(OPT_SFX_VOL, 2);
    CHECK(opt_config_flush());
    CHECK_EQ(g_cfg_writes, 2);
    CHECK_EQ(g_cfg[6], 2);
}
