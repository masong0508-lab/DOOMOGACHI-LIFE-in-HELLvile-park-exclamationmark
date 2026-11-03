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

static int g_cfg_on;                      /* the fake platform keeps a config record */
static uint8_t g_cfg[PLAT_CONFIG_MAX];
static int g_cfg_len, g_cfg_writes, g_changed_opt = -1, g_changed_val;

uint32_t plat_features(void) { return g_cfg_on ? PLATF_CONFIG : 0; }
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
    CHECK(e->read(buf, e->size(), 1));
    CHECK_EQ(opt_get(OPT_MATURE), MATURE_CENSORED);
    CHECK_EQ(opt_get(OPT_LOSS), 0);
    CHECK_EQ(opt_get(OPT_RUMBLE), 0);
    CHECK_EQ(family_loss_enabled(), 0);
    /* a corrupt byte falls back to the default */
    buf[0] = 200;
    CHECK(e->read(buf, e->size(), 1));
    CHECK_EQ(opt_get(OPT_MATURE), MATURE_OFF);
    /* a short chunk (an older version) leaves the rest alone */
    opt_set(OPT_RUMBLE, 1);
    CHECK(e->read(buf, 1, 1));
    CHECK_EQ(opt_get(OPT_RUMBLE), 1);
    e->reset();                                       /* an old save: loss to default */
    CHECK_EQ(opt_get(OPT_LOSS), 1);
}

TEST(u0_machine_record_written_and_read)
{
    fresh(1);
    opt_set(OPT_MATURE, MATURE_ON);
    CHECK(g_cfg_writes >= 1);
    CHECK_EQ(g_cfg_len, (int)PLAT_CONFIG_MAX);
    CHECK(g_cfg[0] == 'D' && g_cfg[1] == 'O' && g_cfg[2] == OPT_CONFIG_VERSION);
    CHECK_EQ(g_cfg[4 + 5], MATURE_ON);
    opt_set(OPT_LANGUAGE, LANG_ES);
    CHECK_EQ(g_cfg[4], LANG_ES);
    opt_set(OPT_MUSIC_VOL, 3);
    CHECK_EQ(g_cfg[5], 3);
    opt_set(OPT_RUMBLE, 0);
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
    opt_set(OPT_MATURE, MATURE_ON);                              /* writes: keeps 32..47 */
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
    opt_set(OPT_MATURE, MATURE_ON);                /* a settings write keeps the code */
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
