/* weak_endings.c - weak defaults for the endings and content pack
 * contracts (owner: E0, LIFESIM_PLAN E0 / R4): ending.h (E1 defines the
 * strong ones in ending.c), pack.h (E2 in pack.c) and opt_mature (U0 in
 * options.c / options_store.c).
 *
 * The defaults are the safe "nothing there" answers, so the game and
 * every save stay as they are until the owners land:
 *   - no ending is ever recorded or seen; the finale resolves to E01;
 *   - mature_adult_ok() refuses everyone, so no mature slot, sexy ending or
 *     route scene can play for anyone;
 *   - there is no content pack (every mslot plays its fade-to-black);
 *   - opt_mature() is MATURE_OFF, so every `jmat` guard takes its cutaway.
 * ending_category and ending_visible are the real rules (pure table and
 * logic); E1 may keep them. Only the mutator ending_mark logs "STUB:", so a
 * runner script with --assert-no-stubs notices an ending that was meant
 * to be recorded. No .bss: they cost no RAM on any port.
 */
#include "doomo/doomo.h"
#include "doomo/ending.h"
#include "doomo/pack.h"

#define WEAK __attribute__((weak))

/* ---- the Mature option (U0) ---- */
WEAK int opt_mature(void) { return MATURE_OFF; }

/* ---- endings (E1) ---- */
static const uint8_t ending_cats[ENDING_COUNT] = {
    ENDCAT_GOOD, ENDCAT_GOOD, ENDCAT_GOOD, ENDCAT_GOOD,           /* E01..E04 */
    ENDCAT_BAD, ENDCAT_BAD, ENDCAT_BAD, ENDCAT_BAD,               /* E05..E08 */
    ENDCAT_SECRET, ENDCAT_SECRET, ENDCAT_SECRET,                  /* E09..E11 */
    ENDCAT_SEXY, ENDCAT_SEXY, ENDCAT_SEXY,                        /* E12..E14 */
    ENDCAT_BONUS                                                  /* "Nope."  */
};

WEAK int ending_category(int k)
{
    return (k >= 0 && k < ENDING_COUNT) ? ending_cats[k] : -1;
}

WEAK int ending_visible(int k)
{
    int c = ending_category(k);
    if (c < 0 || ENDING_IS_RETIRED(k)) return 0;                  /* retired slots are never listed */
    if (c == ENDCAT_SEXY) return opt_mature() != MATURE_OFF;      /* answer 21 */
    if (c == ENDCAT_BONUS) return ending_seen(k);                 /* answer 19 */
    return 1;
}

WEAK DoomoEnds *ending_state(void) { return NULL; }
WEAK int ending_seen(int k) { (void)k; return 0; }
WEAK void ending_mark(int k) { (void)k; plat_log("STUB: ending_mark"); }
WEAK int ending_count_seen(void) { return 0; }
WEAK int ending_resolve_finale(void) { return ENDING_E01; }
WEAK int ending_partner(void) { return -1; }

/* ---- the adult gate (E1): nobody until the real allow-list lands ---- */
WEAK int mature_adult_ok(int id) { (void)id; return 0; }

/* ---- content packs (E2): none ---- */
WEAK int pack_open(void) { return 0; }

WEAK int pack_info(DoomoPackInfo *out)
{
    if (out) memset(out, 0, sizeof(*out));
    return 0;
}

WEAK const uint8_t *pack_art(int kind, uint32_t uid, uint32_t *size)
{
    (void)kind; (void)uid;
    if (size) *size = 0;
    return NULL;
}

WEAK uint32_t pack_art_read(int kind, uint32_t uid, void *dst, uint32_t cap)
{
    (void)kind; (void)uid; (void)dst; (void)cap;
    return 0;
}

WEAK int pack_script(int slot) { (void)slot; return 0; }
