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
