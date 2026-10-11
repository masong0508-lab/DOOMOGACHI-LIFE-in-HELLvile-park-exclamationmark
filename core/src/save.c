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
 * distance d (u16 LE that follows). Encoder and decoder both stream, so
 * the only big buffer needed is the save image itself.
 *
 * Loading verifies magic, version and CRC, then decodes twice: a dry run
 * that validates the whole stream, and only then the real decode into
 * doomo_town (which is therefore never left half-loaded).
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
uint32_t save_crc32(const uint8_t *buf, uint32_t size)
{
    static const uint32_t tab[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u,
        0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu, 0xEDB88320u, 0xF00F9344u,
        0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u,
        0xBDBDF21Cu
    };
    uint32_t c = 0xFFFFFFFFu, i;
    for (i = 0; i < size; i++) {
        c ^= buf[i];
        c = (c >> 4) ^ tab[c & 15];
        c = (c >> 4) ^ tab[c & 15];
    }
    return c ^ 0xFFFFFFFFu;
}

/* ====================================================================== */
/* LZ codec                                                                 */
#define LZ_RING 4096u
#define LZ_MASK (LZ_RING - 1u)
#define LZ_MAXOFF 3840u
#define LZ_MAXLEN 130u
#define LZ_HASH 1024u

static uint8_t lz_ring[LZ_RING];
static uint16_t lz_head[LZ_HASH];
static uint8_t lz_lit[128];
static uint32_t lz_nlit, lz_in, lz_proc;

/* output sink (counts past its capacity so callers learn the size) */
static uint8_t *sk_buf;
static uint32_t sk_cap, sk_n;

static void sk_put(uint8_t b)
{
    if (sk_n < sk_cap) sk_buf[sk_n] = b;
    sk_n++;
}

static void lz_reset(void)
{
    memset(lz_head, 0, sizeof(lz_head));
    lz_nlit = lz_in = lz_proc = 0;
}

static uint32_t lz_hash3(uint32_t p)
{
    uint32_t v = ((uint32_t)lz_ring[p & LZ_MASK] << 16) |
                 ((uint32_t)lz_ring[(p + 1) & LZ_MASK] << 8) | lz_ring[(p + 2) & LZ_MASK];
    return (v * 2654435761u) >> 22;
}

static void lz_flush_lits(void)
{
    uint32_t i;
    if (!lz_nlit) return;
    sk_put((uint8_t)(lz_nlit - 1));
    for (i = 0; i < lz_nlit; i++) sk_put(lz_lit[i]);
    lz_nlit = 0;
}

static void lz_step(void)
{
    uint32_t p = lz_proc, avail = lz_in - p, best = 0, boff = 0;
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
            if (lz_in - (p + k) >= 3) lz_head[lz_hash3(p + k)] = (uint16_t)(p + k + 1);
        lz_proc += best;
    } else {
        lz_lit[lz_nlit++] = lz_ring[p & LZ_MASK];
        if (lz_nlit == 128) lz_flush_lits();
        lz_proc++;
    }
}

static void lz_push(uint8_t b)
{
    lz_ring[lz_in & LZ_MASK] = b;
    lz_in++;
    while (lz_in - lz_proc >= LZ_MAXLEN) lz_step();
}

static void lz_finish(void)
{
    while (lz_proc < lz_in) lz_step();
    lz_flush_lits();
}

/* decoder (pull) */
static const uint8_t *ld_src;
static uint32_t ld_n, ld_pos, ld_out, ld_lit, ld_match, ld_dist;
static int ld_err;

static void ld_reset(const uint8_t *src, uint32_t n)
{
    ld_src = src; ld_n = n; ld_pos = 0; ld_out = 0;
    ld_lit = ld_match = ld_dist = 0;
    ld_err = 0;
}

static uint8_t ld_get(void)
{
    uint8_t b;
    if (ld_err) return 0;
    if (!ld_lit && !ld_match) {
        uint8_t c;
        if (ld_pos >= ld_n) { ld_err = 1; return 0; }
        c = ld_src[ld_pos++];
        if (c < 0x80) {
            ld_lit = (uint32_t)c + 1;
        } else {
            if (ld_pos + 2 > ld_n) { ld_err = 1; return 0; }
            ld_match = (uint32_t)(c & 0x7F) + 3;
            ld_dist = ld_src[ld_pos] | ((uint32_t)ld_src[ld_pos + 1] << 8);
            ld_pos += 2;
            if (ld_dist == 0 || ld_dist > ld_out || ld_dist > LZ_MAXOFF) { ld_err = 1; return 0; }
        }
    }
    if (ld_lit) {
        if (ld_pos >= ld_n) { ld_err = 1; return 0; }
        b = ld_src[ld_pos++];
        ld_lit--;
    } else {
        b = lz_ring[(ld_out - ld_dist) & LZ_MASK];
        ld_match--;
    }
    lz_ring[ld_out & LZ_MASK] = b;
    ld_out++;
    return b;
}

/* ====================================================================== */
/* bit stream                                                               */
static uint32_t bw_acc;
static int bw_n;

static void wbits(uint32_t v, int n)
{
    if (n < 32) v &= (1u << n) - 1u;
    bw_acc |= v << bw_n;
    bw_n += n;
    while (bw_n >= 8) {
        lz_push((uint8_t)bw_acc);
        bw_acc >>= 8;
        bw_n -= 8;
    }
}
static void walign(void)
{
    if (bw_n) { lz_push((uint8_t)bw_acc); bw_acc = 0; bw_n = 0; }
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

static uint32_t br_acc;
static int br_n;

static uint32_t rbits(int n)
{
    uint32_t v;
    while (br_n < n) {
        br_acc |= (uint32_t)ld_get() << br_n;
        br_n += 8;
    }
    v = n < 32 ? br_acc & ((1u << n) - 1u) : br_acc;
    br_acc = n < 32 ? br_acc >> n : 0;
    br_n -= n;
    return v;
}
static void ralign(void) { br_acc = 0; br_n = 0; }
static uint32_t r8(void) { return rbits(8); }
static uint32_t r16(void) { return rbits(16); }
static uint32_t r32(void) { uint32_t lo = rbits(16); return lo | (rbits(16) << 16); }
static void rraw(uint8_t *p, int n) { int i; for (i = 0; i < n; i++) p[i] = (uint8_t)r8(); }
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

static int field_eq(const DoomoResident *a, const DoomoResident *b, int i)
{
    if (rfields[i].bits == NAME_FIELD) return memcmp(a->name, b->name, DOOMO_NAME_LEN) == 0;
    return field_get(a, i) == field_get(b, i);
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
        if (n > DOOMO_NAME_LEN - 1) { n = DOOMO_NAME_LEN - 1; ld_err = 1; }
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

/* cost in bits of a delta record (without the tag) */
static int delta_cost(const DoomoResident *r, const DoomoResident *base)
{
    int g, i, bits = NGROUPS;
    for (g = 0; g < NGROUPS; g++) {
        int used = 0;
        for (i = g * 8; i < g * 8 + 8 && i < NFIELDS; i++)
            if (!field_eq(r, base, i)) { used = 1; bits += field_cost(r, i); }
        if (used) bits += 8;
    }
    return bits;
}

static int full_cost(const DoomoResident *r)
{
    int i, bits = 0;
    for (i = 0; i < NFIELDS; i++) bits += field_cost(r, i);
    return bits;
}

static void write_delta(const DoomoResident *r, const DoomoResident *base)
{
    int g, i;
    uint32_t gmask = 0;
    uint8_t fm[NGROUPS];
    for (g = 0; g < NGROUPS; g++) {
        fm[g] = 0;
        for (i = g * 8; i < g * 8 + 8 && i < NFIELDS; i++)
            if (!field_eq(r, base, i)) fm[g] |= (uint8_t)(1 << (i - g * 8));
        if (fm[g]) gmask |= 1u << g;
    }
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
        if (has_base && delta_cost(&cur, &base) < full_cost(&cur)) {
            wbits(2, 2);
            write_delta(&cur, &base);
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
    {
        static DoomoWeapon wtmp[DOOMO_MAX_WEAPONS];
        rraw(apply ? (uint8_t *)t->weapons : (uint8_t *)wtmp, (int)sizeof(t->weapons));
    }
    {
        DoomoPet pet;
        static DoomoStoryState st;
        read_pet(apply ? &t->pet : &pet);
        read_story(apply ? &t->story : &st);
    }
    {
        static uint8_t junk[DOOMO_UNLOCK_BITS / 8];
        rraw(apply ? t->opt : junk, DOOMO_OPT_BYTES);
        rraw(apply ? t->unlocks : junk, DOOMO_UNLOCK_BITS / 8);
    }
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
        if (ld_err) return 0;
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
    return !ld_err;
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

uint32_t save_serialize(uint8_t *buf, uint32_t cap)
{
    uint32_t total;
    int i;
    last_error[0] = 0;
    sk_buf = buf ? buf + HDR : NULL;
    sk_cap = (buf && cap > HDR) ? cap - HDR : 0;
    sk_n = 0;
    lz_reset();
    bw_acc = 0; bw_n = 0;
    write_town();
    lz_finish();
    total = HDR + sk_n;
    last_size = total;
    if (!buf || total > cap) {
        doomo_snprintf(last_error, sizeof(last_error), "Island too big: %u KB > %u KB",
                       (unsigned)((total + 1023) / 1024), (unsigned)(cap / 1024));
        return 0;
    }
    memset(buf, 0, HDR);
    put32(buf + 0, DOOMO_SAVE_MAGIC);
    put16(buf + 4, DOOMO_SAVE_VERSION);
    put16(buf + 6, HDR);
    put32(buf + 8, total);
    put32(buf + 16, lz_in);
    buf[20] = doomo_town.mode;
    buf[21] = doomo_town.story.chapter;
    buf[22] = doomo_town.population;
    buf[23] = doomo_town.weekday;
    put16(buf + 24, (uint32_t)town_count(-1));
    put16(buf + 26, doomo_town.minute);
    put32(buf + 28, doomo_town.day);
    put32(buf + 32, doomo_town.money);
    for (i = 0; i < DOOMO_NAME_LEN - 1 && doomo_town.player.name[i]; i++)
        buf[36 + i] = (uint8_t)doomo_town.player.name[i];
    put32(buf + 12, save_crc32(buf + 16, total - 16));
    return total;
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

static int decode(const uint8_t *buf, int apply)
{
    uint32_t total = get32(buf + 8), raw = get32(buf + 16);
    int ok;
    ld_reset(buf + HDR, total - HDR);
    br_acc = 0; br_n = 0;
    ok = read_town(apply);
    return ok && !ld_err && ld_out == raw && ld_pos == total - HDR;
}

int save_deserialize(const uint8_t *buf, uint32_t size)
{
    int i;
    last_error[0] = 0;
    if (!check_image(buf, size)) return 0;
    if (!decode(buf, 0)) {
        doomo_snprintf(last_error, sizeof(last_error), "Save data is malformed");
        return 0;
    }
    decode(buf, 1);
    for (i = 0; i < DOOMO_MAX_RESIDENTS; i++) {
        if (doomo_town.res[i].status != RES_EMPTY) resident_sanitize(&doomo_town.res[i]);
        portrait_invalidate(i);
    }
    resident_sanitize(&doomo_town.player);
    portrait_invalidate(DOOMO_PLAYER_ID);
    rel_normalize();
    last_size = get32(buf + 8);
    return 1;
}

int save_game(int slot)
{
    uint32_t cap = 0, max = save_slot_max(slot), n;
    uint8_t *buf = (uint8_t *)plat_save_scratch(&cap);
    last_error[0] = 0;
    if (!max) { doomo_snprintf(last_error, sizeof(last_error), "No such save slot"); return 0; }
    if (!buf || cap < HDR) { doomo_snprintf(last_error, sizeof(last_error), "No memory to save"); return 0; }
    if (cap < max) max = cap;
    n = save_serialize(buf, max);
    if (!n) {
        doomo_snprintf(last_error, sizeof(last_error), "Island too big: %u KB > %u KB",
                       (unsigned)((last_size + 1023) / 1024), (unsigned)(max / 1024));
        return 0;
    }
    if (!plat_save_write(slot, buf, n)) {
        doomo_snprintf(last_error, sizeof(last_error), "Couldn't write save slot %d", slot);
        return 0;
    }
    return 1;
}

static int read_slot(int slot, uint8_t **out, uint32_t *size)
{
    uint32_t cap = 0;
    uint8_t *buf = (uint8_t *)plat_save_scratch(&cap);
    *size = 0;
    if (!buf || !save_slot_max(slot)) return 0;
    if (!plat_save_read(slot, buf, cap, size) || *size < HDR) return 0;
    *out = buf;
    return 1;
}

int load_game(int slot)
{
    uint8_t *buf = NULL;
    uint32_t size;
    last_error[0] = 0;
    if (!read_slot(slot, &buf, &size)) {
        doomo_snprintf(last_error, sizeof(last_error), "No save in slot %d", slot);
        return 0;
    }
    return save_deserialize(buf, size);
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
    uint8_t *buf = NULL;
    uint32_t size;
    if (!read_slot(slot, &buf, &size)) return 0;
    return check_image(buf, size);
}
