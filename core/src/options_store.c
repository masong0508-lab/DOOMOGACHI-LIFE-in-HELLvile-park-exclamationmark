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
#include "doomo/rng.h"

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

/* ---- the adults' security code (U5 / E11; user answer 20) ----
 * A 4-digit parental PIN that turning Mature content on asks for. It lives in bytes 32..47 of the
 * machine config record (life_contract.h CONFIG_SZ_SECURITY), never as the digits themselves:
 *   0 version (1) | 1 flags (bit 0: a code is set) | 2 wrong tries in a row | 3 0 |
 *   4..7 salt | 8..11 hash of salt + digits | 12..13 0 | 14..15 CRC-16/CCITT of bytes 0..13
 * The RAM copy is always kept (so a platform without a record, native, still has a code for the
 * session); a platform with a record gets it written on every change (s_sec_dirty). Tries: three
 * wrong in a row lock the session; after a reboot a locked record grants one try per boot until a
 * right code clears the count, so a forgotten code never bricks the machine and brute force is
 * one guess per restart. */
#define SEC_VERSION 1
#define SEC_TRIES   3
static uint8_t s_sec[CONFIG_SZ_SECURITY];
static uint8_t s_sec_dirty;
static uint8_t s_sec_wrong;      /* wrong tries this boot (RAM only) */

static int sec_digits_ok(const char *c)
{
    int i;
    if (!c) return 0;
    for (i = 0; i < SECCODE_LEN; i++) if (c[i] < '0' || c[i] > '9') return 0;
    return c[SECCODE_LEN] == 0;
}

static uint32_t sec_hash(const uint8_t *salt, const char *code)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < 4; i++) h = (h ^ salt[i]) * 16777619u;
    for (i = 0; i < SECCODE_LEN; i++) h = (h ^ (uint8_t)code[i]) * 16777619u;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
    return h;
}

static void sec_seal(void)
{
    uint16_t c = crc16(s_sec, 14);
    s_sec[14] = (uint8_t)c; s_sec[15] = (uint8_t)(c >> 8);
}

static int sec_block_ok(const uint8_t *b)
{
    return b[0] == SEC_VERSION && crc16(b, 14) == (uint16_t)(b[14] | (b[15] << 8));
}

int seccode_is_set(void) { return s_sec[0] == SEC_VERSION && (s_sec[1] & 1); }

int seccode_tries_left(void)
{
    int r = s_sec[2];
    if (!seccode_is_set()) return SEC_TRIES;
    if (r < SEC_TRIES) return SEC_TRIES - r;
    return s_sec_wrong == 0 ? 1 : 0;
}

int seccode_set(const char *code)
{
    uint32_t salt, h;
    int i;
    if (!sec_digits_ok(code)) return 0;
    salt = rng_next(&doomo_rng);
    memset(s_sec, 0, sizeof s_sec);
    s_sec[0] = SEC_VERSION; s_sec[1] = 1;
    for (i = 0; i < 4; i++) s_sec[4 + i] = (uint8_t)(salt >> (8 * i));
    h = sec_hash(s_sec + 4, code);
    for (i = 0; i < 4; i++) s_sec[8 + i] = (uint8_t)(h >> (8 * i));
    sec_seal();
    s_sec_wrong = 0;
    s_sec_dirty = 1;
    opt_config_save();
    return 1;
}

void seccode_clear(void)
{
    memset(s_sec, 0, sizeof s_sec);
    s_sec_wrong = 0;
    s_sec_dirty = 1;
    opt_config_save();
}

int seccode_check(const char *code)
{
    uint32_t h;
    int i, ok = 1;
    if (!seccode_is_set()) return 1;                  /* nothing to ask */
    if (seccode_tries_left() == 0) return -1;
    if (!sec_digits_ok(code)) return 0;
    h = sec_hash(s_sec + 4, code);
    for (i = 0; i < 4; i++) ok &= s_sec[8 + i] == (uint8_t)(h >> (8 * i));
    if (ok) {
        if (s_sec[2]) { s_sec[2] = 0; sec_seal(); s_sec_dirty = 1; opt_config_save(); }
        s_sec_wrong = 0;
        return 1;
    }
    if (s_sec[2] < 255) s_sec[2]++;
    if (s_sec_wrong < 255) s_sec_wrong++;
    sec_seal();
    s_sec_dirty = 1;
    opt_config_save();
    return 0;
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
    if (n >= (int)PLAT_CONFIG_MAX && sec_block_ok(rec + OPT_CONFIG_SETTINGS_SZ)) memcpy(s_sec, rec + OPT_CONFIG_SETTINGS_SZ, sizeof s_sec);
    else memset(s_sec, 0, sizeof s_sec);
    s_sec_dirty = 0;
    s_sec_wrong = 0;
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
    if (s_sec_dirty) memcpy(rec + OPT_CONFIG_SETTINGS_SZ, s_sec, sizeof s_sec);
    if (plat_config_write(rec, PLAT_CONFIG_MAX)) { s_sec_dirty = 0; return 1; }
    return 0;
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
