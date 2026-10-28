/* save.c - versioned, checksummed save codec (owner: foundation).
 *
 * File layout (all multi-byte values little-endian):
 *   header, 48 bytes, uncompressed
 *     0 u32 magic "DMOO"       4 u16 version        6 u16 header size (48)
 *     8 u32 total file size    12 u32 CRC-32 of bytes [16, total)
 *    16 u32 raw stream size    20 u8 mode, chapter, population, weekday
 *    24 u16 residents, minute  28 u32 day          32 u32 money
 *    36 char player name[12]
 *   body: an LZ-compressed bit stream (below)
 *
 * Bit stream (LSB-first; byte-aligned u8/u16/u32 are therefore LE):
 *   town scalars, inventory, weapons, pet, story, options, unlocks,
 *   progress counters, graves, the relationship graph (sorted, delta
 *   coded varints), the player, then the 1000 residents:
 *     2-bit tag: 0 = run of empty slots, 1 = run of slots identical to
 *     their base record (10-bit run length - 1 follows), 2 = delta vs base
 *     (group mask + field masks + changed fields), 3 = full record.
 *   Every resident field is bit-packed with a fixed width (see rfields[]),
 *   so a fully custom citizen costs ~57 bytes and an untouched premade
 *   citizen a fraction of a bit.
 *   The base of slot i is its premade roster record on a Hellvile Park
 *   Classic island, its seeded random record on a Random Island, and
 *   nothing (full records only) on an Empty Island.
 *
 * Compression: LZ77 with a 4 KB window. Control byte c < 0x80: c+1
 * literal bytes follow; c >= 0x80: copy (c & 0x7F) + 3 bytes from
 * distance d (u16 LE that follows). Encoder and decoder both stream.
 *
 * Loading verifies magic, version and CRC, then decodes twice: a dry run
 * that validates the whole stream, and only then the real decode into
 * doomo_town (which is therefore never left half-loaded).
 *
 * Extension chunks (save_ext.h) follow the town data inside the same
 * compressed body: modules only register a DoomoSaveExt (see write_exts).
 *
 * Memory: the codec has no static buffers. Its LZ window, match heads and
 * the staging area for extension chunks are the last DOOMO_SAVE_WORK_SIZE
 * bytes of plat_save_scratch() (save.h); the front part holds the image
 * or the stream window. When the front offers room for the whole slot, the
 * image is built / read there (one encode pass). Otherwise (the
 * GBA: EWRAM is too precious for an 88 KB buffer) the save is STREAMED
 * through the platform (platform.h, plat_save_stream_* / plat_save_read_at)
 * in chunks of the scratch buffer:
 *   saving   pass 1 encodes into nothing, counting the size and the body's
 *            CRC; the header is then complete (its CRC is combined from
 *            the header's and the body's with crc32_combine); pass 2
 *            encodes again (the same bytes) and streams header + body.
 *   loading  one pass checks the CRC, then the two decodes pull the body
 *            through a window that is refilled with plat_save_read_at.
 * Either way the bytes on the medium are identical.
 */
#include "doomo/doomo.h"

/* Every enum must fit the bit width the format gives it. Growing past a
 * width needs a new DOOMO_SAVE_VERSION. */
DOOMO_STATIC_ASSERT(SPECIES_COUNT <= 64, sv_species);
DOOMO_STATIC_ASSERT(BODY_COUNT <= 32, sv_body);
DOOMO_STATIC_ASSERT(HEAD_COUNT <= 32, sv_head);
DOOMO_STATIC_ASSERT(EYE_COUNT <= 64, sv_eye);
DOOMO_STATIC_ASSERT(BROW_COUNT <= 32, sv_brow);
DOOMO_STATIC_ASSERT(NOSE_COUNT <= 32, sv_nose);
DOOMO_STATIC_ASSERT(MOUTH_COUNT <= 64, sv_mouth);
DOOMO_STATIC_ASSERT(HAIR_COUNT <= 64, sv_hair);
DOOMO_STATIC_ASSERT(FHAIR_COUNT <= 16, sv_fhair);
DOOMO_STATIC_ASSERT(EARS_COUNT <= 32, sv_ears);
DOOMO_STATIC_ASSERT(HORNS_COUNT <= 32, sv_horns);
DOOMO_STATIC_ASSERT(TAIL_COUNT <= 32, sv_tail);
DOOMO_STATIC_ASSERT(WINGS_COUNT <= 32, sv_wings);
DOOMO_STATIC_ASSERT(HALO_COUNT <= 16, sv_halo);
DOOMO_STATIC_ASSERT(MARK_COUNT <= 64, sv_mark);
DOOMO_STATIC_ASSERT(TOP_COUNT <= 64, sv_top);
DOOMO_STATIC_ASSERT(BOTTOM_COUNT <= 32, sv_bottom);
DOOMO_STATIC_ASSERT(SHOES_COUNT <= 32, sv_shoes);
DOOMO_STATIC_ASSERT(HAT_COUNT <= 64, sv_hat);
DOOMO_STATIC_ASSERT(ACC_COUNT <= 64, sv_acc);
DOOMO_STATIC_ASSERT(AURA_COUNT <= 16, sv_aura);
DOOMO_STATIC_ASSERT(PATTERN_COUNT <= 16, sv_pattern);
DOOMO_STATIC_ASSERT(PRONOUN_COUNT <= 8, sv_pronoun);
DOOMO_STATIC_ASSERT(ROMANCE_COUNT <= 8, sv_romance);
DOOMO_STATIC_ASSERT(FOOD_COUNT <= 64, sv_food);
DOOMO_STATIC_ASSERT(HOBBY_COUNT <= 64, sv_hobby);
DOOMO_STATIC_ASSERT(MUSIC_COUNT <= 32, sv_music);
DOOMO_STATIC_ASSERT(FEAR_COUNT <= 32, sv_fear);
DOOMO_STATIC_ASSERT(CATCHPHRASE_COUNT <= 128, sv_catch);
DOOMO_STATIC_ASSERT(MOOD_COUNT <= 32, sv_mood);
DOOMO_STATIC_ASSERT(ACT_COUNT <= 32, sv_act);
DOOMO_STATIC_ASSERT(DEATH_COUNT <= 16, sv_death);
DOOMO_STATIC_ASSERT(DOOMO_MAX_EDGES < 65536 && DOOMO_MAX_RESIDENTS <= 1024, sv_sizes);

#define HDR DOOMO_SAVE_HEADER_SIZE

/* ====================================================================== */
/* CRC-32 (IEEE, reflected), nibble table                                   */
/* byte-wise table (1 KB, ROM): one lookup per byte */
static const uint32_t crc_tab[256] = {
    0x00000000u, 0x77073096u, 0xEE0E612Cu, 0x990951BAu, 0x076DC419u, 0x706AF48Fu,
    0xE963A535u, 0x9E6495A3u, 0x0EDB8832u, 0x79DCB8A4u, 0xE0D5E91Eu, 0x97D2D988u,
    0x09B64C2Bu, 0x7EB17CBDu, 0xE7B82D07u, 0x90BF1D91u, 0x1DB71064u, 0x6AB020F2u,
    0xF3B97148u, 0x84BE41DEu, 0x1ADAD47Du, 0x6DDDE4EBu, 0xF4D4B551u, 0x83D385C7u,
    0x136C9856u, 0x646BA8C0u, 0xFD62F97Au, 0x8A65C9ECu, 0x14015C4Fu, 0x63066CD9u,
    0xFA0F3D63u, 0x8D080DF5u, 0x3B6E20C8u, 0x4C69105Eu, 0xD56041E4u, 0xA2677172u,
    0x3C03E4D1u, 0x4B04D447u, 0xD20D85FDu, 0xA50AB56Bu, 0x35B5A8FAu, 0x42B2986Cu,
    0xDBBBC9D6u, 0xACBCF940u, 0x32D86CE3u, 0x45DF5C75u, 0xDCD60DCFu, 0xABD13D59u,
    0x26D930ACu, 0x51DE003Au, 0xC8D75180u, 0xBFD06116u, 0x21B4F4B5u, 0x56B3C423u,
    0xCFBA9599u, 0xB8BDA50Fu, 0x2802B89Eu, 0x5F058808u, 0xC60CD9B2u, 0xB10BE924u,
    0x2F6F7C87u, 0x58684C11u, 0xC1611DABu, 0xB6662D3Du, 0x76DC4190u, 0x01DB7106u,
    0x98D220BCu, 0xEFD5102Au, 0x71B18589u, 0x06B6B51Fu, 0x9FBFE4A5u, 0xE8B8D433u,
    0x7807C9A2u, 0x0F00F934u, 0x9609A88Eu, 0xE10E9818u, 0x7F6A0DBBu, 0x086D3D2Du,
    0x91646C97u, 0xE6635C01u, 0x6B6B51F4u, 0x1C6C6162u, 0x856530D8u, 0xF262004Eu,
    0x6C0695EDu, 0x1B01A57Bu, 0x8208F4C1u, 0xF50FC457u, 0x65B0D9C6u, 0x12B7E950u,
    0x8BBEB8EAu, 0xFCB9887Cu, 0x62DD1DDFu, 0x15DA2D49u, 0x8CD37CF3u, 0xFBD44C65u,
    0x4DB26158u, 0x3AB551CEu, 0xA3BC0074u, 0xD4BB30E2u, 0x4ADFA541u, 0x3DD895D7u,
    0xA4D1C46Du, 0xD3D6F4FBu, 0x4369E96Au, 0x346ED9FCu, 0xAD678846u, 0xDA60B8D0u,
    0x44042D73u, 0x33031DE5u, 0xAA0A4C5Fu, 0xDD0D7CC9u, 0x5005713Cu, 0x270241AAu,
    0xBE0B1010u, 0xC90C2086u, 0x5768B525u, 0x206F85B3u, 0xB966D409u, 0xCE61E49Fu,
    0x5EDEF90Eu, 0x29D9C998u, 0xB0D09822u, 0xC7D7A8B4u, 0x59B33D17u, 0x2EB40D81u,
    0xB7BD5C3Bu, 0xC0BA6CADu, 0xEDB88320u, 0x9ABFB3B6u, 0x03B6E20Cu, 0x74B1D29Au,
    0xEAD54739u, 0x9DD277AFu, 0x04DB2615u, 0x73DC1683u, 0xE3630B12u, 0x94643B84u,
    0x0D6D6A3Eu, 0x7A6A5AA8u, 0xE40ECF0Bu, 0x9309FF9Du, 0x0A00AE27u, 0x7D079EB1u,
    0xF00F9344u, 0x8708A3D2u, 0x1E01F268u, 0x6906C2FEu, 0xF762575Du, 0x806567CBu,
    0x196C3671u, 0x6E6B06E7u, 0xFED41B76u, 0x89D32BE0u, 0x10DA7A5Au, 0x67DD4ACCu,
    0xF9B9DF6Fu, 0x8EBEEFF9u, 0x17B7BE43u, 0x60B08ED5u, 0xD6D6A3E8u, 0xA1D1937Eu,
    0x38D8C2C4u, 0x4FDFF252u, 0xD1BB67F1u, 0xA6BC5767u, 0x3FB506DDu, 0x48B2364Bu,
    0xD80D2BDAu, 0xAF0A1B4Cu, 0x36034AF6u, 0x41047A60u, 0xDF60EFC3u, 0xA867DF55u,
    0x316E8EEFu, 0x4669BE79u, 0xCB61B38Cu, 0xBC66831Au, 0x256FD2A0u, 0x5268E236u,
    0xCC0C7795u, 0xBB0B4703u, 0x220216B9u, 0x5505262Fu, 0xC5BA3BBEu, 0xB2BD0B28u,
    0x2BB45A92u, 0x5CB36A04u, 0xC2D7FFA7u, 0xB5D0CF31u, 0x2CD99E8Bu, 0x5BDEAE1Du,
    0x9B64C2B0u, 0xEC63F226u, 0x756AA39Cu, 0x026D930Au, 0x9C0906A9u, 0xEB0E363Fu,
    0x72076785u, 0x05005713u, 0x95BF4A82u, 0xE2B87A14u, 0x7BB12BAEu, 0x0CB61B38u,
    0x92D28E9Bu, 0xE5D5BE0Du, 0x7CDCEFB7u, 0x0BDBDF21u, 0x86D3D2D4u, 0xF1D4E242u,
    0x68DDB3F8u, 0x1FDA836Eu, 0x81BE16CDu, 0xF6B9265Bu, 0x6FB077E1u, 0x18B74777u,
    0x88085AE6u, 0xFF0F6A70u, 0x66063BCAu, 0x11010B5Cu, 0x8F659EFFu, 0xF862AE69u,
    0x616BFFD3u, 0x166CCF45u, 0xA00AE278u, 0xD70DD2EEu, 0x4E048354u, 0x3903B3C2u,
    0xA7672661u, 0xD06016F7u, 0x4969474Du, 0x3E6E77DBu, 0xAED16A4Au, 0xD9D65ADCu,
    0x40DF0B66u, 0x37D83BF0u, 0xA9BCAE53u, 0xDEBB9EC5u, 0x47B2CF7Fu, 0x30B5FFE9u,
    0xBDBDF21Cu, 0xCABAC28Au, 0x53B39330u, 0x24B4A3A6u, 0xBAD03605u, 0xCDD70693u,
    0x54DE5729u, 0x23D967BFu, 0xB3667A2Eu, 0xC4614AB8u, 0x5D681B02u, 0x2A6F2B94u,
    0xB40BBE37u, 0xC30C8EA1u, 0x5A05DF1Bu, 0x2D02EF8Du
};
#define CRC_BYTE(c, b) (((c) >> 8) ^ crc_tab[((c) ^ (b)) & 0xFFu])

/* running state: start at 0xFFFFFFFF, finish with ^ 0xFFFFFFFF */
static uint32_t crc_update(uint32_t c, const uint8_t *buf, uint32_t size)
{
    uint32_t i;
    for (i = 0; i < size; i++) c = CRC_BYTE(c, buf[i]);
    return c;
}

uint32_t save_crc32(const uint8_t *buf, uint32_t size)
{
    return crc_update(0xFFFFFFFFu, buf, size) ^ 0xFFFFFFFFu;
}

/* CRC of A followed by B from crc(A), crc(B) and len(B) (zlib's
 * crc32_combine: append len(B) zero bytes to A in GF(2), by squaring) */
static uint32_t gf2_times(const uint32_t *mat, uint32_t vec)
{
    uint32_t sum = 0;
    for (; vec; vec >>= 1, mat++)
        if (vec & 1) sum ^= *mat;
    return sum;
}

static void gf2_square(uint32_t *sq, const uint32_t *mat)
{
    int n;
    for (n = 0; n < 32; n++) sq[n] = gf2_times(mat, mat[n]);
}

static uint32_t crc32_combine(uint32_t crc1, uint32_t crc2, uint32_t len2)
{
    uint32_t even[32], odd[32], row = 1;
    int n;
    if (len2 == 0) return crc1;
    odd[0] = 0xEDB88320u;                   /* one zero bit */
    for (n = 1; n < 32; n++) { odd[n] = row; row <<= 1; }
    gf2_square(even, odd);                  /* two zero bits */
    gf2_square(odd, even);                  /* four */
    for (;;) {
        gf2_square(even, odd);
        if (len2 & 1) crc1 = gf2_times(even, crc1);
        len2 >>= 1;
        if (!len2) break;
        gf2_square(odd, even);
        if (len2 & 1) crc1 = gf2_times(odd, crc1);
        len2 >>= 1;
        if (!len2) break;
    }
    return crc1 ^ crc2;
}

/* ====================================================================== */
/* LZ codec                                                                 */
#define LZ_RING 4096u
#define LZ_MASK (LZ_RING - 1u)
#define LZ_MAXOFF 3840u
#define LZ_MAXLEN 130u
#define LZ_HASH 1024u

/* The codec's hot scalar state in one struct: THUMB code from the GBA
 * cartridge reaches it through one base address instead of one literal
 * per variable (each global costs a ROM literal load + an EWRAM access). */
static struct {
    /* LZ encoder */
    uint32_t lz_nlit, lz_in, lz_proc;
    /* output sink */
    uint8_t *sk_buf;
    uint32_t sk_cap, sk_n, sk_fill, sk_crc;
    int sk_mode, sk_err;
    /* bit writer / reader */
    uint32_t bw_acc, br_acc;
    int bw_n, br_n;
    /* decoder */
    const uint8_t *ld_src;
    uint8_t *ld_wbuf;
    uint32_t ld_n, ld_pos, ld_out, ld_lit, ld_match, ld_dist;
    uint32_t ld_wbase, ld_wlen, ld_wcap;
    int ld_err, ld_slot;
    /* the work area (save.h: the tail of plat_save_scratch(), bound by
     * work_bind() before every encode / decode) */
    uint8_t *ring;          /* LZ_RING bytes: the LZ window              */
    uint16_t *head;         /* LZ_HASH match heads                       */
    uint8_t *lit;           /* 128: the pending literal run              */
    uint8_t *stage;         /* DOOMO_SAVE_EXT_MAX: one extension chunk   */
    int ext_err;
} Z = { 0, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, NULL, NULL, NULL, NULL, 0 };

#define lz_ring Z.ring
#define lz_head Z.head
#define lz_lit  Z.lit
DOOMO_STATIC_ASSERT(LZ_RING + LZ_HASH * 2u + 128u <= DOOMO_SAVE_CODEC_SIZE, sv_codec_fits);

static void work_bind(uint8_t *w)
{
    Z.ring = w;
    Z.head = (uint16_t *)(void *)(w + LZ_RING);
    Z.lit = w + LZ_RING + LZ_HASH * 2u;
    Z.stage = w + DOOMO_SAVE_CODEC_SIZE;
}

/* output sink. SK_MEM: the image in memory (counts past its capacity so
 * callers learn the size); SK_COUNT: nothing kept, size + running CRC;
 * SK_STREAM: chunks of Z.sk_cap bytes go to plat_save_stream_write(). */
enum { SK_MEM, SK_COUNT, SK_STREAM };

static void sk_flush(void)
{
    if (Z.sk_fill && !Z.sk_err && !plat_save_stream_write(Z.sk_buf, Z.sk_fill)) Z.sk_err = 1;
    Z.sk_fill = 0;
}

static inline void sk_put(uint8_t b)
{
    if (Z.sk_mode == SK_MEM) {
        if (Z.sk_n < Z.sk_cap) Z.sk_buf[Z.sk_n] = b;
    } else {
        Z.sk_crc = CRC_BYTE(Z.sk_crc, b);
        if (Z.sk_mode == SK_STREAM) {
            Z.sk_buf[Z.sk_fill++] = b;
            if (Z.sk_fill == Z.sk_cap) sk_flush();
        }
    }
    Z.sk_n++;
}

/* n bytes at once (literal runs) */
static void sk_write(const uint8_t *p, uint32_t n)
{
    uint32_t i;
    if (Z.sk_mode == SK_MEM) {
        if (Z.sk_n < Z.sk_cap) memcpy(Z.sk_buf + Z.sk_n, p, Z.sk_cap - Z.sk_n < n ? Z.sk_cap - Z.sk_n : n);
        Z.sk_n += n;
        return;
    }
    Z.sk_crc = crc_update(Z.sk_crc, p, n);
    Z.sk_n += n;
    if (Z.sk_mode == SK_STREAM)
        for (i = 0; i < n; i++) {
            Z.sk_buf[Z.sk_fill++] = p[i];
            if (Z.sk_fill == Z.sk_cap) sk_flush();
        }
}

static void lz_reset(void)
{
    memset(lz_head, 0, LZ_HASH * sizeof(uint16_t));
    Z.lz_nlit = Z.lz_in = Z.lz_proc = 0;
}

static inline uint32_t lz_hash3(uint32_t p)
{
    uint32_t v = ((uint32_t)lz_ring[p & LZ_MASK] << 16) |
                 ((uint32_t)lz_ring[(p + 1) & LZ_MASK] << 8) | lz_ring[(p + 2) & LZ_MASK];
    return (v * 2654435761u) >> 22;
}

static void lz_flush_lits(void)
{
    if (!Z.lz_nlit) return;
    sk_put((uint8_t)(Z.lz_nlit - 1));
    sk_write(lz_lit, Z.lz_nlit);
    Z.lz_nlit = 0;
}

static void lz_step(void)
{
    uint32_t p = Z.lz_proc, avail = Z.lz_in - p, best = 0, boff = 0;
    if (avail >= 3) {
        uint32_t h = lz_hash3(p);
        uint16_t stored = lz_head[h];
        lz_head[h] = (uint16_t)(p + 1);
        if (stored) {
            uint32_t dist = (p - (uint32_t)(stored - 1)) & 0xFFFFu;
            if (dist >= 1 && dist <= LZ_MAXOFF && dist <= p) {
                uint32_t cand = p - dist, maxl = avail < LZ_MAXLEN ? avail : LZ_MAXLEN, l = 0;
                while (l < maxl && lz_ring[(cand + l) & LZ_MASK] == lz_ring[(p + l) & LZ_MASK]) l++;
                if (l >= 3) { best = l; boff = dist; }
            }
        }
    }
    if (best >= 3) {
        uint32_t k;
        lz_flush_lits();
        sk_put((uint8_t)(0x80 | (best - 3)));
        sk_put((uint8_t)(boff & 0xFF));
        sk_put((uint8_t)(boff >> 8));
        for (k = 1; k < best; k++)
            if (Z.lz_in - (p + k) >= 3) lz_head[lz_hash3(p + k)] = (uint16_t)(p + k + 1);
        Z.lz_proc += best;
    } else {
        lz_lit[Z.lz_nlit++] = lz_ring[p & LZ_MASK];
        if (Z.lz_nlit == 128) lz_flush_lits();
        Z.lz_proc++;
    }
}

static inline void lz_push(uint8_t b)
{
    lz_ring[Z.lz_in & LZ_MASK] = b;
    Z.lz_in++;
    while (Z.lz_in - Z.lz_proc >= LZ_MAXLEN) lz_step();
}

static void lz_finish(void)
{
    while (Z.lz_proc < Z.lz_in) lz_step();
    lz_flush_lits();
}

/* decoder (pull). The compressed body (Z.ld_n bytes) is read through a
 * window: body bytes [Z.ld_wbase, Z.ld_wbase + Z.ld_wlen) at Z.ld_src. In memory
 * the window is the whole body; when streaming it is refilled from slot
 * Z.ld_slot with plat_save_read_at(). */

static void ld_reset(const uint8_t *src, uint32_t n)
{
    Z.ld_src = src; Z.ld_n = n; Z.ld_pos = 0; Z.ld_out = 0;
    Z.ld_lit = Z.ld_match = Z.ld_dist = 0;
    Z.ld_err = 0;
    Z.ld_wbase = 0;
    Z.ld_wlen = src ? n : 0;
}

/* the window ran out: read the next piece of the body (streamed loads) */
static int ld_refill(void)
{
    uint32_t k = Z.ld_n - Z.ld_pos, size = 0;
    if (Z.ld_slot < 0 || !Z.ld_wbuf) return 0;
    if (k > Z.ld_wcap) k = Z.ld_wcap;
    if (!plat_save_read_at(Z.ld_slot, DOOMO_SAVE_HEADER_SIZE + Z.ld_pos, Z.ld_wbuf, k, &size))
        return 0;
    Z.ld_src = Z.ld_wbuf;
    Z.ld_wbase = Z.ld_pos;
    Z.ld_wlen = k;
    return 1;
}

static inline int ld_byte(uint8_t *b)
{
    if (Z.ld_pos >= Z.ld_n) return 0;
    if (Z.ld_pos - Z.ld_wbase >= Z.ld_wlen && !ld_refill()) return 0;
    *b = Z.ld_src[Z.ld_pos - Z.ld_wbase];
    Z.ld_pos++;
    return 1;
}

static uint8_t ld_get_slow(void)
{
    uint8_t b;
    if (Z.ld_err) return 0;
    if (!Z.ld_lit && !Z.ld_match) {
        uint8_t c, d0, d1;
        if (!ld_byte(&c)) { Z.ld_err = 1; return 0; }
        if (c < 0x80) {
            Z.ld_lit = (uint32_t)c + 1;
        } else {
            if (!ld_byte(&d0) || !ld_byte(&d1)) { Z.ld_err = 1; return 0; }
            Z.ld_match = (uint32_t)(c & 0x7F) + 3;
            Z.ld_dist = d0 | ((uint32_t)d1 << 8);
            if (Z.ld_dist == 0 || Z.ld_dist > Z.ld_out || Z.ld_dist > LZ_MAXOFF) { Z.ld_err = 1; return 0; }
        }
    }
    if (Z.ld_lit) {
        if (!ld_byte(&b)) { Z.ld_err = 1; return 0; }
        Z.ld_lit--;
    } else {
        b = lz_ring[(Z.ld_out - Z.ld_dist) & LZ_MASK];
        Z.ld_match--;
    }
    lz_ring[Z.ld_out & LZ_MASK] = b;
    Z.ld_out++;
    return b;
}

/* the common cases inline: inside a literal run (window permitting) or a
 * match; everything else (control bytes, refills, errors) is slow */
static inline uint8_t ld_get(void)
{
    uint8_t b;
    if (Z.ld_lit && Z.ld_pos < Z.ld_n && Z.ld_pos - Z.ld_wbase < Z.ld_wlen) {
        b = Z.ld_src[Z.ld_pos - Z.ld_wbase];
        Z.ld_pos++;
        Z.ld_lit--;
    } else if (Z.ld_match && !Z.ld_err) {
        b = lz_ring[(Z.ld_out - Z.ld_dist) & LZ_MASK];
        Z.ld_match--;
    } else {
        return ld_get_slow();
    }
    lz_ring[Z.ld_out & LZ_MASK] = b;
    Z.ld_out++;
    return b;
}

/* ====================================================================== */
/* bit stream                                                               */

static void wbits(uint32_t v, int n)
{
    if (n < 32) v &= (1u << n) - 1u;
    Z.bw_acc |= v << Z.bw_n;
    Z.bw_n += n;
    while (Z.bw_n >= 8) {
        lz_push((uint8_t)Z.bw_acc);
        Z.bw_acc >>= 8;
        Z.bw_n -= 8;
    }
}
static void walign(void)
{
    if (Z.bw_n) { lz_push((uint8_t)Z.bw_acc); Z.bw_acc = 0; Z.bw_n = 0; }
}
static void w8(uint32_t v) { wbits(v, 8); }
static void w16(uint32_t v) { wbits(v, 16); }
static void w32(uint32_t v) { wbits(v & 0xFFFFu, 16); wbits(v >> 16, 16); }
static void wraw(const uint8_t *p, int n) { int i; for (i = 0; i < n; i++) w8(p[i]); }
/* varint in 5-bit chunks: 4 data bits + continue bit */
static void wvar(uint32_t v)
{
    while (v >= 16) { wbits((v & 15) | 16, 5); v >>= 4; }
    wbits(v, 5);
}


static uint32_t rbits(int n)
{
    uint32_t v;
    while (Z.br_n < n) {
        Z.br_acc |= (uint32_t)ld_get() << Z.br_n;
        Z.br_n += 8;
    }
    v = n < 32 ? Z.br_acc & ((1u << n) - 1u) : Z.br_acc;
    Z.br_acc = n < 32 ? Z.br_acc >> n : 0;
    Z.br_n -= n;
    return v;
}
static void ralign(void) { Z.br_acc = 0; Z.br_n = 0; }
static uint32_t r8(void) { return rbits(8); }
static uint32_t r16(void) { return rbits(16); }
static uint32_t r32(void) { uint32_t lo = rbits(16); return lo | (rbits(16) << 16); }
static void rraw(uint8_t *p, int n) { int i; for (i = 0; i < n; i++) p[i] = (uint8_t)r8(); }
/* rraw into p, or (p == NULL: dry run) just consume the bytes */
static void rraw_or_skip(uint8_t *p, int n)
{
    int i;
    if (p) rraw(p, n);
    else for (i = 0; i < n; i++) r8();
}
static uint32_t rvar(void)
{
    uint32_t v = 0;
    int shift = 0;
    for (;;) {
        uint32_t c = rbits(5);
        v |= (c & 15) << shift;
        if (!(c & 16) || shift > 24) break;
        shift += 4;
    }
    return v;
}

/* ====================================================================== */
/* resident schema                                                          */
typedef struct { uint8_t off, bits; } RField;
#define LO(f) ((uint8_t)offsetof(DoomoResident, look.f))
#define RO(f) ((uint8_t)offsetof(DoomoResident, f))
#define RA(f, i) ((uint8_t)(offsetof(DoomoResident, f) + (i)))
#define NAME_FIELD 0     /* bits == 0 marks the name */

static const RField rfields[] = {
    { LO(species), 6 }, { LO(body), 5 }, { LO(height), 4 }, { LO(build), 4 },
    { LO(skin), 6 }, { LO(head_shape), 5 }, { LO(head_size), 4 },
    { LO(eye_style), 6 }, { LO(eye_color), 5 }, { LO(eye_size), 4 },
    { LO(eye_spacing), 4 }, { LO(eye_height), 4 }, { LO(eye_tilt), 4 },
    { LO(eye_count), 2 }, { LO(brow_style), 5 }, { LO(brow_height), 4 },
    { LO(nose_style), 5 }, { LO(nose_size), 4 }, { LO(mouth_style), 6 },
    { LO(mouth_size), 4 }, { LO(fangs), 2 }, { LO(hair_style), 6 },
    { LO(hair_color), 5 }, { LO(hair_highlight), 6 }, { LO(facial_hair), 4 },
    { LO(ears), 5 }, { LO(horns), 5 }, { LO(horn_color), 5 }, { LO(horn_size), 4 },
    { LO(tail), 5 }, { LO(wings), 5 }, { LO(halo), 4 }, { LO(marking), 6 },
    { LO(marking_color), 5 }, { LO(cheeks), 8 }, { LO(top), 6 },
    { LO(top_color), 5 }, { LO(bottom), 5 }, { LO(bottom_color), 5 },
    { LO(shoes), 5 }, { LO(shoes_color), 5 }, { LO(hat), 6 }, { LO(hat_color), 5 },
    { LO(accessory), 6 }, { LO(accessory_color), 5 }, { LO(aura), 4 },
    { LO(pattern), 4 }, { LO(pattern_color), 5 },
    { RO(name), NAME_FIELD },
    { RA(voice, 0), 5 }, { RA(voice, 1), 5 }, { RA(voice, 2), 5 }, { RA(voice, 3), 5 },
    { RA(voice, 4), 5 },
    { RA(persona, 0), 4 }, { RA(persona, 1), 4 }, { RA(persona, 2), 4 },
    { RA(persona, 3), 4 }, { RA(persona, 4), 4 },
    { RA(stat, 0), 7 }, { RA(stat, 1), 7 }, { RA(stat, 2), 7 }, { RA(stat, 3), 7 },
    { RA(stat, 4), 7 }, { RA(stat, 5), 7 }, { RA(stat, 6), 7 }, { RA(stat, 7), 7 },
    { RO(status), 3 }, { RO(flags), 8 }, { RO(pronoun), 3 }, { RO(romance), 3 },
    { RO(bday_month), 4 }, { RO(bday_day), 5 }, { RO(like_food), 6 },
    { RO(like_hobby), 6 }, { RO(like_music), 5 }, { RO(like_color), 5 },
    { RO(hate_food), 6 }, { RO(fear), 5 }, { RO(catchphrase), 7 },
    { RO(exorcism), 8 }, { RO(level), 7 }, { RO(mood), 5 }, { RO(activity), 5 },
    { RO(death_cause), 4 }
};
#define NFIELDS ((int)(sizeof(rfields) / sizeof(rfields[0])))
#define NGROUPS ((NFIELDS + 7) / 8)
DOOMO_STATIC_ASSERT(sizeof(rfields) / sizeof(rfields[0]) == 48 + 1 + 5 + 5 + 8 + 18, sv_nfields);

static int name_len(const DoomoResident *r)
{
    int n = 0;
    while (n < DOOMO_NAME_LEN - 1 && r->name[n]) n++;
    return n;
}

static uint32_t field_get(const DoomoResident *r, int i)
{
    uint8_t v = ((const uint8_t *)r)[rfields[i].off];
    if (rfields[i].off == LO(hair_highlight) && v == LOOK_NO_COLOR) return 32;
    return v;
}

static void field_set(DoomoResident *r, int i, uint32_t v)
{
    if (rfields[i].off == LO(hair_highlight) && v == 32) v = LOOK_NO_COLOR;
    ((uint8_t *)r)[rfields[i].off] = (uint8_t)v;
}

/* field_get(a, i) == field_get(b, i), raw bytes first (the only mapped
 * value is hair_highlight's "no colour") */
static inline int field_eq(const DoomoResident *a, const DoomoResident *b, int i)
{
    uint8_t off = rfields[i].off;
    if (rfields[i].bits == NAME_FIELD) return memcmp(a->name, b->name, DOOMO_NAME_LEN) == 0;
    if (((const uint8_t *)a)[off] == ((const uint8_t *)b)[off]) return 1;
    return off == LO(hair_highlight) && field_get(a, i) == field_get(b, i);
}

static int field_cost(const DoomoResident *r, int i)
{
    return rfields[i].bits == NAME_FIELD ? 4 + 7 * name_len(r) : rfields[i].bits;
}

static void field_write(const DoomoResident *r, int i)
{
    if (rfields[i].bits == NAME_FIELD) {
        int n = name_len(r), k;
        wbits((uint32_t)n, 4);
        for (k = 0; k < n; k++) wbits((uint32_t)r->name[k] & 0x7F, 7);
    } else {
        wbits(field_get(r, i), rfields[i].bits);
    }
}

static void field_read(DoomoResident *r, int i)
{
    if (rfields[i].bits == NAME_FIELD) {
        int n = (int)rbits(4), k;
        memset(r->name, 0, DOOMO_NAME_LEN);
        if (n > DOOMO_NAME_LEN - 1) { n = DOOMO_NAME_LEN - 1; Z.ld_err = 1; }
        for (k = 0; k < n; k++) r->name[k] = (char)rbits(7);
    } else {
        field_set(r, i, rbits(rfields[i].bits));
    }
}

static void write_full(const DoomoResident *r)
{
    int i;
    for (i = 0; i < NFIELDS; i++) field_write(r, i);
}

static void read_full(DoomoResident *r)
{
    int i;
    memset(r, 0, sizeof(*r));
    for (i = 0; i < NFIELDS; i++) field_read(r, i);
}

/* The fields of r that differ from base (fm[g] bit k = field 8g + k) and
 * the cost in bits of the delta record (without the tag). */
static int delta_masks(const DoomoResident *r, const DoomoResident *base, uint8_t fm[NGROUPS])
{
    int g, i, bits = NGROUPS;
    for (g = 0; g < NGROUPS; g++) {
        fm[g] = 0;
        for (i = g * 8; i < g * 8 + 8 && i < NFIELDS; i++)
            if (!field_eq(r, base, i)) {
                fm[g] |= (uint8_t)(1 << (i - g * 8));
                bits += field_cost(r, i);
            }
        if (fm[g]) bits += 8;
    }
    return bits;
}

static int full_cost(const DoomoResident *r)
{
    int i, bits = 0;
    for (i = 0; i < NFIELDS; i++) bits += field_cost(r, i);
    return bits;
}

static void write_delta(const DoomoResident *r, const uint8_t fm[NGROUPS])
{
    int g, i;
    uint32_t gmask = 0;
    for (g = 0; g < NGROUPS; g++)
        if (fm[g]) gmask |= 1u << g;
    wbits(gmask, NGROUPS);
    for (g = 0; g < NGROUPS; g++) {
        if (!fm[g]) continue;
        wbits(fm[g], 8);
        for (i = g * 8; i < g * 8 + 8 && i < NFIELDS; i++)
            if (fm[g] & (1 << (i - g * 8))) field_write(r, i);
    }
}

static void read_delta(DoomoResident *r, const DoomoResident *base)
{
    int g, i;
    uint32_t gmask = rbits(NGROUPS);
    *r = *base;
    for (g = 0; g < NGROUPS; g++) {
        uint32_t fm;
        if (!(gmask & (1u << g))) continue;
        fm = rbits(8);
        for (i = g * 8; i < g * 8 + 8 && i < NFIELDS; i++)
            if (fm & (1u << (i - g * 8))) field_read(r, i);
    }
}

/* base record of slot id for this island (0 if none) */
static int base_of(int population, uint32_t seed, int id, DoomoResident *out)
{
    if (population == TOWN_POP_PREMADE) town_premade_record(id, out);
    else if (population == TOWN_POP_RANDOM) town_random_record(seed, id, out);
    else return 0;
    resident_sanitize(out);
    return 1;
}

/* ====================================================================== */
/* sections                                                                 */
#define SEC_MAGIC 0xD0u
#define END_MAGIC 0xB00Fu

static void write_pet(const DoomoPet *p)
{
    wraw((const uint8_t *)p->name, 8);
    w8(p->stage); w8(p->form); w8(p->hunger); w8(p->joy); w8(p->discipline);
    w8(p->holiness); w8(p->weight); w8(p->poop); w8(p->sick); w8(p->asleep);
    w8(p->mistakes); w8(p->age_days); w16(p->stage_timer); w16(p->care_score);
}
static void read_pet(DoomoPet *p)
{
    rraw((uint8_t *)p->name, 8);
    p->stage = (uint8_t)r8(); p->form = (uint8_t)r8(); p->hunger = (uint8_t)r8();
    p->joy = (uint8_t)r8(); p->discipline = (uint8_t)r8(); p->holiness = (uint8_t)r8();
    p->weight = (uint8_t)r8(); p->poop = (uint8_t)r8(); p->sick = (uint8_t)r8();
    p->asleep = (uint8_t)r8(); p->mistakes = (uint8_t)r8(); p->age_days = (uint8_t)r8();
    p->stage_timer = (uint16_t)r16(); p->care_score = (uint16_t)r16();
}

static void write_story(const DoomoStoryState *s)
{
    int i;
    w8(s->chapter); w8(s->map); w16((uint16_t)s->x); w16((uint16_t)s->y);
    w8(s->facing); w8(s->equipped); w8(s->hp); w8(s->armor);
    for (i = 0; i < 8; i++) w16(s->ammo[i]);
    w8(s->quest); w8(s->quest_step); w16(s->kills);
    wraw(s->flags, DOOMO_STORY_FLAGS / 8);
}
static void read_story(DoomoStoryState *s)
{
    int i;
    s->chapter = (uint8_t)r8(); s->map = (uint8_t)r8();
    s->x = (int16_t)(uint16_t)r16(); s->y = (int16_t)(uint16_t)r16();
    s->facing = (uint8_t)r8(); s->equipped = (uint8_t)r8();
    s->hp = (uint8_t)r8(); s->armor = (uint8_t)r8();
    for (i = 0; i < 8; i++) s->ammo[i] = (uint16_t)r16();
    s->quest = (uint8_t)r8(); s->quest_step = (uint8_t)r8(); s->kills = (uint16_t)r16();
    rraw(s->flags, DOOMO_STORY_FLAGS / 8);
}

static void write_town(void)
{
    const DoomoTown *t = &doomo_town;
    DoomoResident cur, base;
    uint8_t fm[NGROUPS];
    int i, n;

    w8(SEC_MAGIC);
    w8(t->mode); w8(t->weekday); w16(t->minute); w32(t->day); w32(t->money);
    w32(t->seed); w16(t->monster_meter); w16(t->exorcisms_won); w8(t->population);
    /* inventory: non-zero entries */
    for (i = 0, n = 0; i < DOOMO_MAX_ITEMS; i++) if (t->items[i]) n++;
    w8((uint32_t)n);
    for (i = 0; i < DOOMO_MAX_ITEMS; i++) if (t->items[i]) { w8((uint32_t)i); w8(t->items[i]); }
    wraw((const uint8_t *)t->weapons, (int)sizeof(t->weapons));
    write_pet(&t->pet);
    write_story(&t->story);
    wraw(t->opt, DOOMO_OPT_BYTES);
    wraw(t->unlocks, DOOMO_UNLOCK_BITS / 8);
    for (i = 0; i < 32; i++) w32(t->progress[i]);
    n = DOOMO_MIN(t->grave_count, DOOMO_MAX_GRAVES);
    w16((uint32_t)n);
    for (i = 0; i < n; i++) { w16(t->graves[i].resident); w16(t->graves[i].day); }
    /* relationship graph: normally sorted by (a, b), so most edges are
     * "same a, next b" (1 bit + a small varint); anything else is raw */
    n = DOOMO_MIN(t->edge_count, DOOMO_MAX_EDGES);
    w16((uint32_t)n);
    {
        uint32_t pa = 0xFFFFu, pb = 0;
        for (i = 0; i < n; i++) {
            const DoomoEdge *e = &t->edges[i];
            uint32_t a = e->a, b = e->b;
            if (a == pa && b > pb) {
                wbits(1, 1);
                wvar(b - pb - 1);
            } else {
                wbits(0, 1);
                wbits(a, 11);
                wbits(b, 11);
            }
            w8((uint8_t)e->value);
            w8(e->kind);
            pa = a; pb = b;
        }
    }
    walign();
    /* the player */
    cur = t->player;
    resident_sanitize(&cur);
    write_full(&cur);
    /* residents */
    i = 0;
    while (i < DOOMO_MAX_RESIDENTS) {
        int has_base, run;
        if (t->res[i].status == RES_EMPTY) {
            for (run = 1; i + run < DOOMO_MAX_RESIDENTS && run < 1024 &&
                          t->res[i + run].status == RES_EMPTY; run++) {}
            wbits(0, 2);
            wbits((uint32_t)(run - 1), 10);
            i += run;
            continue;
        }
        cur = t->res[i];
        resident_sanitize(&cur);
        has_base = base_of(t->population, t->seed, i, &base);
        if (has_base && memcmp(&cur, &base, sizeof(cur)) == 0) {
            for (run = 1; i + run < DOOMO_MAX_RESIDENTS && run < 1024; run++) {
                DoomoResident c2 = t->res[i + run], b2;
                if (c2.status == RES_EMPTY) break;
                resident_sanitize(&c2);
                base_of(t->population, t->seed, i + run, &b2);
                if (memcmp(&c2, &b2, sizeof(c2)) != 0) break;
            }
            wbits(1, 2);
            wbits((uint32_t)(run - 1), 10);
            i += run;
            continue;
        }
        if (has_base && delta_masks(&cur, &base, fm) < full_cost(&cur)) {
            wbits(2, 2);
            write_delta(&cur, fm);
        } else {
            wbits(3, 2);
            write_full(&cur);
        }
        i++;
    }
    w16(END_MAGIC);
    walign();
}

/* Decode the stream. apply == 0: validate only. Returns 1 if well formed. */
static int read_town(int apply)
{
    DoomoTown *t = &doomo_town;
    DoomoResident tmp, base;
    uint8_t scal_mode, scal_week, pop;
    uint32_t seed;
    int i, n;

    if (r8() != SEC_MAGIC) return 0;
    scal_mode = (uint8_t)r8();
    scal_week = (uint8_t)r8();
    if (apply) {
        memset(t, 0, sizeof(*t));
        t->mode = scal_mode;
        t->weekday = scal_week;
        t->minute = (uint16_t)r16(); t->day = r32(); t->money = r32();
        t->seed = r32(); t->monster_meter = (uint16_t)r16();
        t->exorcisms_won = (uint16_t)r16(); t->population = (uint8_t)r8();
        seed = t->seed;
        pop = t->population;
    } else {
        r16(); r32(); r32();
        seed = r32();
        r16(); r16();
        pop = (uint8_t)r8();
    }
    if (pop > TOWN_POP_RANDOM) return 0;
    n = (int)r8();
    if (n > DOOMO_MAX_ITEMS) return 0;
    for (i = 0; i < n; i++) {
        uint32_t id = r8(), c = r8();
        if (id >= DOOMO_MAX_ITEMS) return 0;
        if (apply) t->items[id] = (uint8_t)c;
    }
    rraw_or_skip(apply ? (uint8_t *)t->weapons : NULL, (int)sizeof(t->weapons));
    {
        DoomoPet pet;
        DoomoStoryState st;
        read_pet(apply ? &t->pet : &pet);
        read_story(apply ? &t->story : &st);
    }
    rraw_or_skip(apply ? t->opt : NULL, DOOMO_OPT_BYTES);
    if (apply) lang_loaded();
    rraw_or_skip(apply ? t->unlocks : NULL, DOOMO_UNLOCK_BITS / 8);
    for (i = 0; i < 32; i++) {
        uint32_t v = r32();
        if (apply) t->progress[i] = v;
    }
    n = (int)r16();
    if (n > DOOMO_MAX_GRAVES) return 0;
    for (i = 0; i < n; i++) {
        uint32_t rid = r16(), day = r16();
        if (apply) { t->graves[i].resident = (uint16_t)rid; t->graves[i].day = (uint16_t)day; }
    }
    if (apply) t->grave_count = (uint16_t)n;
    n = (int)r16();
    if (n > DOOMO_MAX_EDGES) return 0;
    {
        uint32_t pa = 0xFFFFu, pb = 0;
        for (i = 0; i < n; i++) {
            uint32_t a, b, v, k;
            if (rbits(1)) {
                if (pa == 0xFFFFu) return 0;
                a = pa;
                b = pb + 1 + rvar();
            } else {
                a = rbits(11);
                b = rbits(11);
            }
            v = r8();
            k = r8();
            if (a > DOOMO_PLAYER_ID || b > DOOMO_PLAYER_ID) return 0;
            if (apply) {
                t->edges[i].a = (uint16_t)a;
                t->edges[i].b = (uint16_t)b;
                t->edges[i].value = (int8_t)(uint8_t)v;
                t->edges[i].kind = (uint8_t)k;
            }
            pa = a; pb = b;
        }
    }
    if (apply) t->edge_count = (uint16_t)n;
    ralign();
    read_full(apply ? &t->player : &tmp);
    i = 0;
    while (i < DOOMO_MAX_RESIDENTS) {
        uint32_t tag = rbits(2);
        if (Z.ld_err) return 0;
        if (tag == 0 || tag == 1) {
            int run = (int)rbits(10) + 1, k;
            if (i + run > DOOMO_MAX_RESIDENTS) return 0;
            if (tag == 1 && pop == TOWN_POP_EMPTY) return 0;
            if (apply) {
                for (k = 0; k < run; k++) {
                    if (tag == 0) memset(&t->res[i + k], 0, sizeof(DoomoResident));
                    else base_of(pop, seed, i + k, &t->res[i + k]);
                }
            }
            i += run;
        } else if (tag == 2) {
            if (pop == TOWN_POP_EMPTY) return 0;
            if (apply) {
                base_of(pop, seed, i, &base);
                read_delta(&t->res[i], &base);
            } else {
                memset(&base, 0, sizeof(base));
                read_delta(&tmp, &base);
            }
            i++;
        } else {
            read_full(apply ? &t->res[i] : &tmp);
            i++;
        }
    }
    if (r16() != END_MAGIC) return 0;
    ralign();
    return !Z.ld_err;
}

/* ====================================================================== */
/* extension chunks (save_ext.h)                                            */
/* After the town data (byte aligned): every registered chunk as
 * [tag u32][version u16][len u32][len bytes]; the header's raw stream size
 * says where they end. write() / read() see the chunk in the staging area
 * of the work area (save.h), so no module needs a buffer of its own, and
 * the bytes are produced twice when a save streams (they must not change
 * between the two passes - the stream is checked against pass 1). */
static void write_exts(void)
{
    int i, n = save_ext_count();
    for (i = 0; i < n; i++) {
        const DoomoSaveExt *e = save_ext_at(i);
        uint32_t len;
        if (!e || !e->write) continue;
        len = e->size ? e->size() : DOOMO_SAVE_EXT_MAX;
        if (len > DOOMO_SAVE_EXT_MAX) { Z.ext_err = 1; continue; }
        len = e->write(Z.stage, len);
        if (len > DOOMO_SAVE_EXT_MAX) { Z.ext_err = 1; continue; }
        w32(e->tag);
        w16(e->version);
        w32(len);
        wraw(Z.stage, (int)len);
    }
}

static const DoomoSaveExt *ext_by_tag(uint32_t tag, int *index)
{
    int i, n = save_ext_count();
    for (i = 0; i < n; i++) {
        const DoomoSaveExt *e = save_ext_at(i);
        if (e && e->tag == tag) { *index = i; return e; }
    }
    return NULL;
}

/* the chunks after the town data, up to raw stream byte `raw`. Unknown
 * tags are skipped; known ones are handed to read() on the applying pass
 * (a chunk read() rejects is reset() instead - it never fails the load);
 * registered chunks the save does not have are reset(). 0 = malformed. */
static int read_exts(uint32_t raw, int apply)
{
    uint32_t seen = 0;
    int i, n = save_ext_count();
    while (!Z.ld_err && Z.ld_out < raw) {
        uint32_t tag, ver, len;
        int idx = 0;
        const DoomoSaveExt *e;
        if (raw - Z.ld_out < 10) return 0;
        tag = r32();
        ver = r16();
        len = r32();
        if (Z.ld_err || len > raw - Z.ld_out) return 0;
        e = ext_by_tag(tag, &idx);
        if (apply && e && e->read && len <= DOOMO_SAVE_EXT_MAX) {
            rraw(Z.stage, (int)len);
            if (Z.ld_err) return 0;
            if (!e->read(Z.stage, len, (uint16_t)ver) && e->reset) e->reset();
            if (idx < 32) seen |= 1u << idx;
        } else {
            rraw_or_skip(NULL, (int)len);
        }
    }
    if (apply)
        for (i = 0; i < n && i < 32; i++) {
            const DoomoSaveExt *e = save_ext_at(i);
            if (!(seen & (1u << i)) && e && e->reset) e->reset();
        }
    return !Z.ld_err && Z.ld_out == raw;
}

/* ====================================================================== */
/* the work area                                                            */
/* The scratch buffer split as save.h describes: binds the codec to its
 * last DOOMO_SAVE_WORK_SIZE bytes and returns the front part (its size in
 * *cap), or NULL when the platform offers too little. */
static uint8_t *scratch_front(uint32_t *cap)
{
    uint32_t c = 0, front;
    uint8_t *s = (uint8_t *)plat_save_scratch(&c);
    *cap = 0;
    if (!s || c < DOOMO_SAVE_STREAM_MIN) return NULL;
    front = c - DOOMO_SAVE_WORK_SIZE;
    front -= (uint32_t)((uintptr_t)(s + front) & 3u);   /* align the work area */
    work_bind(s + front);
    *cap = front;
    return s;
}

/* bind the work area for a caller-supplied image buffer (save_serialize /
 * save_deserialize); 0 if there is none or it overlaps the buffer */
static int work_for(const uint8_t *buf, uint32_t len)
{
    uint32_t cap;
    uintptr_t b0, b1, w0, w1;
    if (!scratch_front(&cap)) return 0;
    if (!buf) return 1;
    b0 = (uintptr_t)buf; b1 = b0 + len;
    w0 = (uintptr_t)Z.ring; w1 = w0 + DOOMO_SAVE_WORK_SIZE;
    return b1 <= w0 || b0 >= w1;
}

/* ====================================================================== */
/* public API                                                               */
static uint32_t last_size;
static char last_error[64];

uint32_t save_last_size(void) { return last_size; }
const char *save_last_error(void) { return last_error; }

uint32_t save_slot_max(int slot)
{
    if (slot == 0) return DOOMO_SAVE_SLOT0_MAX;
    if (slot == 1) return DOOMO_SAVE_SLOT1_MAX;
    return 0;
}

static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v); put16(p + 2, v >> 16); }
static uint32_t get16(const uint8_t *p) { return p[0] | ((uint32_t)p[1] << 8); }
static uint32_t get32(const uint8_t *p) { return get16(p) | (get16(p + 2) << 16); }

/* encode doomo_town into the current sink; returns the body size */
static uint32_t encode(void)
{
    Z.sk_n = 0;
    Z.sk_fill = 0;
    Z.sk_crc = 0xFFFFFFFFu;
    lz_reset();
    Z.bw_acc = 0; Z.bw_n = 0;
    Z.ext_err = 0;
    write_town();
    write_exts();
    lz_finish();
    return Z.sk_n;
}

/* everything of the 48-byte header but the CRC (bytes 12..15) */
static void make_header(uint8_t *h, uint32_t total, uint32_t raw)
{
    int i;
    memset(h, 0, HDR);
    put32(h + 0, DOOMO_SAVE_MAGIC);
    put16(h + 4, DOOMO_SAVE_VERSION);
    put16(h + 6, HDR);
    put32(h + 8, total);
    put32(h + 16, raw);
    h[20] = doomo_town.mode;
    h[21] = doomo_town.story.chapter;
    h[22] = doomo_town.population;
    h[23] = doomo_town.weekday;
    put16(h + 24, (uint32_t)town_count(-1));
    put16(h + 26, doomo_town.minute);
    put32(h + 28, doomo_town.day);
    put32(h + 32, doomo_town.money);
    for (i = 0; i < DOOMO_NAME_LEN - 1 && doomo_town.player.name[i]; i++)
        h[36 + i] = (uint8_t)doomo_town.player.name[i];
}

static void too_big(uint32_t total, uint32_t cap)
{
    doomo_snprintf(last_error, sizeof(last_error), "Island too big: %u KB > %u KB",
                   (unsigned)((total + 1023) / 1024), (unsigned)(cap / 1024));
}

static int ext_too_big(void)
{
    if (!Z.ext_err) return 0;
    doomo_snprintf(last_error, sizeof(last_error), "Save extension over %u KB",
                   (unsigned)(DOOMO_SAVE_EXT_MAX / 1024));
    return 1;
}

static int no_memory(void)
{
    doomo_snprintf(last_error, sizeof(last_error), "No memory to save");
    return 0;
}

/* the whole image into buf (the work area is bound) */
static uint32_t serialize_into(uint8_t *buf, uint32_t cap)
{
    uint32_t total;
    last_error[0] = 0;
    Z.sk_mode = SK_MEM;
    Z.sk_buf = buf ? buf + HDR : NULL;
    Z.sk_cap = (buf && cap > HDR) ? cap - HDR : 0;
    total = HDR + encode();
    last_size = total;
    if (ext_too_big()) return 0;
    if (!buf || total > cap) {
        too_big(total, cap);
        return 0;
    }
    make_header(buf, total, Z.lz_in);
    put32(buf + 12, save_crc32(buf + 16, total - 16));
    return total;
}

uint32_t save_serialize(uint8_t *buf, uint32_t cap)
{
    if (!work_for(buf, cap)) return (uint32_t)no_memory();
    return serialize_into(buf, cap);
}

int save_info_from(const uint8_t *buf, uint32_t size, DoomoSaveInfo *out)
{
    DoomoSaveInfo info;
    int i;
    if (!buf || size < HDR) return 0;
    if (get32(buf) != DOOMO_SAVE_MAGIC || get16(buf + 4) != DOOMO_SAVE_VERSION ||
        get16(buf + 6) != HDR)
        return 0;
    memset(&info, 0, sizeof(info));
    info.mode = buf[20];
    info.chapter = buf[21];
    info.population = buf[22];
    info.weekday = buf[23];
    info.residents = (uint16_t)get16(buf + 24);
    info.minute = (uint16_t)get16(buf + 26);
    info.day = get32(buf + 28);
    info.money = get32(buf + 32);
    info.size = get32(buf + 8);
    for (i = 0; i < DOOMO_NAME_LEN - 1; i++) {
        uint8_t c = buf[36 + i];
        info.player[i] = (char)((c >= 0x20 && c <= 0x7E) ? c : 0);
        if (!c) break;
    }
    if (out) *out = info;
    return 1;
}

static int check_image(const uint8_t *buf, uint32_t size)
{
    uint32_t total;
    if (!save_info_from(buf, size, NULL)) { doomo_snprintf(last_error, sizeof(last_error), "Not a save file"); return 0; }
    total = get32(buf + 8);
    if (total < HDR || total > size) { doomo_snprintf(last_error, sizeof(last_error), "Save is truncated"); return 0; }
    if (save_crc32(buf + 16, total - 16) != get32(buf + 12)) {
        doomo_snprintf(last_error, sizeof(last_error), "Save is damaged (bad CRC)");
        return 0;
    }
    return 1;
}

/* decode the body described by header h: from memory (body != NULL) or
 * streamed from Z.ld_slot */
static int decode(const uint8_t *h, const uint8_t *body, int apply)
{
    uint32_t total = get32(h + 8), raw = get32(h + 16);
    int ok;
    ld_reset(body, total - HDR);
    Z.br_acc = 0; Z.br_n = 0;
    ok = read_town(apply) && read_exts(raw, apply);
    return ok && !Z.ld_err && Z.ld_out == raw && Z.ld_pos == total - HDR;
}

/* both decodes + the fix-ups after a load */
static int load_decoded(const uint8_t *h, const uint8_t *body)
{
    int i;
    if (!decode(h, body, 0)) {
        doomo_snprintf(last_error, sizeof(last_error), "Save data is malformed");
        return 0;
    }
    decode(h, body, 1);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        if (doomo_town.res[i].status != RES_EMPTY) resident_sanitize(&doomo_town.res[i]);
        portrait_invalidate(i);
    }
    resident_sanitize(&doomo_town.player);
    portrait_invalidate(DOOMO_PLAYER_ID);
    rel_normalize();
    last_size = get32(h + 8);
    return 1;
}

/* the whole image in memory (the work area is bound) */
static int deserialize_from(const uint8_t *buf, uint32_t size)
{
    last_error[0] = 0;
    if (!check_image(buf, size)) return 0;
    Z.ld_slot = -1;
    return load_decoded(buf, buf + HDR);
}

int save_deserialize(const uint8_t *buf, uint32_t size)
{
    if (!work_for(buf, size)) return no_memory();
    return deserialize_from(buf, size);
}

/* Streamed save (platform.h): pass 1 sizes the save and checksums the
 * body, pass 2 streams the same bytes in chunks of the scratch buffer. */
static int save_stream(int slot, uint8_t *chunk, uint32_t chunk_cap, uint32_t total,
                       uint32_t body_crc)
{
    uint8_t hdr[HDR];
    uint32_t n = total - HDR, raw = Z.lz_in;
    int ok;
    make_header(hdr, total, raw);
    put32(hdr + 12, crc32_combine(save_crc32(hdr + 16, HDR - 16), body_crc, n));
    if (!plat_save_stream_begin(slot, total)) return 0;
    Z.sk_err = !plat_save_stream_write(hdr, HDR);
    Z.sk_mode = SK_STREAM;
    Z.sk_buf = chunk;
    Z.sk_cap = chunk_cap;
    encode();
    sk_flush();
    ok = !Z.sk_err && !Z.ext_err && Z.sk_n == n && (Z.sk_crc ^ 0xFFFFFFFFu) == body_crc &&
         Z.lz_in == raw;
    return plat_save_stream_end(ok) && ok;
}

int save_game(int slot)
{
    uint32_t cap = 0, max = save_slot_max(slot), n, total, body_crc;
    uint8_t *buf;
    last_error[0] = 0;
    if (!max) { doomo_snprintf(last_error, sizeof(last_error), "No such save slot"); return 0; }
    buf = scratch_front(&cap);
    if (!buf || cap < HDR) return no_memory();
    if (cap >= max) {
        /* the whole image fits in front of the work area: build it there */
        n = serialize_into(buf, max);
        if (!n) { if (!Z.ext_err) too_big(last_size, max); return 0; }
        if (!plat_save_write(slot, buf, n)) {
            doomo_snprintf(last_error, sizeof(last_error), "Couldn't write save slot %d", slot);
            return 0;
        }
        return 1;
    }
    /* pass 1: how big is it? */
    Z.sk_mode = SK_COUNT;
    total = HDR + encode();
    body_crc = Z.sk_crc ^ 0xFFFFFFFFu;
    last_size = total;
    if (ext_too_big()) return 0;
    if (total > max) { too_big(total, max); return 0; }
    if (total <= cap) {
        /* it fits after all: in memory (works without streaming support) */
        n = serialize_into(buf, cap);
        if (!n || !plat_save_write(slot, buf, n)) {
            doomo_snprintf(last_error, sizeof(last_error), "Couldn't write save slot %d", slot);
            return 0;
        }
        return 1;
    }
    if (!save_stream(slot, buf, cap, total, body_crc)) {
        doomo_snprintf(last_error, sizeof(last_error), "Couldn't write save slot %d", slot);
        return 0;
    }
    return 1;
}

static int read_slot(int slot, uint8_t **out, uint32_t *size)
{
    uint32_t cap = 0;
    uint8_t *buf = scratch_front(&cap);
    *size = 0;
    if (!buf || !save_slot_max(slot)) return 0;
    if (!plat_save_read(slot, buf, cap, size) || *size < HDR) return 0;
    *out = buf;
    return 1;
}

/* Streamed load: the header, a CRC pass over the slot, then the decodes
 * read the body through the scratch buffer (window). 1 = verified;
 * *hdr holds the header. */
static int stream_check(int slot, uint8_t *win, uint32_t wcap, uint8_t *hdr)
{
    uint32_t size = 0, total, pos, c;
    if (!plat_save_read_at(slot, 0, hdr, HDR, &size)) {
        doomo_snprintf(last_error, sizeof(last_error), "No save in slot %d", slot);
        return 0;
    }
    if (!save_info_from(hdr, HDR, NULL)) {
        doomo_snprintf(last_error, sizeof(last_error), "Not a save file");
        return 0;
    }
    total = get32(hdr + 8);
    if (total < HDR || total > size) {
        doomo_snprintf(last_error, sizeof(last_error), "Save is truncated");
        return 0;
    }
    c = crc_update(0xFFFFFFFFu, hdr + 16, HDR - 16);
    for (pos = HDR; pos < total;) {
        uint32_t k = total - pos < wcap ? total - pos : wcap, got = 0;
        if (!plat_save_read_at(slot, pos, win, k, &got)) {
            doomo_snprintf(last_error, sizeof(last_error), "Save is truncated");
            return 0;
        }
        c = crc_update(c, win, k);
        pos += k;
    }
    if ((c ^ 0xFFFFFFFFu) != get32(hdr + 12)) {
        doomo_snprintf(last_error, sizeof(last_error), "Save is damaged (bad CRC)");
        return 0;
    }
    return 1;
}

/* The scratch buffer when it is too small for slot `slot` and the
 * platform reads saves piecewise (plat_save_read_at); NULL otherwise -
 * then the whole-image path runs (and reports a missing save itself). */
static uint8_t *stream_window(int slot, uint32_t *cap)
{
    uint8_t probe[4];
    uint32_t size = 0;
    uint8_t *buf = scratch_front(cap);
    if (!buf || *cap < 64 || *cap >= save_slot_max(slot)) return NULL;
    if (!plat_save_read_at(slot, 0, probe, sizeof(probe), &size)) return NULL;
    return buf;
}

int load_game(int slot)
{
    uint8_t *buf = NULL, hdr[HDR];
    uint32_t size, cap = 0;
    last_error[0] = 0;
    if (!save_slot_max(slot)) {
        doomo_snprintf(last_error, sizeof(last_error), "No save in slot %d", slot);
        return 0;
    }
    buf = stream_window(slot, &cap);
    if (buf) {
        int ok;
        if (!stream_check(slot, buf, cap, hdr)) return 0;
        Z.ld_slot = slot;
        Z.ld_wbuf = buf;
        Z.ld_wcap = cap;
        ok = load_decoded(hdr, NULL);
        Z.ld_slot = -1;
        Z.ld_wbuf = NULL;
        return ok;
    }
    if (!read_slot(slot, &buf, &size)) {
        doomo_snprintf(last_error, sizeof(last_error), "No save in slot %d", slot);
        return 0;
    }
    return deserialize_from(buf, size);
}

int save_peek(int slot, DoomoSaveInfo *out)
{
    uint8_t hdr[HDR];
    uint8_t *buf = NULL;
    uint32_t size = 0;
    if (!save_slot_max(slot)) return 0;
    /* cheap path: just the header, no scratch buffer, no CRC */
    if (plat_save_read(slot, hdr, HDR, &size) && size == HDR)
        return save_info_from(hdr, HDR, out);
    /* platforms that cannot read part of a save: read and verify it all */
    if (!read_slot(slot, &buf, &size)) return 0;
    if (!check_image(buf, size)) return 0;
    return save_info_from(buf, size, out);
}

int save_exists(int slot)
{
    return save_peek(slot, NULL);
}

/* A full check of a slot (CRC included) without loading it. */
int save_verify(int slot)
{
    uint8_t *buf = NULL, hdr[HDR];
    uint32_t size, cap = 0;
    if (!save_slot_max(slot)) return 0;
    buf = stream_window(slot, &cap);
    if (buf) return stream_check(slot, buf, cap, hdr);
    if (!read_slot(slot, &buf, &size)) return 0;
    return check_image(buf, size);
}
