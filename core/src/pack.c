/* pack.c - the user content pack reader (E2; format: include/doomo/pack.h,
 * built and validated offline by tools/mpack/mpack.py, same rules).
 *
 * pack_open() finds the platform's pack (plat_pack_blob: addressable, the
 * GBA mature ROM / a native file; else plat_pack_read: a disc file that
 * streams), checks it once (header, port, caps, CRC, every entry) and keeps
 * 80 bytes of state. Nothing here allocates: art is returned in place (RAW,
 * addressable) or decoded into the caller's buffer.
 *
 * Linked everywhere, costs nothing until a scene calls it: the weak
 * defaults in weak_endings.c are overridden here, and the linker drops what
 * nothing references (PS1 / GBA: --gc-sections). */
#include <string.h>

#include "doomo/pack.h"
#include "doomo/txt.h"

#if defined(DOOMO_PLATFORM_GBA)
#define THIS_PORT PK_PORT_GBA
#define PORT_CAP  PACK_CAP_GBA_HARD
#elif defined(DOOMO_PLATFORM_PS1)
#define THIS_PORT PK_PORT_PS1
#define PORT_CAP  PACK_CAP_DISC
#elif defined(DOOMO_PLATFORM_DC)
#define THIS_PORT PK_PORT_DC
#define PORT_CAP  PACK_CAP_DISC
#else
#define THIS_PORT PK_PORT_NATIVE
#define PORT_CAP  PACK_CAP_DISC
#endif

static struct {
    const uint8_t *blob;                /* addressable pack, else NULL (streams) */
    DoomoPackInfo  info;
    uint32_t       total;
    uint16_t       n;
    uint16_t       user_set;            /* bit i: user text pack 112+i registered by us */
    uint8_t        open;
} P;

static const uint8_t *pack_bytes(uint32_t off, uint32_t len)
{
    return (P.blob && off <= P.total && len <= P.total - off) ? P.blob + off : NULL;
}

static uint32_t le32(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }
static uint32_t le16(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8; }

/* read len bytes at off into dst; 1 on success */
static int rd(uint32_t off, void *dst, uint32_t len)
{
    const uint8_t *p = pack_bytes(off, len);
    if (p) { memcpy(dst, p, len); return 1; }
    if (P.blob || off > P.total || len > P.total - off) return 0;
    return plat_pack_read(off, dst, len) == len;
}

typedef struct { uint32_t uid, off, size; uint8_t kind, codec; uint16_t arg; } Ent;

static int get_ent(uint32_t i, Ent *e)
{
    uint8_t b[PACK_ENTRY_SIZE];
    if (!rd(PACK_HEADER_SIZE + i * PACK_ENTRY_SIZE, b, PACK_ENTRY_SIZE)) return 0;
    e->uid = le32(b); e->off = le32(b + 4); e->size = le32(b + 8);
    e->kind = b[12]; e->codec = b[13]; e->arg = (uint16_t)le16(b + 14);
    return 1;
}

static uint32_t crc_step(uint32_t c, const uint8_t *p, uint32_t n)
{
    static const uint32_t t[16] = {          /* IEEE, reflected, 4 bits at a time */
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu };
    while (n--) {
        c ^= *p++;
        c = (c >> 4) ^ t[c & 15];
        c = (c >> 4) ^ t[c & 15];
    }
    return c;
}

static int pack_crc_ok(uint32_t want)
{
    uint8_t buf[128];
    uint32_t off = 16, c = 0xFFFFFFFFu;
    const uint8_t *p = pack_bytes(16, P.total - 16);
    if (p) return (crc_step(c, p, P.total - 16) ^ 0xFFFFFFFFu) == want;
    while (off < P.total) {
        uint32_t n = P.total - off < sizeof buf ? P.total - off : (uint32_t)sizeof buf;
        if (!rd(off, buf, n)) return 0;
        c = crc_step(c, buf, n);
        off += n;
    }
    return (c ^ 0xFFFFFFFFu) == want;
}

void pack_close(void)
{
    int i;
    for (i = 0; i < (int)TXT_PACK_USER_COUNT; i++)
        if (P.user_set & (1u << i)) txt_user_set(TXT_PACK_USER_FIRST + i, NULL);
    memset(&P, 0, sizeof P);
}

/* validate the pack behind (P.blob | plat_pack_read) of P.total bytes */
static int check_and_register(void)
{
    uint8_t h[PACK_HEADER_SIZE];
    uint32_t i, last = 0, end, dir_end;
    uint16_t used = 0;
    Ent e;
    if (P.total < PACK_HEADER_SIZE || !rd(0, h, sizeof h)) return 0;
    if (le32(h) != PACK_MAGIC || le16(h + 4) != PACK_VERSION || le16(h + 6) != PACK_HEADER_SIZE) return 0;
    if (le32(h + 8) != P.total || P.total > PORT_CAP) return 0;
    if (h[18] != THIS_PORT || (h[19] & ~(PACK_F_SFW_EXAMPLE | PACK_F_ATTESTED))) return 0;
    P.n = (uint16_t)le16(h + 16);
    if (P.n > PACK_ENTRIES_MAX || le32(h + 20) != PACK_HEADER_SIZE) return 0;
    dir_end = PACK_HEADER_SIZE + (uint32_t)P.n * PACK_ENTRY_SIZE;
    if (dir_end > P.total || !pack_crc_ok(le32(h + 12))) return 0;
    memset(&P.info, 0, sizeof P.info);
    memcpy(P.info.name, h + 24, PACK_NAME_LEN);
    memcpy(P.info.author, h + 44, PACK_AUTHOR_LEN);
    P.info.port = h[18];
    P.info.flags = h[19];
    P.info.total_size = P.total;
    for (i = 0; i < P.n; i++) {
        if (!get_ent(i, &e)) return 0;
        if ((i && e.uid <= last) || e.uid < PACK_UID_BASE || ((e.uid - PACK_UID_BASE) >> 16) != e.kind) return 0;
        last = e.uid;
        if (e.kind >= PK_KIND_COUNT || e.codec >= PK_CODEC_COUNT || e.size == 0) return 0;
        end = e.off + e.size;
        if ((e.off & 3u) || e.off < dir_end || end < e.off || end > P.total) return 0;
        if (e.size > PACK_ENTRY_CAP_PS1 && THIS_PORT == PK_PORT_PS1) return 0;
        if (e.kind == PK_KIND_CG && e.arg >= MSLOT_MAX) return 0;
        if (e.kind == PK_KIND_CG) P.info.n_cg++;
        if (e.kind == PK_KIND_PORTRAIT) P.info.n_portrait++;
        if (e.kind == PK_KIND_FIGURE) P.info.n_figure++;
        if (e.kind == PK_KIND_SCRIPT) {
            uint8_t t[16];
            int bit = (int)e.arg - TXT_PACK_USER_FIRST;
            if ((e.uid & 0xFFFFu) >= MSLOT_MAX || bit < 0 || bit >= (int)TXT_PACK_USER_COUNT) return 0;
            if ((used >> bit) & 1u || e.codec != PK_CODEC_RAW || e.size < sizeof t) return 0;
            used |= (uint16_t)(1u << bit);
            if (!rd(e.off, t, sizeof t) || t[0] != 'T' || t[1] != 'X' || t[2] != 1 || t[3] != e.arg ||
                !(le16(t + 14) & TXF_RAW)) return 0;
            P.info.n_script++;
        }
    }
    /* addressable pack: hand every script to the text engine in place */
    if (P.blob) {
        for (i = 0; i < P.n; i++) {
            get_ent(i, &e);
            if (e.kind != PK_KIND_SCRIPT) continue;
            if (!txt_user_set(e.arg, P.blob + e.off)) return 0;
            P.user_set |= (uint16_t)(1u << (e.arg - TXT_PACK_USER_FIRST));
        }
    }
    return 1;
}

int pack_open_image(const uint8_t *blob, uint32_t size)
{
    pack_close();
    if (!blob || size < PACK_HEADER_SIZE) return 0;
    P.blob = blob;
    P.total = size;
    if (!check_and_register()) { pack_close(); return 0; }
    P.open = 1;
    return 1;
}

int pack_open(void)
{
    uint32_t size = 0;
    const uint8_t *b;
    uint8_t h[PACK_HEADER_SIZE];
    if (P.open) return 1;
    b = plat_pack_blob(&size);
    if (b) return pack_open_image(b, size);
    pack_close();
    if (plat_pack_read(0, h, sizeof h) != sizeof h) return 0;
    P.total = le32(h + 8);                     /* checked against the caps and the file below */
    if (P.total < PACK_HEADER_SIZE || P.total > PORT_CAP || !check_and_register()) { pack_close(); return 0; }
    P.open = 1;
    return 1;
}

int pack_info(DoomoPackInfo *out)
{
    if (out) {
        if (P.open) *out = P.info;
        else memset(out, 0, sizeof *out);
    }
    return P.open;
}

static int find(int kind, uint32_t uid, Ent *e)
{
    uint32_t lo = 0, hi = P.n;
    if (!P.open) return 0;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (!get_ent(mid, e)) return 0;
        if (e->uid == uid) return e->kind == kind;
        if (e->uid < uid) lo = mid + 1; else hi = mid;
    }
    return 0;
}

const uint8_t *pack_art(int kind, uint32_t uid, uint32_t *size)
{
    Ent e;
    if (size) *size = 0;
    if (!find(kind, uid, &e) || e.codec != PK_CODEC_RAW || !P.blob) return NULL;
    if (size) *size = e.size;
    return P.blob + e.off;
}

/* a forward-only byte cursor over [off, end) of the pack */
typedef struct { uint32_t off, end; uint8_t win[64]; uint32_t pos, len; } Cur;

static int cur_byte(Cur *c)
{
    if (c->pos >= c->len) {
        uint32_t n = c->end - c->off < sizeof c->win ? c->end - c->off : (uint32_t)sizeof c->win;
        if (!n || !rd(c->off, c->win, n)) return -1;
        c->off += n; c->pos = 0; c->len = n;
    }
    return c->win[c->pos++];
}

/* the core's save codec (save.c): c < 0x80: c+1 literals; else copy (c&127)+3 from u16 distance */
static uint32_t lz_to(Cur *c, uint8_t *dst, uint32_t cap)
{
    uint32_t o = 0;
    int b;
    while ((b = cur_byte(c)) >= 0) {
        if (b < 0x80) {
            uint32_t n = (uint32_t)b + 1;
            if (o + n > cap) return 0;
            while (n--) { int v = cur_byte(c); if (v < 0) return 0; dst[o++] = (uint8_t)v; }
        } else {
            uint32_t n = (uint32_t)(b & 0x7F) + 3, d;
            int lo = cur_byte(c), hi = cur_byte(c);
            if (lo < 0 || hi < 0) return 0;
            d = (uint32_t)lo | (uint32_t)hi << 8;
            if (d < 1 || d > o || o + n > cap) return 0;
            while (n--) { dst[o] = dst[o - d]; o++; }
        }
    }
    return o;
}

uint32_t pack_art_read(int kind, uint32_t uid, void *dst, uint32_t cap)
{
    Ent e;
    Cur c;
    if (!dst || !find(kind, uid, &e)) return 0;
    if (e.codec == PK_CODEC_RAW) return e.size <= cap && rd(e.off, dst, e.size) ? e.size : 0;
    memset(&c, 0, sizeof c);
    c.off = e.off; c.end = e.off + e.size;
    return lz_to(&c, (uint8_t *)dst, cap);
}

int pack_script(int slot)
{
    Ent e;
    if (slot < 0 || slot >= MSLOT_MAX || !find(PK_KIND_SCRIPT, PACK_UID_SCRIPT(slot), &e)) return 0;
    return (P.user_set >> (e.arg - TXT_PACK_USER_FIRST)) & 1u ? e.arg : 0;
}

int pack_script_bind(int slot, void *buf, uint32_t cap)
{
    Ent e;
    int bit;
    if (slot < 0 || slot >= MSLOT_MAX || !buf || !find(PK_KIND_SCRIPT, PACK_UID_SCRIPT(slot), &e)) return 0;
    if (e.size > cap || !rd(e.off, buf, e.size)) return 0;
    bit = (int)e.arg - TXT_PACK_USER_FIRST;
    if (!txt_user_set(e.arg, (const uint8_t *)buf)) return 0;
    P.user_set |= (uint16_t)(1u << bit);
    return e.arg;
}
