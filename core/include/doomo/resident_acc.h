/* resident_acc.h - accessors for the DoomoResident members that move into the
 * 384-bit gene (customisation v2, P0.M0a "accessors first").
 *
 * Callers read and write a resident's look, voice, persona and identity
 * through these macros instead of naming the struct members. Today they sit
 * over the 96-byte v1 layout and cost nothing; the one patch that shrinks the
 * record to 76 bytes redefines them over gene_get/gene_set and nothing else
 * has to change. tools/repack_sites.py finds and converts the call sites.
 *
 *   RES_SPECIES(r)             the hot species byte
 *   RES_LOOK(r, GF_HAIR_COLOR) a look field by gene id (GF_BODY .. GF_PATTERN_COLOR);
 *                              hair_highlight reads LOOK_NO_COLOR for "none"
 *   RES_VOICE(r, i)  RES_PERSONA(r, i)
 *   RES_PRONOUN(r) RES_BDAY_MONTH(r) RES_BDAY_DAY(r) RES_LIKE_FOOD(r) RES_LIKE_HOBBY(r)
 *   RES_LIKE_MUSIC(r) RES_LIKE_COLOR(r) RES_HATE_FOOD(r) RES_FEAR(r) RES_CATCHPHRASE(r)
 *   RES_SET_<same>(r, ..., v)  assign, and evaluate to the stored value
 *
 * (`romance` keeps its own RES_ROMANCE / RES_STAGE macros in resident.h.)
 * The getters are rvalues: take no address of them and do not ++/--; use the
 * setter. The resident argument is evaluated once (twice in an `op=` rewrite),
 * so it must have no side effects.
 */
#ifndef DOOMO_RESIDENT_ACC_H
#define DOOMO_RESIDENT_ACC_H

#include "gene.h"

#define RES_SPECIES(r)          ((r)->look.species)
#define RES_SET_SPECIES(r, v)   ((r)->look.species = (uint8_t)(v))

/* GF_BODY + i is look byte 1 + i (gene.h). */
#define RES_LOOK(r, gf)         (((const uint8_t *)&(r)->look)[1 + (gf)])
#define RES_SET_LOOK(r, gf, v)  (((uint8_t *)&(r)->look)[1 + (gf)] = (uint8_t)(v))

/* Whole-record uses (v1 layout only: the flip replaces these with res_look() style copies).
 * RES_LOOK_PTR(r) is a DoomoLook pointer for read-only passes and copies. */
#define RES_LOOK_PTR(r)         (&(r)->look)
#define RES_VOICE_PTR(r)        ((r)->voice)
#define RES_PERSONA_PTR(r)      ((r)->persona)

#define RES_VOICE(r, i)         ((r)->voice[i])
#define RES_SET_VOICE(r, i, v)  ((r)->voice[i] = (uint8_t)(v))
#define RES_PERSONA(r, i)       ((r)->persona[i])
#define RES_SET_PERSONA(r, i, v) ((r)->persona[i] = (uint8_t)(v))

#define RES_PRONOUN(r)           ((r)->pronoun)
#define RES_SET_PRONOUN(r, v)    ((r)->pronoun = (uint8_t)(v))
#define RES_BDAY_MONTH(r)        ((r)->bday_month)
#define RES_SET_BDAY_MONTH(r, v) ((r)->bday_month = (uint8_t)(v))
#define RES_BDAY_DAY(r)          ((r)->bday_day)
#define RES_SET_BDAY_DAY(r, v)   ((r)->bday_day = (uint8_t)(v))
#define RES_LIKE_FOOD(r)         ((r)->like_food)
#define RES_SET_LIKE_FOOD(r, v)  ((r)->like_food = (uint8_t)(v))
#define RES_LIKE_HOBBY(r)        ((r)->like_hobby)
#define RES_SET_LIKE_HOBBY(r, v) ((r)->like_hobby = (uint8_t)(v))
#define RES_LIKE_MUSIC(r)        ((r)->like_music)
#define RES_SET_LIKE_MUSIC(r, v) ((r)->like_music = (uint8_t)(v))
#define RES_LIKE_COLOR(r)        ((r)->like_color)
#define RES_SET_LIKE_COLOR(r, v) ((r)->like_color = (uint8_t)(v))
#define RES_HATE_FOOD(r)         ((r)->hate_food)
#define RES_SET_HATE_FOOD(r, v)  ((r)->hate_food = (uint8_t)(v))
#define RES_FEAR(r)              ((r)->fear)
#define RES_SET_FEAR(r, v)       ((r)->fear = (uint8_t)(v))
#define RES_CATCHPHRASE(r)       ((r)->catchphrase)
#define RES_SET_CATCHPHRASE(r, v)((r)->catchphrase = (uint8_t)(v))

/* ---- field ids (creator rows, presets, tests) --------------------------------
 * A "field id" names one editable value of a resident without a byte offset:
 *   0 .. GF_COUNT-1     a gene field (gene.h): look, voice, persona, identity
 *   RES_F_SPECIES       the hot species byte
 *   RES_F_ROMANCE       the whole romance byte (preference + life stage bits)
 *   RES_F_FLAGS         RF_* flags
 *   RES_F_NAME          the name (text; no numeric value)
 * res_field_get / res_field_set read and write by id. The v1 bodies below go
 * through the struct; the flip rewrites them over gene_get / gene_set. */
enum { RES_F_SPECIES = 0x80, RES_F_ROMANCE, RES_F_FLAGS, RES_F_NAME };
#define RES_F_IS_LOOK(f)  ((f) == RES_F_SPECIES || (f) <= GF_PATTERN_COLOR)

static inline int res_field_get(const DoomoResident *r, int f)
{
    if (f == RES_F_SPECIES) return RES_SPECIES(r);
    if (f == RES_F_ROMANCE) return r->romance;
    if (f == RES_F_FLAGS)   return r->flags;
    if (f <= GF_PATTERN_COLOR) return RES_LOOK(r, f);
    if (f >= GF_VOICE0 && f < GF_VOICE0 + VOICE_COUNT) return RES_VOICE(r, f - GF_VOICE0);
    if (f >= GF_PERSONA0 && f < GF_PERSONA0 + PERS_COUNT) return RES_PERSONA(r, f - GF_PERSONA0);
    switch (f) {
    case GF_PRONOUN:     return RES_PRONOUN(r);
    case GF_BDAY_MONTH:  return RES_BDAY_MONTH(r);
    case GF_BDAY_DAY:    return RES_BDAY_DAY(r);
    case GF_LIKE_FOOD:   return RES_LIKE_FOOD(r);
    case GF_LIKE_HOBBY:  return RES_LIKE_HOBBY(r);
    case GF_LIKE_MUSIC:  return RES_LIKE_MUSIC(r);
    case GF_LIKE_COLOR:  return RES_LIKE_COLOR(r);
    case GF_HATE_FOOD:   return RES_HATE_FOOD(r);
    case GF_FEAR:        return RES_FEAR(r);
    case GF_CATCHPHRASE: return RES_CATCHPHRASE(r);
    default:             return 0;
    }
}

static inline void res_field_set(DoomoResident *r, int f, int v)
{
    if (f == RES_F_SPECIES) { RES_SET_SPECIES(r, v); return; }
    if (f == RES_F_ROMANCE) { r->romance = (uint8_t)v; return; }
    if (f == RES_F_FLAGS)   { r->flags = (uint8_t)v; return; }
    if (f <= GF_PATTERN_COLOR) { RES_SET_LOOK(r, f, v); return; }
    if (f >= GF_VOICE0 && f < GF_VOICE0 + VOICE_COUNT) { RES_SET_VOICE(r, f - GF_VOICE0, v); return; }
    if (f >= GF_PERSONA0 && f < GF_PERSONA0 + PERS_COUNT) { RES_SET_PERSONA(r, f - GF_PERSONA0, v); return; }
    switch (f) {
    case GF_PRONOUN:     RES_SET_PRONOUN(r, v); break;
    case GF_BDAY_MONTH:  RES_SET_BDAY_MONTH(r, v); break;
    case GF_BDAY_DAY:    RES_SET_BDAY_DAY(r, v); break;
    case GF_LIKE_FOOD:   RES_SET_LIKE_FOOD(r, v); break;
    case GF_LIKE_HOBBY:  RES_SET_LIKE_HOBBY(r, v); break;
    case GF_LIKE_MUSIC:  RES_SET_LIKE_MUSIC(r, v); break;
    case GF_LIKE_COLOR:  RES_SET_LIKE_COLOR(r, v); break;
    case GF_HATE_FOOD:   RES_SET_HATE_FOOD(r, v); break;
    case GF_FEAR:        RES_SET_FEAR(r, v); break;
    case GF_CATCHPHRASE: RES_SET_CATCHPHRASE(r, v); break;
    default: break;
    }
}

#endif
