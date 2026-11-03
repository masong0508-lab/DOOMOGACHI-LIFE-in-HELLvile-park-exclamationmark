/* options_store.c - U0: the options with ids 64 and up, the 'OPT2' chunk and the machine config
 * record (owner: U0; add-only ids in town.h, layout in options_store.h).
 *
 * Why a second store: doomo_town.opt is a full 64 B table inside every save. New options live here:
 *   - OPT_MATURE  Off / Censored / On / On + pack. opt_mature() answers it (weak_endings.c has the
 *                 "Off" default; this file overrides it). Machine-wide: it is NOT taken from a save.
 *   - OPT_LOSS    family loss on / off (family_loss_set). A property of the world: saved with it.
 *   - OPT_RUMBLE  controller rumble on / off, apart from Screen shake. Machine-wide.
 * Machine-wide options (these two plus language, the volumes and OPT_3D_FAST) go to the config
 * record (plat_config_read/write) when the platform keeps one (PLATF_CONFIG). Without one (native,
 * today) they are carried by the 'OPT2' chunk instead, so a save still remembers them.
 */
#include "doomo/doomo.h"
#include "doomo/options_store.h"
#include "doomo/save_ext.h"
#include "doomo/save_budget.h"
#include "doomo/plat_ext.h"
#include "doomo/ending.h"
#include "doomo/family.h"
#include "doomo/lang.h"

static const char *const v_off_on_[] = { "Off", "On" };
static const char *const v_mature_[] = { "Off", "Censored", "On", "On + pack" };

const DoomoOptionInfo doomo_option2_info[OPT2_COUNT] = {
    /* OPT_MATURE */ { "Mature",  MATURE_LEVELS, MATURE_OFF, v_mature_, OPTPAGE_GAME },
    /* OPT_LOSS   */ { "Loss",    2, 1, v_off_on_, OPTPAGE_SIM },
    /* OPT_RUMBLE */ { "Rumble",  2, 1, v_off_on_, OPTPAGE_CONTROLS }
};

static uint8_t s_opt2[OPT2_COUNT] = { MATURE_OFF, 1, 1 };
static uint8_t s_quiet;          /* applying a record: no writes back, no platform calls */

static int is_machine(int opt)
{
    return opt == OPT_LANGUAGE || opt == OPT_MUSIC_VOL || opt == OPT_SFX_VOL || opt == OPT_VOICE_VOL ||
           opt == OPT_3D_FAST || opt == OPT_MATURE || opt == OPT_RUMBLE;
}

static void apply2(int opt)
{
    int v = s_opt2[opt - OPT2_FIRST];
    if (opt == OPT_LOSS) family_loss_set(v);
    if (opt == OPT_RUMBLE && !s_quiet) plat_option_changed(opt, v);
}

int opt2_get(int opt)
{
    int i = opt - OPT2_FIRST, v;
    if (i < 0 || i >= OPT2_COUNT) return 0;
    v = s_opt2[i];
    return v < doomo_option2_info[i].count ? v : doomo_option2_info[i].def;
}

void opt2_set(int opt, int value)
{
    int i = opt - OPT2_FIRST;
    if (i < 0 || i >= OPT2_COUNT) return;
    s_opt2[i] = (uint8_t)DOOMO_CLAMP(value, 0, doomo_option2_info[i].count - 1);
    apply2(opt);
    opt_machine_changed(opt, s_opt2[i]);
}

void opt2_defaults(void)
{
    int i;
    for (i = 0; i < OPT2_COUNT; i++) {
        s_opt2[i] = doomo_option2_info[i].def;
        apply2(OPT2_FIRST + i);
    }
}

int opt_mature(void) { return opt2_get(OPT_MATURE); }

/* ---- the machine config record ---- */
static uint16_t crc16(const uint8_t *p, int n)
{
    uint16_t c = 0xFFFFu;
    int i, b;
    for (i = 0; i < n; i++) {
        c ^= (uint16_t)((uint16_t)p[i] << 8);
        for (b = 0; b < 8; b++) c = (uint16_t)((c & 0x8000u) ? ((c << 1) ^ 0x1021) : (c << 1));
    }
    return c;
}

static int have_config(void) { return (plat_features() & PLATF_CONFIG) != 0; }

/* one machine-wide id at record byte 4 + k */
static const int16_t k_rec[7] = { OPT_LANGUAGE, OPT_MUSIC_VOL, OPT_SFX_VOL, OPT_VOICE_VOL, OPT_3D_FAST, OPT_MATURE, OPT_RUMBLE };

static const DoomoOptionInfo *info_of(int opt)
{
    return opt >= OPT2_FIRST ? &doomo_option2_info[opt - OPT2_FIRST] : &doomo_option_info[opt];
}

static void put_machine(int opt, int v)
{
    const DoomoOptionInfo *in = info_of(opt);
    if (v < 0 || v >= in->count) v = in->def;
    if (opt >= OPT2_FIRST) { s_opt2[opt - OPT2_FIRST] = (uint8_t)v; apply2(opt); }
    else doomo_town.opt[opt] = (uint8_t)v;
}

static int s_first_boot;       /* U5: the platform keeps a record and none was there at boot */

int opt_config_first_boot(void) { return s_first_boot; }
void opt_config_first_boot_done(void) { s_first_boot = 0; }

int opt_config_load(void)
{
    uint8_t rec[PLAT_CONFIG_MAX];
    int n, k, lang_before = opt_get(OPT_LANGUAGE);
    s_first_boot = 0;
    if (!have_config()) return 0;
    memset(rec, 0, sizeof(rec));
    n = plat_config_read(rec, sizeof(rec));
    if (n < (int)OPT_CONFIG_SETTINGS_SZ || rec[0] != 'D' || rec[1] != 'O' || rec[2] != OPT_CONFIG_VERSION ||
        crc16(rec, 30) != (uint16_t)(rec[30] | (rec[31] << 8))) {
        s_first_boot = 1;                                   /* no valid record: first boot (U5) */
        return 0;
    }
    s_quiet = 1;
    for (k = 0; k < 7; k++) put_machine(k_rec[k], rec[4 + k]);
    s_quiet = 0;
    if (opt_get(OPT_LANGUAGE) != lang_before) lang_changed();
    return 1;
}

int opt_config_save(void)
{
    uint8_t rec[PLAT_CONFIG_MAX];
    uint16_t c;
    int n, k;
    if (!have_config()) return 0;
    memset(rec, 0, sizeof(rec));
    n = plat_config_read(rec, sizeof(rec));                 /* keep bytes 32..47 (the security code) */
    if (n < (int)OPT_CONFIG_SETTINGS_SZ || rec[0] != 'D' || rec[1] != 'O') memset(rec, 0, sizeof(rec));
    rec[0] = 'D'; rec[1] = 'O'; rec[2] = OPT_CONFIG_VERSION; rec[3] = 0;
    for (k = 0; k < 7; k++) rec[4 + k] = (uint8_t)opt_get(k_rec[k]);
    memset(rec + 11, 0, 19);
    c = crc16(rec, 30);
    rec[30] = (uint8_t)c; rec[31] = (uint8_t)(c >> 8);
    return plat_config_write(rec, PLAT_CONFIG_MAX);
}

void opt_machine_changed(int opt, int value)
{
    if (s_quiet) return;
    if (opt == OPT_3D_FAST) plat_option_changed(opt, value);
    if (is_machine(opt)) opt_config_save();
}

/* ---- the 'OPT2' chunk: OPT2_COUNT value bytes ---- */
static uint32_t opt2_size(void) { return OPT2_COUNT; }

static uint32_t opt2_write(uint8_t *dst, uint32_t cap)
{
    int i;
    if (cap < OPT2_COUNT) return 0;
    for (i = 0; i < OPT2_COUNT; i++) dst[i] = (uint8_t)opt2_get(OPT2_FIRST + i);
    return OPT2_COUNT;
}

static int opt2_read(const uint8_t *src, uint32_t len, uint16_t version)
{
    uint32_t i;
    (void)version;
    for (i = 0; i < len && i < OPT2_COUNT; i++) {
        int opt = OPT2_FIRST + (int)i;
        if (is_machine(opt) && have_config()) continue;       /* the machine's record wins */
        put_machine(opt, src[i]);
    }
    return 1;
}

static void opt2_reset(void)
{
    /* an old save without the chunk: the world's options go to their defaults, the machine's stay */
    s_opt2[OPT_LOSS - OPT2_FIRST] = doomo_option2_info[OPT_LOSS - OPT2_FIRST].def;
    apply2(OPT_LOSS);
    if (!have_config()) {
        s_opt2[OPT_MATURE - OPT2_FIRST] = MATURE_OFF;
        s_opt2[OPT_RUMBLE - OPT2_FIRST] = 1;
    }
}

static const DoomoSaveExt opt2_ext = {
    SAVE_EXT_TAG_OPT2, 1, opt2_size, opt2_write, opt2_read, opt2_reset, SAVE_EXT_MAX_OPT2
};
const DoomoSaveExt *const doomo_save_ext_opt2 = &opt2_ext;
