/* talk_db.c - the Talk database reader (owner: TK, T2; the format is in
 * core/include/doomo/talk.h, the compiler core/tools/talkc.py).
 *
 * One TALK_<lang>.BIN is open at a time, read a few bytes per call through
 * plat_talk_read() (or a memory image: the unit tests' fixture): a concept
 * row is 8 B, an association row 16 B, a surface form one short Huffman
 * string decoded by txt.c's txt_decode with the story's own code. Nothing
 * of the database is resident (the PS1 keeps it in SPU RAM, TP1).
 *
 * RAM: the directory (14 banks x 16 B) and a few words, about 250 B of
 * .bss; nothing on the GBA, which never references it (--gc-sections).
 * Per call: at most 128 B of stack for a string's stream.
 */
#include "doomo/doomo.h"
#include "doomo/talk.h"
#include "doomo/txt.h"
#include "doomo/plat_ext.h"
#include "doomo/lang.h"

typedef struct TalkBank {
    uint32_t off, packed, raw, crc;
    uint8_t  kind, flags, present, pad;
} TalkBank;

static struct {
    const uint8_t *mem;            /* a memory image, or NULL: the platform */
    uint32_t size;                 /* the image's / file's size              */
    uint16_t nconcepts, ncats;
    uint8_t  open, lang;
    TalkBank bank[TB_COUNT];
} db;

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* len bytes at file offset off: 1 ok */
static int rd(uint32_t off, void *dst, uint32_t len)
{
    if (len == 0) return 1;
    if (off > db.size || len > db.size - off) return 0;
    if (db.mem) { memcpy(dst, db.mem + off, len); return 1; }
    return plat_talk_read(off, dst, len) == len;
}

/* len bytes at offset `at` of bank `id`: 1 ok */
static int rd_bank(int id, uint32_t at, void *dst, uint32_t len)
{
    const TalkBank *b;
    if (!db.open || id <= 0 || id >= TB_COUNT || !db.bank[id].present) return 0;
    b = &db.bank[id];
    if (at > b->packed || len > b->packed - at) return 0;
    return rd(b->off + at, dst, len);
}

uint32_t talk_crc32(uint32_t crc, const uint8_t *p, uint32_t len)
{
    static const uint32_t nib[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u,
        0x4DB26158u, 0x5005713Cu, 0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
        0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu };
    crc = ~crc;
    while (len--) {
        crc ^= *p++;
        crc = (crc >> 4) ^ nib[crc & 15];
        crc = (crc >> 4) ^ nib[crc & 15];
    }
    return ~crc;
}

void talk_db_close(void)
{
    memset(&db, 0, sizeof(db));
}

int talk_db_is_open(void) { return db.open; }
int talk_db_nconcepts(void) { return db.open ? db.nconcepts : 0; }
int talk_db_lang(void) { return db.open ? db.lang : -1; }
int talk_db_ncats(void) { return db.open ? db.ncats : 0; }

static int parse(void)
{
    uint8_t h[TALK_HDR_SIZE], e[TALK_DIR_ENTRY], c4[4];
    unsigned i, nb;
    static const uint8_t need[] = { TB_CONCEPT, TB_ASSOC, TB_ATTR, TB_FORMS, TB_CATS, TB_CATNAMES };
    if (db.size < TALK_HDR_SIZE || !rd(0, h, TALK_HDR_SIZE)) return TALK_DB_BAD_DIR;
    if (le32(h) != TALK_MAGIC) return TALK_DB_BAD_MAGIC;
    if (le16(h + 4) != TALK_VERSION) return TALK_DB_BAD_VERSION;
    if (le32(h + 8) != txt_codec_hash()) return TALK_DB_DICT_MISMATCH;
    nb = le16(h + 12);
    db.nconcepts = le16(h + 14);
    db.lang = (uint8_t)le16(h + 6);
    if (le32(h + 16) != db.size || db.nconcepts == 0 || db.nconcepts > TALK_MAX_CONCEPTS ||
        nb == 0 || nb >= TB_COUNT || TALK_HDR_SIZE + nb * TALK_DIR_ENTRY > db.size)
        return TALK_DB_BAD_DIR;
    for (i = 0; i < nb; i++) {
        unsigned id;
        TalkBank *b;
        if (!rd(TALK_HDR_SIZE + i * TALK_DIR_ENTRY, e, TALK_DIR_ENTRY)) return TALK_DB_BAD_DIR;
        id = le16(e);
        if (id == 0 || id >= TB_COUNT || db.bank[id].present) return TALK_DB_BAD_DIR;
        b = &db.bank[id];
        b->kind = e[2];
        b->flags = e[3];
        b->off = le32(e + 4);
        b->packed = le32(e + 8);
        b->raw = le32(e + 12);
        b->crc = le32(e + 16);
        if (b->off < TALK_HDR_SIZE + nb * TALK_DIR_ENTRY || b->off > db.size || b->packed > db.size - b->off ||
            b->kind > TBK_TXT)
            return TALK_DB_BAD_DIR;
        b->present = 1;
    }
    for (i = 0; i < sizeof(need); i++)
        if (!db.bank[need[i]].present) return TALK_DB_BAD_DIR;
    /* the fixed-size banks must hold every concept */
    if (db.bank[TB_CONCEPT].packed < 8u * (db.nconcepts + 1u) ||
        db.bank[TB_ASSOC].packed < 16u * (db.nconcepts + 1u) ||
        db.bank[TB_FORMS].kind != TBK_TXT || db.bank[TB_CATNAMES].kind != TBK_TXT)
        return TALK_DB_BAD_DIR;
    db.open = 1;
    if (!rd_bank(TB_CATS, 0, c4, 4)) { db.open = 0; return TALK_DB_BAD_DIR; }
    db.ncats = le16(c4);
    if (4u + 4u * (db.ncats + 1u) > db.bank[TB_CATS].packed) { db.open = 0; return TALK_DB_BAD_DIR; }
    return TALK_DB_OK;
}

static int finish(int r)
{
    if (r != TALK_DB_OK) talk_db_close();
    return r;
}

int talk_db_open_mem(const uint8_t *image, uint32_t size)
{
    talk_db_close();
    if (!image) return TALK_DB_NONE;
    db.mem = image;
    db.size = size;
    return finish(parse());
}

int talk_db_open(int lang)
{
    uint32_t size;
    talk_db_close();
    if (lang < 0 || lang >= LANG_COUNT) return TALK_DB_NONE;
    size = plat_talk_load(doomo_lang_info[lang].code);
    if (size == PLAT_TALK_LOADING) return TALK_DB_LOADING;
    if (size == 0) return TALK_DB_NONE;
    db.size = size;
    return finish(parse());
}

int talk_db_verify(void)
{
    uint8_t buf[64];
    int id;
    if (!db.open) return TALK_DB_NONE;
    for (id = 1; id < TB_COUNT; id++) {
        const TalkBank *b = &db.bank[id];
        uint32_t at = 0, crc = 0;
        if (!b->present) continue;
        while (at < b->packed) {
            uint32_t n = b->packed - at > sizeof(buf) ? (uint32_t)sizeof(buf) : b->packed - at;
            if (!rd(b->off + at, buf, n)) return TALK_DB_BAD_DIR;
            crc = talk_crc32(crc, buf, n);
            at += n;
        }
        if (crc != b->crc) return TALK_DB_BAD_CRC;
    }
    return TALK_DB_OK;
}

int talk_db_bank(int id, uint32_t *offset, uint32_t *packed, uint32_t *raw)
{
    if (!db.open || id <= 0 || id >= TB_COUNT || !db.bank[id].present) return 0;
    if (offset) *offset = db.bank[id].off;
    if (packed) *packed = db.bank[id].packed;
    if (raw) *raw = db.bank[id].raw;
    return 1;
}

int talk_db_concept(int id, TalkConcept *out)
{
    uint8_t r[8];
    if (!out || id <= 0 || id > talk_db_nconcepts() || !rd_bank(TB_CONCEPT, 8u * (uint32_t)id, r, 8)) return 0;
    out->id = (uint16_t)id;
    out->cat = r[0];
    out->sent = (int8_t)((r[1] & 7) - 3);
    out->reg = (uint8_t)((r[1] >> 3) & 7);
    out->age = (uint8_t)(r[1] >> 6);
    out->attr_first = le16(r + 2);
    out->form = le16(r + 4);
    out->flags = le16(r + 6);
    return out->cat != 0;                      /* 0: a retired id */
}

int talk_db_assoc(int id, uint16_t *ids, uint8_t *weights)
{
    uint8_t r[16];
    int i, n = 0;
    if (id <= 0 || id > talk_db_nconcepts() || !rd_bank(TB_ASSOC, 16u * (uint32_t)id, r, 16)) return 0;
    for (i = 0; i < TALK_ASSOC_TOP; i++) {
        uint16_t v = le16(r + 2 * i);
        if (!v) break;
        if (ids) ids[n] = (uint16_t)(v & 0x0FFFu);
        if (weights) weights[n] = (uint8_t)(v >> 12);
        n++;
    }
    return n;
}

int talk_db_edges(int id, uint16_t *ids, uint8_t *weights, int cap)
{
    uint8_t r[4];
    uint32_t first, end, k;
    int n = 0;
    if (id <= 0 || id > talk_db_nconcepts() || cap <= 0 ||
        !rd_bank(TB_EDGES, 2u * (uint32_t)id, r, 4)) return 0;
    first = le16(r);
    end = le16(r + 2);
    for (k = first; k < end && n < cap; k++) {
        uint8_t e[2];
        uint16_t v;
        if (!rd_bank(TB_EDGES, 2u * (db.nconcepts + 2u) + 2u * k, e, 2)) break;
        v = le16(e);
        if (ids) ids[n] = (uint16_t)(v & 0x0FFFu);
        if (weights) weights[n] = (uint8_t)(v >> 12);
        n++;
    }
    return n;
}

int talk_db_attrs(int id, uint16_t *ids)
{
    TalkConcept c;
    int i, n;
    if (!talk_db_concept(id, &c)) return 0;
    n = c.flags & TCF_NATTR_MASK;
    if (n > TALK_ATTR_MAX) n = TALK_ATTR_MAX;
    for (i = 0; i < n; i++) {
        uint8_t e[2];
        if (!rd_bank(TB_ATTR, 2u * (c.attr_first + (uint32_t)i), e, 2)) return i;
        if (ids) ids[i] = le16(e);
    }
    return n;
}

/* string `idx` of TBK_TXT bank `id` into out */
static int bank_string(int id, unsigned idx, char *out, int cap)
{
    uint8_t h[8], s[128];
    uint32_t nblocks, base, off, at, n;
    unsigned shift;
    out[0] = 0;
    if (!rd_bank(id, 0, h, 8) || idx >= le16(h)) return -1;
    shift = h[2];
    nblocks = le32(h + 4);
    if (shift > 15 || (idx >> shift) >= nblocks) return -1;
    if (!rd_bank(id, 8u + 4u * (idx >> shift), s, 4)) return -1;
    base = le32(s);
    if (!rd_bank(id, 8u + 4u * nblocks + 2u * idx, s, 2)) return -1;
    off = le16(s);
    at = base + off;
    if (at >= db.bank[id].packed) return -1;
    n = db.bank[id].packed - at;
    if (n > sizeof(s)) n = sizeof(s);
    if (!rd_bank(id, at, s, n)) return -1;
    return txt_decode(s, n, out, cap);
}

static int put(char *out, int cap, int n, const char *src)
{
    while (*src && n < cap - 1) out[n++] = *src++;
    out[n] = 0;
    return n;
}

int talk_db_form(int id, int which, char *out, int cap)
{
    TalkConcept c;
    char f[96];
    char *plural;
    const char *word;
    int n = 0;
    if (!out || cap <= 0) return -1;
    out[0] = 0;
    if (!talk_db_concept(id, &c) || bank_string(TB_FORMS, c.form, f, sizeof(f)) < 0) return -1;
    plural = strchr(f, '/');
    if (plural) *plural++ = 0;
    switch (which) {
    case TF_LEMMA:  return put(out, cap, 0, f);
    case TF_PLURAL: return put(out, cap, 0, plural ? plural : f);
    case TF_DEFAULT: return put(out, cap, 0, (c.flags & TCF_PLURAL) && plural ? plural : f);
    case TF_INDEF:
    case TF_DEF:
        word = (c.flags & TCF_PLURAL) && plural ? plural : f;
        if (c.flags & TCF_PROPER) {
            if (c.flags & TCF_THE) n = put(out, cap, 0, "the ");
            return put(out, cap, n, f);
        }
        if (which == TF_DEF || (c.flags & TCF_THE)) n = put(out, cap, 0, "the ");
        else if ((c.flags & (TCF_MASS | TCF_PLURAL)) || !(c.flags & TCF_COUNTABLE)) n = put(out, cap, 0, "some ");
        else n = put(out, cap, 0, (c.flags & TCF_AN) ? "an " : "a ");
        return put(out, cap, n, word);
    default:
        return -1;
    }
}

int talk_db_cat(int cat, uint8_t *parent, uint8_t *flags)
{
    uint8_t r[4];
    if (cat <= 0 || cat > talk_db_ncats() || !rd_bank(TB_CATS, 4u + 4u * (uint32_t)cat, r, 4)) return 0;
    if (parent) *parent = r[0];
    if (flags) *flags = r[1];
    return 1;
}

int talk_db_cat_name(int cat, char *out, int cap)
{
    uint8_t r[4];
    if (!out || cap <= 0) return -1;
    out[0] = 0;
    if (cat <= 0 || cat > talk_db_ncats() || !rd_bank(TB_CATS, 4u + 4u * (uint32_t)cat, r, 4)) return -1;
    return bank_string(TB_CATNAMES, le16(r + 2), out, cap);
}

int talk_db_cat_root(int cat)
{
    uint8_t parent = 0;
    if (!talk_db_cat(cat, &parent, NULL)) return TCR_NONE;
    return parent ? parent : cat;
}
