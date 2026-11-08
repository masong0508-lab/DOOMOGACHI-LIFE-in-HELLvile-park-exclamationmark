/* parts.h - the part-script format (customisation v2, P1.M1; SPEC 4.4; CR2 many-part support).
 *
 * tools/parts_build.py compiles the text files of core/data/parts/<dir>/ (one dir per slot or group
 * of slots, any number of dirs) into core/gen/parts_<dir>.c and the registry core/gen/parts_index.c;
 * the VM in src/portrait_script.c runs the bytes. A script entry belongs to one (list, value): `list`
 * is a part list id (part_meta.h: a GF_* id, or PL_DEEP0 + DF_x), `value` the choice. An entry is a
 * run of BLOCKS, each a PartHdr and `len` bytes of ops, closed by a header whose group is PH_END.
 * The painters run a worn value's script in place of their own drawing (portrait_script.c,
 * "slot hooks"); the build checks every block's group against the hooks of its list.
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
    /* CR2 macro ops: the shape grammar of the part families (hats, tops, neckwear, jewellery ...),
     * each one op instead of a handful of primitives. They draw with the current material, shade
     * and solid / decal mode; x mirrors with `mirror on`. */
    PO_CROWN,            /* kind x y w h               hat crown: PCROWN_* standing on y            */
    PO_BRIM,             /* kind x y w h               brim at y: PBRIM_*                           */
    PO_BAND,             /* x y w h                    a band across (hat band, belt): own part     */
    PO_STRAP,            /* x0 y0 x1 y1 w              a strap with a darker edge line              */
    PO_BOW,              /* x y s                      a bow: two loops and a knot                  */
    PO_RING,             /* x y r t                    a hoop of thickness t (earring, halo, cuff)  */
    PO_CHAIN,            /* x0 y0 x1 y1 n sag          n links along a sagging line                 */
    PO_FRILL,            /* x0 x1 y n r                n scallops along y (ruff, hem, crown points) */
    PO_POUCH,            /* x y w h                    a pouch with a flap line and a button        */
    PO_COUNT
};

enum { PCROWN_DOME, PCROWN_CONE, PCROWN_CYLINDER, PCROWN_FLAT, PCROWN_SLOUCH, PCROWN_PEAKED,
       PCROWN_BERET, PCROWN_COUNT };
enum { PBRIM_NONE, PBRIM_BILL, PBRIM_SMALL, PBRIM_WIDE, PBRIM_FLOPPY, PBRIM_COCKED, PBRIM_COUNT };

/* PO_SOLID flag: keep the current part id (pc_same), e.g. a head shape drawn on the head part */
#define PSF_SAME 0x80

/* One script entry of a set: refs are sorted by (list, value) and grouped into PartList ranges.
 * `top` is how far the entry reaches above the head centre (head units / 8; 0 for torso parts), so
 * the bust can make room for a tall hat (ph_top_extent). */
typedef struct PartRef { uint8_t value, top; uint16_t ofs; } PartRef;
typedef struct PartList { uint16_t list, first, n; } PartList;           /* refs[first .. first+n) */
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
    const PartList *lists; uint8_t nlists;
} PartSet;

/* The registry (core/gen/parts_index.c): every set, and per list id the set that holds it (a list's
 * scripts all live in one dir), sorted by list. doomo_part_minv[gf] is the smallest scripted value
 * of gene list gf (0xFF: none), so a painter's hook costs one compare for an unscripted look. */
typedef struct PartListSet { uint16_t list; uint8_t set, minv; } PartListSet;
extern const PartSet *const doomo_part_scripts[];
extern const uint8_t doomo_part_script_count;
extern const PartListSet doomo_part_script_lists[];
extern const uint8_t doomo_part_script_nlists;
extern const uint8_t doomo_part_minv[128];

/* The ops of the entry at raw offset `ofs`. With DOOMO_PARTS_LZ the block is decoded into a two-slot
 * (4 KB) LRU and the ENTRY is copied into a 4 KB per-render part arena (CR2): each worn entry is
 * decoded once per render, whatever the number of layer groups and hooks that run it. The arena is
 * reset by part_arena_begin() (pctx_init); a pointer stays valid until the next reset, or, for an
 * entry that does not fit the arena, until another block is fetched (one entry's run never does).
 * The compressed bytes come through part_lz_src(): ROM / RAM by default; a PS1 build that keeps the
 * blocks in SPU RAM (DOOMO_PARTS_SPU, CR10) provides its own (DMA into `stage`). */
#if defined(DOOMO_PARTS_LZ) || defined(DOOMO_PARTS_LZ_TEST)
#define PART_SLOTS 2
#define PART_ARENA 4096
const uint8_t *part_lz_ops(const PartLz *z, unsigned ofs);
const uint8_t *part_arena_ops(const PartLz *z, unsigned ofs);
void part_arena_begin(void);
const uint8_t *part_lz_src(const PartLz *z, unsigned blk, uint8_t *stage, unsigned cap);
uint32_t part_lru_loads(void);       /* blocks decoded since the last reset (tests, bench) */
uint32_t part_arena_fills(void);     /* entries copied into the arena since the last reset    */
void part_lru_reset(void);
#endif
#if defined(DOOMO_PARTS_LZ)
#define PART_OPS(set, ofs) part_arena_ops((set)->lz, (ofs))
#else
#define PART_OPS(set, ofs) ((set)->ops + (ofs))
#endif

/* each dir's set is doomo_parts_<dir> (and doomo_parts_<dir>_lz): see core/gen/parts_index.c */
extern const PartSet doomo_parts_face;
extern const PartSet doomo_parts_head;

#endif
