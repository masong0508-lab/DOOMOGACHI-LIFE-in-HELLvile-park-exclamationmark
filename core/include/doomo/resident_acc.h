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

#endif
