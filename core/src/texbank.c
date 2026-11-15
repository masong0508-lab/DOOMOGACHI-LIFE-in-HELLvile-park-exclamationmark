/* texbank.c - reader of the per-level texture bank (LV7a; see doomo/texbank.h for the format). */
#include "doomo/texbank.h"
#include "doomo/textures.h"
#include <string.h>

#if !defined(DOOMO_PLATFORM_PS1)

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int texbank_open(DoomoTexBank *b, const uint8_t *blob, uint32_t len)
{
    DoomoTexBank t;
    uint32_t total = 0, need;
    int i, k;
    if (!b || !blob) return TB_ERR_NULL;
    if (len < DOOMO_TEXBANK_HDR) return TB_ERR_SHORT;
    if (blob[0] != 'L' || blob[1] != 'V' || blob[2] != 'T' || blob[3] != 'B') return TB_ERR_MAGIC;
    if (blob[4] != DOOMO_TEXBANK_VERSION || rd16(blob + 6) != 0) return TB_ERR_VERSION;
    if (blob[5] < 1 || blob[5] > DOOMO_TEXBANK_MAX) return TB_ERR_COUNT;
    t.count = blob[5];
    need = DOOMO_TEXBANK_HDR + (uint32_t)DOOMO_TEXBANK_ENT * t.count;
    if (len < need) return TB_ERR_SHORT;
    t.ent = blob + DOOMO_TEXBANK_HDR;
    for (i = 0; i < t.count; i++) {
        const uint8_t *e = t.ent + i * DOOMO_TEXBANK_ENT;
        int n = 0;
        for (k = 0; k < DOOMO_TEXBANK_NAME; k++) {
            uint8_t c = e[k];
            if (!c) break;
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return TB_ERR_NAME;
            n++;
        }
        if (n == 0 || k == DOOMO_TEXBANK_NAME) return TB_ERR_NAME;   /* empty, or no NUL */
        for (; k < DOOMO_TEXBANK_NAME; k++)
            if (e[k]) return TB_ERR_NAME;                           /* bytes after the NUL */
        for (k = 0; k < i; k++)
            if (!strcmp((const char *)e, (const char *)(t.ent + k * DOOMO_TEXBANK_ENT))) return TB_ERR_NAME;
        if (e[12] < 1 || e[12] > DOOMO_TEXBANK_FRAMES_MAX) return TB_ERR_FRAMES;
        if (e[14] != DOOMO_ANIM_LOOP && e[14] != DOOMO_ANIM_PINGPONG) return TB_ERR_MODE;
        t.first[i] = (uint8_t)total;
        total += e[12];
        if (total > DOOMO_TEXBANK_TOTAL_MAX) return TB_ERR_FRAMES;
    }
    for (; i < DOOMO_TEXBANK_MAX; i++) t.first[i] = 0;
    if (rd32(blob + 8) != len || len != need + total * (uint32_t)DOOMO_TEX_SIZE_PX) return TB_ERR_LENGTH;
    t.pix = blob + need;
    *b = t;
    return TB_OK;
}

int texbank_slot(const DoomoTexBank *b, const char *name)
{
    int i;
    if (!b || !name) return -1;
    for (i = 0; i < b->count; i++)
        if (!strcmp(name, (const char *)(b->ent + i * DOOMO_TEXBANK_ENT))) return i;
    return -1;
}

const char *texbank_name(const DoomoTexBank *b, int slot)
{
    if (!b || slot < 0 || slot >= b->count) return "";
    return (const char *)(b->ent + slot * DOOMO_TEXBANK_ENT);
}

int texbank_frames(const DoomoTexBank *b, int slot)
{
    if (!b || slot < 0 || slot >= b->count) return 0;
    return b->ent[slot * DOOMO_TEXBANK_ENT + 12];
}

uint8_t texbank_avg(const DoomoTexBank *b, int slot)
{
    if (!b || slot < 0 || slot >= b->count) return 0;
    return b->ent[slot * DOOMO_TEXBANK_ENT + 15];
}

static int frame_at(const DoomoTexBank *b, int slot, uint16_t time)
{
    const uint8_t *e = b->ent + slot * DOOMO_TEXBANK_ENT;
    int n = e[12], sp = e[13] ? e[13] : 1;
    if (n <= 1) return 0;
    if (e[14] == DOOMO_ANIM_PINGPONG) {
        int period = 2 * (n - 1), k = (int)((time / (unsigned)sp) % (unsigned)period);
        return k < n ? k : period - k;
    }
    return (int)((time / (unsigned)sp) % (unsigned)n);
}

const uint8_t *texbank_pixels(const DoomoTexBank *b, int slot, uint16_t time)
{
    if (!b || slot < 0 || slot >= b->count) return NULL;
    return b->pix + (uint32_t)(b->first[slot] + frame_at(b, slot, time)) * DOOMO_TEX_SIZE_PX;
}

#if DOOMO_TEXBANK_ACTIVE
/* ---- the active bank: the texture ids 56..63 of the level on screen ---- */
#define TB_UID_BASE 0x00B10000u     /* const-image uids below 0x01000000 (textures.h): one per frame, per bank serial */
static const DoomoTexBank *s_active;
static DoomoImage s_img[DOOMO_TEXBANK_TOTAL_MAX];
static uint8_t s_serial;

void texbank_activate(const DoomoTexBank *b)
{
    int i, total;
    s_active = b;
    s_serial++;
    if (!b) return;
    total = b->first[b->count - 1] + b->ent[(b->count - 1) * DOOMO_TEXBANK_ENT + 12];
    /* a fresh uid per frame and per activation: a renderer that caches by uid never keeps the
     * pixels of the bank it showed before */
    for (i = 0; i < total; i++) {
        s_img[i].w = 64; s_img[i].h = 64;
        s_img[i].pix = b->pix + (uint32_t)i * DOOMO_TEX_SIZE_PX;
        s_img[i].uid = TB_UID_BASE + ((uint32_t)s_serial << 5) + (uint32_t)i;
    }
}

const DoomoTexBank *texbank_active(void) { return s_active; }

int texbank_id_slot(int tex_id)
{
    int s = tex_id - DOOMO_TEXBANK_FIRST;
    if (!s_active || s < 0 || s >= s_active->count) return -1;
    return s;
}

const uint8_t *texbank_id_pixels(int tex_id, uint16_t time)
{
    int s = texbank_id_slot(tex_id);
    return s < 0 ? NULL : texbank_pixels(s_active, s, time);
}

const DoomoImage *texbank_id_image(int tex_id, uint16_t time)
{
    int s = texbank_id_slot(tex_id);
    if (s < 0) return NULL;
    return &s_img[s_active->first[s] + frame_at(s_active, s, time)];
}
#endif

#endif /* !PS1 */
