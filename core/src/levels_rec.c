/* levels_rec.c - the records of the 24 newer Hell Breach levels and the
 * state of the New Game+ "Arrears" arc: the 'BRCH' save chunk (owner:
 * levels; LV0, levels plan section 8.5; save_ext.h).
 *
 * The old seven levels keep their ranks in PROG_BREACH_RANKS and their
 * golds as unlock ids (no migration); everything newer is here, because
 * the PROG counters and the 1024 unlock bits are all taken.
 *
 * The chunk, version 1 (LV_BRCH_MAX = 148 B; 80 B on the GBA):
 *   0      version (1)
 *   1..3   a 24-bit mask, little endian: bit i = a record of level 7 + i
 *          follows (every build writes the levels of its own manifest)
 *   4..51  the arc, LV_BRCH_ARC bytes:
 *            0      act (LVA_*)
 *            1      the previous run's last ending + 1 (0 = none)
 *            2..3   16 flag bits
 *            4..5   the buy-back mask
 *            6..7   the two choice bytes
 *            8      the fourth player line
 *            9      held residents, 0..16
 *            10..41 their ids, 16 x u16
 *            42..47 spare, 0
 *   52..   4 B per record, in level order: rank 3 bits | gold 1 | feats 3 |
 *          cleared 1; secrets found (best); 16 collectible bits
 * All little endian. A fresh island is all zero and writes an empty chunk
 * (like 'ENDS'); a save without the chunk, an empty one or one this build
 * cannot read loads as "nothing recorded". A record of a level this build
 * does not list is skipped on load.
 *
 * The working copy is the arc and the records as they are stored: 144 B of
 * .bss (GBA: 76 B, its seven levels; the PS1: none, see LV_TABLE_N). Only the four chunk functions are
 * called from outside the levels (by save.c through the registry).
 */
#include "levels_int.h"
#include "doomo/save_budget.h"

#if LV_HAS_NEW          /* the PS1 builds none of this for now (LV_TABLE_N) */

#define ARC_ACT     0
#define ARC_ENDING  1
#define ARC_FLAGS   2
#define ARC_BUYBACK 4
#define ARC_CHOICE  6
#define ARC_LINE4   8
#define ARC_HELD_N  9
#define ARC_HELD    10
DOOMO_STATIC_ASSERT(ARC_HELD + 2 * LV_ARC_HELD <= LV_BRCH_ARC, arc_fits_its_bytes);
DOOMO_STATIC_ASSERT(LV2_COUNT == 24, the_mask_is_24_bits);

static struct {
    uint8_t arc[LV_BRCH_ARC];
    uint8_t rec[LV_REC_N][4];
} R;

/* the record slot of new level 7 + i on this build, -1 = not listed */
#define REC_SLOT(i) lv_rec_slot(LV_OLD_COUNT + (i))

static uint8_t *rec_of(int level)
{
    int k = lv_rec_slot(level);
    return k < 0 ? NULL : R.rec[k];
}

/* ---- per-level records ---------------------------------------------------------- */
int lv_rec_rank(int level)
{
    const uint8_t *r = rec_of(level);
    int v = r ? r[0] & 7 : RANK_NONE;
    return v > RANK_S ? RANK_S : v;
}

int lv_rec_set_rank(int level, int rank)
{
    uint8_t *r = rec_of(level);
    if (!r || rank > RANK_S || rank <= lv_rec_rank(level)) return 0;
    r[0] = (uint8_t)((r[0] & ~7) | rank);
    return 1;
}

int lv_rec_gold(int level)
{
    const uint8_t *r = rec_of(level);
    return r ? (r[0] >> 3) & 1 : 0;
}

void lv_rec_set_gold(int level)
{
    uint8_t *r = rec_of(level);
    if (r) r[0] |= 1 << 3;
}

int lv_rec_feats(int level)
{
    const uint8_t *r = rec_of(level);
    return r ? (r[0] >> 4) & LV_FEAT_MASK : 0;
}

void lv_rec_add_feats(int level, int mask)
{
    uint8_t *r = rec_of(level);
    if (r) r[0] |= (uint8_t)((mask & LV_FEAT_MASK) << 4);
}

int lv_rec_cleared(int level)
{
    const uint8_t *r = rec_of(level);
    return r ? r[0] >> 7 : 0;
}

void lv_rec_set_cleared(int level)
{
    uint8_t *r = rec_of(level);
    if (r) r[0] |= 1 << 7;
}

int lv_rec_secrets(int level)
{
    const uint8_t *r = rec_of(level);
    return r ? r[1] : 0;
}

void lv_rec_set_secrets(int level, int n)
{
    uint8_t *r = rec_of(level);
    if (n > 255) n = 255;
    if (r && n > r[1]) r[1] = (uint8_t)n;
}

unsigned lv_rec_collect(int level)
{
    const uint8_t *r = rec_of(level);
    return r ? (unsigned)(r[2] | (r[3] << 8)) : 0u;
}

void lv_rec_add_collect(int level, unsigned mask)
{
    uint8_t *r = rec_of(level);
    if (!r) return;
    r[2] |= (uint8_t)mask;
    r[3] |= (uint8_t)(mask >> 8);
}

/* ---- the arc ---------------------------------------------------------------------- */
static unsigned arc16(int at) { return (unsigned)(R.arc[at] | (R.arc[at + 1] << 8)); }
static void arc16_set(int at, unsigned v) { R.arc[at] = (uint8_t)v; R.arc[at + 1] = (uint8_t)(v >> 8); }

int  lv_arc_act(void) { return R.arc[ARC_ACT]; }
void lv_arc_set_act(int act) { R.arc[ARC_ACT] = (uint8_t)DOOMO_CLAMP(act, LVA_NONE, LVA_DONE); }
int  lv_arc_finished(void) { return R.arc[ARC_ACT] == LVA_DONE; }

int lv_arc_flag(int i) { return i >= 0 && i < LV_ARC_FLAGS ? (int)((arc16(ARC_FLAGS) >> i) & 1u) : 0; }

void lv_arc_set_flag(int i, int on)
{
    unsigned f = arc16(ARC_FLAGS);
    if (i < 0 || i >= LV_ARC_FLAGS) return;
    arc16_set(ARC_FLAGS, on ? f | (1u << i) : f & ~(1u << i));
}

unsigned lv_arc_buyback(void) { return arc16(ARC_BUYBACK); }
void lv_arc_set_buyback(unsigned mask) { arc16_set(ARC_BUYBACK, mask); }

int lv_arc_choice(int i) { return i >= 0 && i < 2 ? R.arc[ARC_CHOICE + i] : 0; }
void lv_arc_set_choice(int i, int v) { if (i >= 0 && i < 2) R.arc[ARC_CHOICE + i] = (uint8_t)v; }

int lv_arc_held_count(void) { return R.arc[ARC_HELD_N]; }

unsigned lv_arc_held(int i)
{
    return i >= 0 && i < R.arc[ARC_HELD_N] ? arc16(ARC_HELD + 2 * i) : LV_ARC_NOBODY;
}

int lv_arc_hold(unsigned resident)
{
    int i, n = R.arc[ARC_HELD_N];
    for (i = 0; i < n; i++) if (arc16(ARC_HELD + 2 * i) == resident) return 1;
    if (n >= LV_ARC_HELD || resident >= LV_ARC_NOBODY) return 0;
    arc16_set(ARC_HELD + 2 * n, resident);
    R.arc[ARC_HELD_N] = (uint8_t)(n + 1);
    return 1;
}

void lv_arc_release(unsigned resident)
{
    int i, n = R.arc[ARC_HELD_N];
    for (i = 0; i < n; i++) {
        if (arc16(ARC_HELD + 2 * i) != resident) continue;
        /* the last one takes its place; the freed entry goes back to 0 */
        arc16_set(ARC_HELD + 2 * i, arc16(ARC_HELD + 2 * (n - 1)));
        arc16_set(ARC_HELD + 2 * (n - 1), 0);
        R.arc[ARC_HELD_N] = (uint8_t)(n - 1);
        return;
    }
}

int  lv_arc_last_ending(void) { return R.arc[ARC_ENDING] ? R.arc[ARC_ENDING] - 1 : 255; }
void lv_arc_set_last_ending(int ending) { R.arc[ARC_ENDING] = (uint8_t)(ending >= 0 && ending < 255 ? ending + 1 : 0); }

int  lv_arc_line4(void) { return R.arc[ARC_LINE4]; }
void lv_arc_set_line4(int v) { R.arc[ARC_LINE4] = (uint8_t)v; }

/* ---- the chunk -------------------------------------------------------------------- */
static void brch_reset(void) { memset(&R, 0, sizeof(R)); }

static uint32_t brch_size(void)
{
    const uint8_t *p = (const uint8_t *)&R;
    uint32_t i;
    for (i = 0; i < sizeof(R); i++) if (p[i]) return LV_BRCH_HDR + (uint32_t)sizeof(R);
    return 0;                                    /* nothing recorded: an empty chunk */
}

static uint32_t brch_write(uint8_t *d, uint32_t cap)
{
    uint32_t n = brch_size(), mask = 0;
    int i;
    if (!n || cap < n) return 0;
    for (i = 0; i < LV2_COUNT; i++) if (REC_SLOT(i) >= 0) mask |= 1u << i;
    d[0] = LV_BRCH_VERSION;
    d[1] = (uint8_t)mask; d[2] = (uint8_t)(mask >> 8); d[3] = (uint8_t)(mask >> 16);
    memcpy(d + LV_BRCH_HDR, &R, sizeof(R));
    return n;
}

static int brch_read(const uint8_t *s, uint32_t len, uint16_t ver)
{
    uint32_t mask, need = LV_BRCH_HDR + LV_BRCH_ARC;
    const uint8_t *p = s + LV_BRCH_HDR + LV_BRCH_ARC;
    int i;
    brch_reset();
    if (ver != LV_BRCH_VERSION || len < need) return 0;
    mask = (uint32_t)(s[1] | (s[2] << 8) | (s[3] << 16));
    for (i = 0; i < LV2_COUNT; i++) need += ((mask >> i) & 1u) * 4u;
    if (len != need) return 0;
    memcpy(R.arc, s + LV_BRCH_HDR, LV_BRCH_ARC);
    if (R.arc[ARC_ACT] > LVA_DONE || R.arc[ARC_HELD_N] > LV_ARC_HELD) { brch_reset(); return 0; }
    for (i = 0; i < LV2_COUNT; i++) {
        int k;
        if (!((mask >> i) & 1u)) continue;
        k = REC_SLOT(i);
        if (k >= 0) memcpy(R.rec[k], p, 4);      /* else: a level this build does not list */
        p += 4;
    }
    return 1;
}

static const DoomoSaveExt brch_ext = {
    SAVE_EXT_TAG_BRCH, LV_BRCH_VERSION, brch_size, brch_write, brch_read, brch_reset, SAVE_EXT_MAX_BRCH
};
const DoomoSaveExt *const doomo_save_ext_breach = &brch_ext;

#endif /* LV_HAS_NEW */
