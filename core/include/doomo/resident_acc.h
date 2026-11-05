/* resident_acc.h - accessors for the DoomoResident members that live in the 384-bit gene
 * (customisation v2, P0.M0b-1: the 96 -> 76 B flip).
 *
 * The look (except the hot `species` byte), voice, persona and identity are bit-packed in
 * r->gene and have no address. Callers read and write them through these macros and functions:
 *
 *   RES_SPECIES(r)             the hot species byte
 *   RES_LOOK(r, GF_HAIR_COLOR) a look field by gene id (GF_BODY .. GF_PATTERN_COLOR);
 *                              hair_highlight reads LOOK_NO_COLOR for "none"
 *   RES_VOICE(r, i)  RES_PERSONA(r, i)
 *   RES_PRONOUN(r) RES_BDAY_MONTH(r) RES_BDAY_DAY(r) RES_LIKE_FOOD(r) RES_LIKE_HOBBY(r)
 *   RES_LIKE_MUSIC(r) RES_LIKE_COLOR(r) RES_HATE_FOOD(r) RES_FEAR(r) RES_CATCHPHRASE(r)
 *   RES_ROMANCE(r)             the ROMANCE_* preference (gene GF_ROMANCE)
 *   RES_STAGE(r)               the LIFE_* stage (gene GF_AGE)
 *   RES_SET_<same>(r, ..., v)  assign (a value over the field's width clamps), and evaluate to
 *                              the stored value
 * The getters are rvalues: take no address of them and do not ++/--; use the setter. Every
 * argument is evaluated once. The gene is read byte-wise (gene.h), so a record at any address
 * is safe on ARM7TDMI and R3000.
 *
 * Whole-record uses copy OUT, never point INTO the record:
 *   res_get_look(r, &l)      copy the wide look out         res_set_look(r, &l)   store one back
 *   res_get_voice(r, v5)     VOICE_COUNT bytes out          res_set_voice(r, v5)
 *   res_get_persona(r, p)    PERS_COUNT bytes out           res_set_persona(r, p)
 *   RES_LOOK_VIEW(r)         const DoomoLook * to a temporary copy (a C99 compound literal): valid
 *   RES_VOICE_VIEW(r)        to the end of the enclosing block, so use it as a call argument or in
 *   RES_PERSONA_VIEW(r)      one statement, never keep it in a longer-lived pointer or return it.
 * A function that must hand a pointer back keeps its own static or caller-supplied buffer.
 *
 * res_unpack / res_pack move a whole record to and from the DoomoCitizen working struct
 * (res_pack sanitizes). res_to_v1 / res_from_v1 are the 96 B v1 image (saves, goldens). */
#ifndef DOOMO_RESIDENT_ACC_H
#define DOOMO_RESIDENT_ACC_H

#include <string.h>
#include "gene.h"

#define RES_SPECIES(r)          ((r)->species)
#define RES_SET_SPECIES(r, v)   ((r)->species = (uint8_t)(v))

static inline uint32_t res_gget(const DoomoResident *r, int gf) { return gene_get(r->gene, gf); }
static inline uint8_t res_gset(DoomoResident *r, int gf, uint32_t v)
{
    gene_set(r->gene, gf, v);
    return (uint8_t)gene_get(r->gene, gf);
}

/* the gene stores hair_highlight's "none" (LOOK_NO_COLOR 0xFF) as 32 (CF-3) */
static inline uint8_t res_look_get(const DoomoResident *r, int gf)
{
    uint32_t v = gene_get(r->gene, gf);
    return (uint8_t)(gf == GF_HAIR_HIGHLIGHT && v == 32 ? LOOK_NO_COLOR : v);
}
static inline uint8_t res_look_set(DoomoResident *r, int gf, uint32_t v)
{
    gene_set(r->gene, gf, gf == GF_HAIR_HIGHLIGHT && v == LOOK_NO_COLOR ? 32u : v);
    return res_look_get(r, gf);
}
#define RES_LOOK(r, gf)         res_look_get((r), (gf))
#define RES_SET_LOOK(r, gf, v)  res_look_set((r), (gf), (uint32_t)(uint8_t)(v))

#define RES_VOICE(r, i)         ((uint8_t)res_gget((r), GF_VOICE0 + (i)))
#define RES_SET_VOICE(r, i, v)  res_gset((r), GF_VOICE0 + (i), (uint32_t)(uint8_t)(v))
#define RES_PERSONA(r, i)       ((uint8_t)res_gget((r), GF_PERSONA0 + (i)))
#define RES_SET_PERSONA(r, i, v) res_gset((r), GF_PERSONA0 + (i), (uint32_t)(uint8_t)(v))

#define RES_PRONOUN(r)           ((uint8_t)res_gget((r), GF_PRONOUN))
#define RES_SET_PRONOUN(r, v)    res_gset((r), GF_PRONOUN, (uint32_t)(uint8_t)(v))
#define RES_BDAY_MONTH(r)        ((uint8_t)res_gget((r), GF_BDAY_MONTH))
#define RES_SET_BDAY_MONTH(r, v) res_gset((r), GF_BDAY_MONTH, (uint32_t)(uint8_t)(v))
#define RES_BDAY_DAY(r)          ((uint8_t)res_gget((r), GF_BDAY_DAY))
#define RES_SET_BDAY_DAY(r, v)   res_gset((r), GF_BDAY_DAY, (uint32_t)(uint8_t)(v))
#define RES_LIKE_FOOD(r)         ((uint8_t)res_gget((r), GF_LIKE_FOOD))
#define RES_SET_LIKE_FOOD(r, v)  res_gset((r), GF_LIKE_FOOD, (uint32_t)(uint8_t)(v))
#define RES_LIKE_HOBBY(r)        ((uint8_t)res_gget((r), GF_LIKE_HOBBY))
#define RES_SET_LIKE_HOBBY(r, v) res_gset((r), GF_LIKE_HOBBY, (uint32_t)(uint8_t)(v))
#define RES_LIKE_MUSIC(r)        ((uint8_t)res_gget((r), GF_LIKE_MUSIC))
#define RES_SET_LIKE_MUSIC(r, v) res_gset((r), GF_LIKE_MUSIC, (uint32_t)(uint8_t)(v))
#define RES_LIKE_COLOR(r)        ((uint8_t)res_gget((r), GF_LIKE_COLOR))
#define RES_SET_LIKE_COLOR(r, v) res_gset((r), GF_LIKE_COLOR, (uint32_t)(uint8_t)(v))
#define RES_HATE_FOOD(r)         ((uint8_t)res_gget((r), GF_HATE_FOOD))
#define RES_SET_HATE_FOOD(r, v)  res_gset((r), GF_HATE_FOOD, (uint32_t)(uint8_t)(v))
#define RES_FEAR(r)              ((uint8_t)res_gget((r), GF_FEAR))
#define RES_SET_FEAR(r, v)       res_gset((r), GF_FEAR, (uint32_t)(uint8_t)(v))
#define RES_CATCHPHRASE(r)       ((uint8_t)res_gget((r), GF_CATCHPHRASE))
#define RES_SET_CATCHPHRASE(r, v) res_gset((r), GF_CATCHPHRASE, (uint32_t)(uint8_t)(v))

/* romance preference (ROMANCE_*, bits 0..2 of the v1 byte) and life stage (LIFE_*, bits 4..6) */
#define RES_ROMANCE(r)           ((int)res_gget((r), GF_ROMANCE))
#define RES_SET_ROMANCE(r, v)    res_gset((r), GF_ROMANCE, (uint32_t)(v))
#define RES_STAGE(r)             ((int)res_gget((r), GF_AGE))
#define RES_SET_STAGE(r, v)      res_gset((r), GF_AGE, (uint32_t)(v))

/* ---- whole-record copies (resident.c / gene.c) ---------------------------------- */
void res_get_look(const DoomoResident *r, DoomoLook *out);
void res_set_look(DoomoResident *r, const DoomoLook *l);
void res_get_voice(const DoomoResident *r, uint8_t *out);      /* VOICE_COUNT bytes */
void res_set_voice(DoomoResident *r, const uint8_t *v);
void res_get_persona(const DoomoResident *r, uint8_t *out);    /* PERS_COUNT bytes */
void res_set_persona(DoomoResident *r, const uint8_t *p);

static inline const DoomoLook *res_look_view_(const DoomoResident *r, DoomoLook *tmp)
{ res_get_look(r, tmp); return tmp; }
static inline const uint8_t *res_voice_view_(const DoomoResident *r, uint8_t *tmp)
{ res_get_voice(r, tmp); return tmp; }
static inline const uint8_t *res_persona_view_(const DoomoResident *r, uint8_t *tmp)
{ res_get_persona(r, tmp); return tmp; }
#define RES_LOOK_VIEW(r)     res_look_view_((r), &(DoomoLook){0})
#define RES_VOICE_VIEW(r)    res_voice_view_((r), (uint8_t[VOICE_COUNT]){0})
#define RES_PERSONA_VIEW(r)  res_persona_view_((r), (uint8_t[PERS_COUNT]){0})

/* the working struct <-> the stored record (SPEC 1.2) */
void res_unpack(const DoomoResident *r, DoomoCitizen *out);
void res_pack(const DoomoCitizen *c, DoomoResident *out);      /* sanitizes */
void res_look(const DoomoResident *r, DoomoLook *out);         /* = res_get_look */
/* the 96 B v1 image: saves, goldens (a memcpy of the v1-layout DoomoCitizen) */
void res_to_v1(const DoomoCitizen *c, uint8_t out96[96]);
void res_from_v1(const uint8_t in96[96], DoomoCitizen *out);

/* ---- field ids (creator rows, presets, tests) ----------------------------------
 * A "field id" names one editable value of a resident without a byte offset:
 *   0 .. GF_COUNT-1     a gene field (gene.h): look, voice, persona, identity
 *   RES_F_SPECIES       the hot species byte
 *   RES_F_ROMANCE       the whole v1 romance byte (preference bits 0..2 + life stage bits 4..6)
 *   RES_F_FLAGS         RF_* flags
 *   RES_F_NAME          the name (text; no numeric value) */
enum { RES_F_SPECIES = 0x80, RES_F_ROMANCE, RES_F_FLAGS, RES_F_NAME };
#define RES_F_IS_LOOK(f)  ((f) == RES_F_SPECIES || (f) <= GF_PATTERN_COLOR)

static inline int res_field_get(const DoomoResident *r, int f)
{
    if (f == RES_F_SPECIES) return RES_SPECIES(r);
    if (f == RES_F_ROMANCE)
        return (int)(RES_ROMANCE(r) | ((unsigned)RES_STAGE(r) << LIFE_STAGE_SHIFT));
    if (f == RES_F_FLAGS)   return r->flags;
    if (f <= GF_PATTERN_COLOR) return RES_LOOK(r, f);
    if (f >= GF_VOICE0 && f < GF_VOICE0 + VOICE_COUNT) return RES_VOICE(r, f - GF_VOICE0);
    if (f >= GF_PERSONA0 && f < GF_PERSONA0 + PERS_COUNT) return RES_PERSONA(r, f - GF_PERSONA0);
    switch (f) {
    case GF_PRONOUN:     return RES_PRONOUN(r);
    case GF_ROMANCE:     return RES_ROMANCE(r);
    case GF_BDAY_MONTH:  return RES_BDAY_MONTH(r);
    case GF_BDAY_DAY:    return RES_BDAY_DAY(r);
    case GF_LIKE_FOOD:   return RES_LIKE_FOOD(r);
    case GF_LIKE_HOBBY:  return RES_LIKE_HOBBY(r);
    case GF_LIKE_MUSIC:  return RES_LIKE_MUSIC(r);
    case GF_LIKE_COLOR:  return RES_LIKE_COLOR(r);
    case GF_HATE_FOOD:   return RES_HATE_FOOD(r);
    case GF_FEAR:        return RES_FEAR(r);
    case GF_CATCHPHRASE: return RES_CATCHPHRASE(r);
    case GF_AGE:         return RES_STAGE(r);
    default:             return 0;
    }
}

static inline void res_field_set(DoomoResident *r, int f, int v)
{
    if (f == RES_F_SPECIES) { RES_SET_SPECIES(r, v); return; }
    if (f == RES_F_ROMANCE) {
        RES_SET_ROMANCE(r, (unsigned)v & ROMANCE_PREF_MASK);
        RES_SET_STAGE(r, ((unsigned)v & LIFE_STAGE_MASK) >> LIFE_STAGE_SHIFT);
        return;
    }
    if (f == RES_F_FLAGS)   { r->flags = (uint8_t)v; return; }
    if (f <= GF_PATTERN_COLOR) { RES_SET_LOOK(r, f, v); return; }
    if (f >= GF_VOICE0 && f < GF_VOICE0 + VOICE_COUNT) { RES_SET_VOICE(r, f - GF_VOICE0, v); return; }
    if (f >= GF_PERSONA0 && f < GF_PERSONA0 + PERS_COUNT) { RES_SET_PERSONA(r, f - GF_PERSONA0, v); return; }
    switch (f) {
    case GF_PRONOUN:     RES_SET_PRONOUN(r, v); break;
    case GF_ROMANCE:     RES_SET_ROMANCE(r, v); break;
    case GF_BDAY_MONTH:  RES_SET_BDAY_MONTH(r, v); break;
    case GF_BDAY_DAY:    RES_SET_BDAY_DAY(r, v); break;
    case GF_LIKE_FOOD:   RES_SET_LIKE_FOOD(r, v); break;
    case GF_LIKE_HOBBY:  RES_SET_LIKE_HOBBY(r, v); break;
    case GF_LIKE_MUSIC:  RES_SET_LIKE_MUSIC(r, v); break;
    case GF_LIKE_COLOR:  RES_SET_LIKE_COLOR(r, v); break;
    case GF_HATE_FOOD:   RES_SET_HATE_FOOD(r, v); break;
    case GF_FEAR:        RES_SET_FEAR(r, v); break;
    case GF_CATCHPHRASE: RES_SET_CATCHPHRASE(r, v); break;
    case GF_AGE:         RES_SET_STAGE(r, v); break;
    default: break;
    }
}

#endif
