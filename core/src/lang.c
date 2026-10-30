/* lang.c - the language setting and the UI string lookup (owner: foundation;
 * the contract is in include/doomo/lang.h).
 *
 * lang_tr() is on the draw path of every text call in Spanish mode, so it is
 * kept to one hash and a binary search over 32-bit keys (about a dozen
 * probes for the PS1's table); in English mode it is a compare and a return.
 */
#include "doomo/doomo.h"
#include "doomo/lang.h"

/* core/gen/lang_ui_es.c; absent on the PS1, whose table comes from the disc */
extern const uint32_t doomo_lang_ui_es[] __attribute__((weak));

/* the long strings (option help...): core/gen/lang_help_es.c, not linked on the PS1 */
extern const uint32_t doomo_lang_help_es[] __attribute__((weak));

/* catalogs, jobs, unlock hints...: core/gen/lang_cat_es.c, not linked on the PS1 */
extern const uint32_t doomo_lang_cat_es[] __attribute__((weak));

extern const uint32_t doomo_lang_ui2_es[] __attribute__((weak));

__attribute__((weak)) const uint32_t *plat_lang_ui(void) { return doomo_lang_ui_es; }
__attribute__((weak)) void plat_lang_changed(int lang) { (void)lang; }

static int8_t s_pref = -1;          /* the session's chosen language, -1 = none yet */

int lang_get(void)
{
    return doomo_town.opt[OPT_LANGUAGE] == LANG_ES ? LANG_ES : LANG_EN;
}

uint32_t lang_hash(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
    return h;
}

static const char *find(const uint32_t *t, const char *s, uint32_t h)
{
    const uint32_t *key;
    const uint16_t *off;
    unsigned lo = 0, hi, n;
    if (!t || t[0] != LANG_UI_MAGIC) return NULL;
    n = t[1] & 0xFFFFu;
    key = t + 2;
    off = (const uint16_t *)(key + n);
    hi = n;
    (void)s;
    while (lo < hi) {
        unsigned mid = (lo + hi) >> 1;
        if (key[mid] < h) lo = mid + 1;
        else hi = mid;
    }
    if (lo < n && key[lo] == h) return (const char *)t + (t[1] >> 16) + off[lo];
    return NULL;
}

const char *lang_tr(const char *s)
{
    const char *r;
    uint32_t h;
    if (!s || !s[0] || doomo_town.opt[OPT_LANGUAGE] != LANG_ES) return s;
    h = lang_hash(s);
    r = find(plat_lang_ui(), s, h);
    if (!r) r = find(doomo_lang_help_es, s, h);
    if (!r) r = find(doomo_lang_cat_es, s, h);
    if (!r) r = find(doomo_lang_ui2_es, s, h);
    return r ? r : s;
}

void lang_changed(void)
{
    s_pref = (int8_t)lang_get();
    plat_lang_changed(s_pref);
}

void lang_loaded(void)
{
    if (s_pref < 0) s_pref = (int8_t)lang_get();             /* nothing chosen yet: adopt the save's */
    else doomo_town.opt[OPT_LANGUAGE] = (uint8_t)s_pref;     /* keep this session's language */
    plat_lang_changed(s_pref);
}
