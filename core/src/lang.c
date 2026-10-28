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

const char *lang_tr(const char *s)
{
    const uint32_t *t, *key;
    const uint16_t *off;
    unsigned lo, hi, n;
    uint32_t h;
    if (!s || !s[0] || doomo_town.opt[OPT_LANGUAGE] != LANG_ES) return s;
    t = plat_lang_ui();
    if (!t || t[0] != LANG_UI_MAGIC) return s;
    n = t[1] & 0xFFFFu;
    key = t + 2;
    off = (const uint16_t *)(key + n);
    h = lang_hash(s);
    lo = 0;
    hi = n;
    while (lo < hi) {
        unsigned mid = (lo + hi) >> 1;
        if (key[mid] < h) lo = mid + 1;
        else hi = mid;
    }
    if (lo < n && key[lo] == h) return (const char *)t + (t[1] >> 16) + off[lo];
    return s;
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
