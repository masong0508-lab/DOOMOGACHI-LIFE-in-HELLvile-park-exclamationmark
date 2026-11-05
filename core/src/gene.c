/* gene.c - the 384-bit gene codec and the v1 bridge (P0.M0a, gene.h).
 * Owner: customisation P0. Plain integer code, no .bss, no state. */
#include "doomo/doomo.h"
#include "doomo/gene.h"

/* name@bit:width, LSB-first, from the spec's 1.3 list. max = the width's mask. */
#define F(pos, bits) { (pos), (uint16_t)((1u << (bits)) - 1u), (bits) }
const DoomoGeneField doomo_gene_field[GF_COUNT] = {
    F(0, 4),   F(4, 4),   F(8, 4),   F(12, 6),  F(18, 5),  F(23, 4),   /* body height build skin head_shape head_size */
    F(27, 6),  F(33, 5),  F(38, 4),  F(42, 4),  F(46, 4),  F(50, 4),   /* eye_style .. eye_tilt */
    F(54, 3),  F(57, 5),  F(62, 4),  F(66, 5),  F(71, 4),              /* eye_count brow_style brow_height nose_style nose_size */
    F(75, 5),  F(80, 4),  F(84, 2),  F(86, 6),  F(92, 5),              /* mouth_style mouth_size fangs hair_style hair_color */
    F(97, 6),  F(103, 4), F(107, 5), F(112, 5), F(117, 5), F(122, 4),  /* hair_highlight facial_hair ears horns horn_color horn_size */
    F(126, 5), F(131, 5), F(136, 4), F(140, 5), F(145, 5), F(150, 8),  /* tail wings halo marking marking_color cheeks */
    F(158, 6), F(164, 5), F(169, 5), F(174, 5), F(179, 5), F(184, 5),  /* top top_color bottom bottom_color shoes shoes_color */
    F(189, 6), F(195, 5), F(200, 5), F(205, 5), F(210, 5), F(215, 5), F(220, 5), /* hat hat_color accessory accessory_color aura pattern pattern_color */
    F(225, 5), F(230, 5), F(235, 5), F(240, 5), F(245, 5),             /* voice[0..4] */
    F(250, 4), F(254, 4), F(258, 4), F(262, 4), F(266, 4),             /* persona[0..4] */
    F(270, 3), F(273, 3), F(276, 4), F(280, 5), F(285, 6), F(291, 5),  /* pronoun romance bday_month bday_day like_food like_hobby */
    F(296, 4), F(300, 5), F(305, 6), F(311, 5), F(316, 6),             /* like_music like_color hate_food fear catchphrase */
    F(322, 11), F(333, 2), F(335, 3), F(338, 5), F(343, 4), F(347, 5), /* look_seed auto_style age pupil sclera fur_pattern */
    F(352, 6), F(358, 5), F(363, 5), F(368, 4), F(372, 4), F(376, 3),  /* fur_color3 glasses outer legwear blush_style ear_fold */
    F(379, 1), F(380, 1), F(381, 3)                                    /* hood_up handedness gene_spare */
};
#undef F

uint32_t gene_get(const uint8_t *g, int gf)
{
    const DoomoGeneField *f = &doomo_gene_field[gf];
    int b = f->pos >> 3, sh = f->pos & 7;
    uint32_t w = g[b];
    if (b + 1 < DOOMO_GENE_BYTES) w |= (uint32_t)g[b + 1] << 8;
    if (b + 2 < DOOMO_GENE_BYTES) w |= (uint32_t)g[b + 2] << 16;
    return (w >> sh) & f->max;
}

void gene_set(uint8_t *g, int gf, uint32_t v)
{
    const DoomoGeneField *f = &doomo_gene_field[gf];
    int b = f->pos >> 3, sh = f->pos & 7;
    uint32_t mask, w;
    if (v > f->max) v = f->max;
    mask = (uint32_t)f->max << sh;
    w = (uint32_t)g[b];
    if (b + 1 < DOOMO_GENE_BYTES) w |= (uint32_t)g[b + 1] << 8;
    if (b + 2 < DOOMO_GENE_BYTES) w |= (uint32_t)g[b + 2] << 16;
    w = (w & ~mask) | (v << sh);
    g[b] = (uint8_t)w;
    if (b + 1 < DOOMO_GENE_BYTES) g[b + 1] = (uint8_t)(w >> 8);
    if (b + 2 < DOOMO_GENE_BYTES) g[b + 2] = (uint8_t)(w >> 16);
}

/* ---- the v1 bridge: the DoomoCitizen (v1 layout) <-> the gene --------------------------- */
#define CIT_PREF(c)   ((uint32_t)((c)->romance & ROMANCE_PREF_MASK))
#define CIT_STAGE(c)  ((uint32_t)(((c)->romance & LIFE_STAGE_MASK) >> LIFE_STAGE_SHIFT))

/* identity members of DoomoCitizen, in gene order from GF_PRONOUN */
static uint8_t *ident(DoomoCitizen *r, int i)
{
    switch (i) {
    case 0: return &r->pronoun;     case 1: return &r->romance;
    case 2: return &r->bday_month;  case 3: return &r->bday_day;
    case 4: return &r->like_food;   case 5: return &r->like_hobby;
    case 6: return &r->like_music;  case 7: return &r->like_color;
    case 8: return &r->hate_food;   case 9: return &r->fear;
    default: return &r->catchphrase;
    }
}

int gene_pack_v1(const DoomoCitizen *r, uint8_t g[DOOMO_GENE_BYTES])
{
    const uint8_t *look = (const uint8_t *)&r->look;
    DoomoCitizen tmp = *r;
    int i, fit = 1;
    uint32_t v;
    memset(g, 0, DOOMO_GENE_BYTES);
    for (i = 0; i < GF_VOICE0; i++) {
        v = look[1 + i];                                  /* 0 is `species` */
        if (i == GF_HAIR_HIGHLIGHT && v == LOOK_NO_COLOR) v = 32;
        if (v > doomo_gene_field[i].max) fit = 0;
        gene_set(g, i, v);
    }
    for (i = 0; i < VOICE_COUNT; i++) {
        v = r->voice[i];
        if (v > doomo_gene_field[GF_VOICE0 + i].max) fit = 0;
        gene_set(g, GF_VOICE0 + i, v);
    }
    for (i = 0; i < PERS_COUNT; i++) {
        v = r->persona[i];
        if (v > doomo_gene_field[GF_PERSONA0 + i].max) fit = 0;
        gene_set(g, GF_PERSONA0 + i, v);
    }
    for (i = 0; i <= GF_CATCHPHRASE - GF_PRONOUN; i++) {
        v = *ident(&tmp, i);
        if (GF_PRONOUN + i == GF_ROMANCE) v = CIT_PREF(r);   /* preference half only */
        if (v > doomo_gene_field[GF_PRONOUN + i].max) fit = 0;
        gene_set(g, GF_PRONOUN + i, v);
    }
    gene_set(g, GF_AGE, CIT_STAGE(r));                /* the life stage, same numbers */
    return fit;
}

void gene_unpack_v1(const uint8_t g[DOOMO_GENE_BYTES], DoomoCitizen *r)
{
    uint8_t *look = (uint8_t *)&r->look;
    int i;
    uint32_t v;
    for (i = 0; i < GF_VOICE0; i++) {
        v = gene_get(g, i);
        if (i == GF_HAIR_HIGHLIGHT && v == 32) v = LOOK_NO_COLOR;
        look[1 + i] = (uint8_t)v;
    }
    for (i = 0; i < VOICE_COUNT; i++) r->voice[i] = (uint8_t)gene_get(g, GF_VOICE0 + i);
    for (i = 0; i < PERS_COUNT; i++) r->persona[i] = (uint8_t)gene_get(g, GF_PERSONA0 + i);
    for (i = 0; i <= GF_CATCHPHRASE - GF_PRONOUN; i++) {
        v = gene_get(g, GF_PRONOUN + i);
        if (GF_PRONOUN + i == GF_ROMANCE)             /* keep the stage bits */
            r->romance = (uint8_t)((r->romance & ~ROMANCE_PREF_MASK) | (v & ROMANCE_PREF_MASK));
        else
            *ident(r, i) = (uint8_t)v;
    }
    v = gene_get(g, GF_AGE);
    if (v >= LIFE_COUNT) v = LIFE_ADULT;              /* 6, 7: invalid, read as an adult */
    r->romance = (uint8_t)((r->romance & ~LIFE_STAGE_MASK) | ((v << LIFE_STAGE_SHIFT) & LIFE_STAGE_MASK));
}

/* ---- whole-record copies (resident_acc.h) ------------------------------------------------ */
void res_get_look(const DoomoResident *r, DoomoLook *out)
{
    uint8_t *l = (uint8_t *)out;
    int i;
    memset(out, 0, sizeof(*out));
    l[0] = r->species;
    for (i = 0; i < GF_VOICE0; i++) l[1 + i] = res_look_get(r, i);
}

void res_set_look(DoomoResident *r, const DoomoLook *in)
{
    const uint8_t *l = (const uint8_t *)in;
    int i;
    r->species = l[0];
    for (i = 0; i < GF_VOICE0; i++) res_look_set(r, i, l[1 + i]);
}

void res_look(const DoomoResident *r, DoomoLook *out) { res_get_look(r, out); }

void res_get_voice(const DoomoResident *r, uint8_t *out)
{
    int i;
    for (i = 0; i < VOICE_COUNT; i++) out[i] = RES_VOICE(r, i);
}
void res_set_voice(DoomoResident *r, const uint8_t *v)
{
    int i;
    for (i = 0; i < VOICE_COUNT; i++) RES_SET_VOICE(r, i, v[i]);
}
void res_get_persona(const DoomoResident *r, uint8_t *out)
{
    int i;
    for (i = 0; i < PERS_COUNT; i++) out[i] = RES_PERSONA(r, i);
}
void res_set_persona(DoomoResident *r, const uint8_t *p)
{
    int i;
    for (i = 0; i < PERS_COUNT; i++) RES_SET_PERSONA(r, i, p[i]);
}

/* ---- the record <-> the working struct ---------------------------------------------------- */
void res_unpack(const DoomoResident *r, DoomoCitizen *out)
{
    memset(out, 0, sizeof(*out));
    out->look.species = r->species;
    memcpy(out->name, r->name, DOOMO_NAME_LEN);
    memcpy(out->stat, r->stat, STAT_COUNT);
    out->status = r->status; out->flags = r->flags; out->exorcism = r->exorcism;
    out->level = r->level; out->mood = r->mood; out->activity = r->activity;
    out->death_cause = r->death_cause;
    gene_unpack_v1(r->gene, out);       /* look (not species), voice, persona, identity, stage */
}

void res_pack(const DoomoCitizen *c, DoomoResident *out)
{
    memset(out, 0, sizeof(*out));
    out->species = c->look.species;
    memcpy(out->name, c->name, DOOMO_NAME_LEN);
    memcpy(out->stat, c->stat, STAT_COUNT);
    out->status = c->status; out->flags = c->flags; out->exorcism = c->exorcism;
    out->level = c->level; out->mood = c->mood; out->activity = c->activity;
    out->death_cause = c->death_cause;
    gene_pack_v1(c, out->gene);         /* the new core fields (look_seed ..) start 0 */
    resident_sanitize(out);
}

void res_to_v1(const DoomoCitizen *c, uint8_t out96[96]) { memcpy(out96, c, 96); }
void res_from_v1(const uint8_t in96[96], DoomoCitizen *out) { memcpy(out, in96, 96); }
