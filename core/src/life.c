/* life.c - the 'LIFE' save chunk (owner: life; LIFESIM_PLAN LS1).
 *
 * A list of sections ([id u8][version u8][length u16 LE][bytes], life.h).
 * Each section is a row in `sections` below: later packages (F1 family,
 * LS3 events, F5 pets, F6 loss, the household tier) add a row and their
 * own functions; the chunk keeps version 1. Sections with nothing to say
 * are not written, so an island without a family costs only the news.
 * On load: unknown ids are skipped (a newer save), a section whose read()
 * rejects it is reset, and sections the save lacks are reset (an older
 * save). A new town resets every section (save_ext_reset_all).
 */
#include "doomo/doomo.h"
#include "doomo/life.h"
#include "doomo/life_contract.h"
#include "doomo/family.h"
#include "doomo/save_budget.h"

typedef struct LifeSection {
    uint8_t  id, version;
    uint32_t (*size)(void);                         /* 0 = not written    */
    uint32_t (*write)(uint8_t *dst, uint32_t cap);
    int      (*read)(const uint8_t *src, uint32_t len);   /* 1 ok      */
    void     (*reset)(void);
} LifeSection;

static const LifeSection sections[] = {
    { LIFE_SEC_NEWS, 1, sim_news_save_size, sim_news_save, sim_news_load, sim_news_clear },
    { LIFE_SEC_FAMILY, 1, family_save_size, family_save, family_load, family_clear },   /* F1 */
    { LIFE_SEC_POOL, 1, family_pool_save_size, family_pool_save, family_pool_load, family_pool_reset },   /* F1 */
    { LIFE_SEC_BOND, 1, family_bond_save_size, family_bond_save, family_bond_load, family_bond_clear },   /* F7/F8 */
    { LIFE_SEC_LOSS, 1, family_loss_save_size, family_loss_save, family_loss_load, family_loss_clear },   /* F6 */
};
#define NSEC ((int)DOOMO_COUNTOF(sections))

static uint32_t life_size(void)
{
    uint32_t sz = 0, s;
    int i;
    for (i = 0; i < NSEC; i++)
        if ((s = sections[i].size()) > 0) sz += LIFE_SEC_HDR + s;
    return sz;
}

static uint32_t life_write(uint8_t *dst, uint32_t cap)
{
    uint32_t o = 0, s, w;
    int i;
    for (i = 0; i < NSEC; i++) {
        if ((s = sections[i].size()) == 0) continue;
        if (o + LIFE_SEC_HDR + s > cap || s > 0xFFFFu) return o;
        w = sections[i].write(dst + o + LIFE_SEC_HDR, s);
        dst[o] = sections[i].id;
        dst[o + 1] = sections[i].version;
        dst[o + 2] = (uint8_t)w;
        dst[o + 3] = (uint8_t)(w >> 8);
        o += LIFE_SEC_HDR + w;
    }
    return o;
}

static void life_reset(void)
{
    int i;
    for (i = 0; i < NSEC; i++) sections[i].reset();
}

static int life_read(const uint8_t *src, uint32_t len, uint16_t version)
{
    uint32_t o = 0, seen = 0;
    int i;
    if (version < 1) return 0;
    while (o + LIFE_SEC_HDR <= len) {
        uint32_t l = (uint32_t)src[o + 2] | ((uint32_t)src[o + 3] << 8);
        int id = src[o];
        if (o + LIFE_SEC_HDR + l > len) break;          /* truncated: stop */
        for (i = 0; i < NSEC; i++) {
            if (sections[i].id != id || (seen & (1u << i))) continue;
            if (!sections[i].read(src + o + LIFE_SEC_HDR, l)) sections[i].reset();
            seen |= 1u << i;
            break;
        }
        o += LIFE_SEC_HDR + l;
    }
    for (i = 0; i < NSEC; i++)
        if (!(seen & (1u << i))) sections[i].reset();
    return 1;
}

static const DoomoSaveExt life_ext = {
    SAVE_EXT_TAG_LIFE, LIFE_CHUNK_VERSION, life_size, life_write, life_read, life_reset,
    SAVE_EXT_MAX_LIFE
};
const DoomoSaveExt *const doomo_save_ext_life = &life_ext;

typedef char life_news_fits_[(LIFE_SEC_HDR + LIFE_SZ_NEWS <= LIFE_SZ_LS1_PLAN) ? 1 : -1];
typedef char life_news_len_[(NEWS_MAX == 32 && NEWS_FREE == 2 && NEWS_LEN == 64) ? 1 : -1];
