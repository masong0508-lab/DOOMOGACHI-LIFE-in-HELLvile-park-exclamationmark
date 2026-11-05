/* parts.h - the part-script format (customisation v2, P1.M1; SPEC 4.4).
 *
 * tools/parts_build.py compiles the text files of core/data/parts/<dir>/ into core/gen/parts_<dir>.c;
 * the VM in src/portrait_script.c runs the bytes. A script entry belongs to one (list, value):
 * `list` is the GF_* id of the style field (e.g. GF_EYE_STYLE), `value` the choice. An entry is a
 * run of BLOCKS, each a PartHdr and `len` bytes of ops, closed by a header whose group is PH_END.
 *
 * Operands: coordinates are int8 in steps of 2 normalised units (head / torso space, 256 = one
 * radius, so -256..254); radii are uint8 in normalised units; colours are 0..4 (PMat c[]).
 * Ops are 1 + 0..7 operand bytes. */
#ifndef DOOMO_PARTS_H
#define DOOMO_PARTS_H

#include "types.h"

#define PH_END 0xFF

typedef struct PartHdr {
    uint8_t group;       /* DL group 0..6 the block belongs to (portrait.c), PH_END ends an entry   */
    uint8_t anchor;      /* PA_*                                                                     */
    uint8_t lod_views;   /* bits 0..2: LOD mask (1 << PHead.lod); bits 3..6: view mask (1 << VK_*)  */
    uint8_t len;         /* op bytes that follow                                                    */
} PartHdr;

#define PLV_LOD(m)   ((m) & 7)
#define PLV_VIEW(m)  (((m) >> 3) & 15)
#define PLV_ALL      0x7F

enum { PA_HEAD, PA_TORSO };

enum {                   /* opcode (+ operand bytes)                                                */
    PO_END,              /*                                                                         */
    PO_ANCHOR,           /* a                          switch anchor (PA_*)                         */
    PO_MAT,              /* gf tone                    material of colour field gf (pm_of)         */
    PO_SHADE,            /* mode                       SH_*                                         */
    PO_SOLID,            /* flags                      new solid part (PF_*)                        */
    PO_DECAL,            /*                            following shapes keep the ids underneath    */
    PO_ELLIPSE,          /* x y rx ry                                                               */
    PO_CAPSULE,          /* x0 y0 x1 y1 r0 r1                                                       */
    PO_TRI,              /* x0 y0 x1 y1 x2 y2                                                       */
    PO_LINE,             /* x0 y0 x1 y1 c                                                           */
    PO_DOT,              /* x y c                                                                   */
    PO_STAMP,            /* id x y q4                  drawn once (no mirror)                       */
    PO_MIRROR,           /* on                         shapes drawn on both sides of the centre    */
    PO_SCALE,            /* s                          coordinates and radii * s / 64 from here   */
    PO_REGION,           /* r                          decal clip: REG_*                            */
    PO_IFVIEW,           /* mask skip                  skip `skip` bytes unless view in mask        */
    PO_IFYAW,            /* mask skip                  yaw 0..4                                     */
    PO_IFANIM,           /* mask skip                  1 blink, 2 breath, 4 twitch, 8 wag           */
    PO_COUNT
};

typedef struct PartRef { uint8_t list, value; uint16_t ofs; } PartRef;   /* sorted by (list, value) */
typedef struct StampRef { uint8_t w, h; uint16_t ofs; } StampRef;        /* 1bpp rows, MSB first    */

/* PS1: the scripts are kept as LZ blocks (the save codec: c < 0x80 = c+1 literals, c >= 0x80 = copy
 * (c & 0x7F) + 3 bytes from the u16 LE distance after it), each at most PART_BLK raw bytes and ending
 * on an entry boundary, so one entry never straddles two blocks. rofs[b] is the raw offset block b
 * starts at (rofs[nblk] = the raw size); zofs[b] where its compressed bytes start in z. The raw
 * offsets in PartRef are the same either way. Built only with DOOMO_PARTS_LZ (PS1) or
 * DOOMO_PARTS_LZ_TEST (host tests, which check the blocks against the raw array). */
#define PART_BLK 2048
typedef struct PartLz {
    const uint8_t *z;
    const uint16_t *zofs;
    const uint16_t *rofs;
    uint16_t nblk;
} PartLz;

typedef struct PartSet {
    const PartRef *ref;  uint16_t n;
    const uint8_t *ops;  /* the blocks (NULL when only the LZ form is linked)                        */
    const StampRef *st;  uint8_t nst;
    const uint8_t *bits;
    const PartLz *lz;    /* the LZ form (DOOMO_PARTS_LZ builds), else NULL                           */
} PartSet;

/* The ops of the entry at raw offset `ofs`. With DOOMO_PARTS_LZ the block is decoded into a four-slot
 * (8 KB) LRU: the pointer stays valid until three other blocks have been fetched, which one entry's
 * run never does. */
#if defined(DOOMO_PARTS_LZ) || defined(DOOMO_PARTS_LZ_TEST)
const uint8_t *part_lz_ops(const PartLz *z, unsigned ofs);
extern const PartLz doomo_parts_face_lz, doomo_parts_head_lz;
uint32_t part_lru_loads(void);       /* blocks decoded since the last reset (tests, bench) */
void part_lru_reset(void);
#endif
#if defined(DOOMO_PARTS_LZ)
#define PART_OPS(set, ofs) part_lz_ops((set)->lz, (ofs))
#else
#define PART_OPS(set, ofs) ((set)->ops + (ofs))
#endif

extern const PartSet doomo_parts_face;
extern const PartSet doomo_parts_head;

#endif
