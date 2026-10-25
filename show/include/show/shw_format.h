/* shw_format.h - the EP###.SHW binary format (spec 7.3). Owner: ENGINE lane 1.
 *
 * Mirrored field for field by show/tools/showtk/shw_format.py (the compiler
 * writes it, tests/compiler checks the two agree, AT4.2). Little-endian,
 * every struct and record 4-byte aligned. ADD-ONLY: never move or resize a
 * field; bump SHW_ABI only for an incompatible change and update every
 * user on origin in the same push.
 *
 * File layout
 *   ShwEpHeader                    at 0                         (64 B)
 *   ShwShotIndex[nshots]           at index_off                 (48 B each)
 *   strtab                         at strtab_off (episode strings, NUL-terminated)
 *   shot chunks                    each at chunk_sector * 2048 (sector aligned,
 *                                  so the disc loader reads whole sectors)
 *
 * A shot chunk is a ShwShotHdr followed by `nrec` RECORDS. A record is an
 * 8-byte ShwRec header, then `bytes` of payload, then zero padding to a
 * multiple of 4. Record types are allocated below, one line per package;
 * the player skips types it does not know, so a package can add a record
 * type without touching anyone else's code. A host finds its records with
 * show_chunk_find() (show_api.h).
 *
 * Shots that start from another shot's state (base= / replay=, spec 5.4) are
 * resolved by the COMPILER into a self-contained chunk plus `preroll`: the
 * player runs that chunk's logic for `preroll` frames with drawing skipped
 * before frame 0 is shown (DECISIONS.md "WP2 base/replay").
 */
#ifndef SHOW_SHW_FORMAT_H
#define SHOW_SHW_FORMAT_H

#include <stdint.h>

#define SHW_MAGIC        0x31574853u    /* 'SHW1' as read little-endian    */
#define SHW_SHOT_MAGIC   0x544f4853u    /* 'SHOT'                          */
#define SHW_ABI          1
#define SHW_SECTOR       2048
#define SHW_CHUNK_MAX    (160 * 1024)   /* spec 7.3: a shot chunk <= 160 KB */
#define SHW_ID_LEN       8              /* shot id "co01", NUL-padded       */

typedef struct ShwEpHeader {            /* 64 B */
    uint32_t magic;                     /* SHW_MAGIC                         */
    uint16_t abi;                       /* SHW_ABI                           */
    uint16_t nshots;
    uint8_t  fps;                       /* 30                                */
    uint8_t  px_canvas;                 /* 1 (reserved for the wide hatch)   */
    uint16_t canvas_w, canvas_h, y_off; /* 320, 180, 30                      */
    uint32_t seed;                      /* episode seed                      */
    uint16_t episode;                   /* 101                               */
    uint16_t pad;
    uint8_t  toolchain_hash[8];         /* showc + format, first 8 B of sha256 */
    uint32_t index_off;                 /* byte offset of ShwShotIndex[0]    */
    uint32_t strtab_off, strtab_bytes;
    uint32_t total_frames;              /* sum of len                        */
    uint32_t file_bytes;
    uint32_t reserved[3];
} ShwEpHeader;

enum {                                  /* ShwShotIndex.flags                */
    SHWF_SLUG     = 1,                  /* a slug card (show slug)           */
    SHWF_HOLDLAST = 2,                  /* reserved                          */
    SHWF_3D       = 4                   /* uses VIEW3D/OVERWORLD/ROOM        */
};

typedef struct ShwShotIndex {           /* 48 B */
    uint32_t start_frame;               /* episode frame of the shot's frame 0 */
    uint16_t len;                       /* frames                            */
    uint8_t  px;                        /* 1|2|3 (spec 2.3)                  */
    uint8_t  look;                      /* look register (0 = none)          */
    uint32_t chunk_sector;              /* chunk at chunk_sector * SHW_SECTOR */
    uint32_t chunk_bytes;
    uint16_t base_shot, base_frame;     /* provenance only (0xFFFF = none);   */
    uint16_t replay_shot, replay_from;  /*   the chunk is already resolved    */
    uint16_t flags;                     /* SHWF_*                            */
    uint16_t preroll;                   /* logic frames run, undrawn, before 0 */
    uint32_t seed;                      /* shot seed (RNG = ep seed ^ this)   */
    char     id[SHW_ID_LEN];            /* "co01"                            */
    uint32_t beat;                      /* rundown beat number (logs, slugs)  */
    uint32_t reserved;
} ShwShotIndex;

typedef struct ShwShotHdr {             /* 32 B, at the start of a chunk     */
    uint32_t magic;                     /* SHW_SHOT_MAGIC                    */
    uint16_t nrec;                      /* records that follow               */
    uint16_t shot;                      /* its index (a check)               */
    uint32_t bytes;                     /* whole chunk incl. this header     */
    uint8_t  set_kind;                  /* SHWP_* of the main set plane      */
    uint8_t  badge_est;                 /* SHWB_*                            */
    uint8_t  bg;                        /* palette index the frame clears to */
    uint8_t  nlabels;                   /* beat labels used (BeatScript)     */
    uint16_t vram_plan[4];              /* set-pool KB, image-pool blocks, -, - */
    uint32_t content_crc;               /* CRC32 of the records (cache key input) */
    uint32_t reserved;
} ShwShotHdr;

enum { SHWB_PS1_LEGAL = 0, SHWB_EMU_ONLY = 1 };

typedef struct ShwRec {                 /* 8 B, then `bytes` of payload      */
    uint16_t type;                      /* SHWR_*                            */
    uint16_t n;                         /* items in the payload (type-defined) */
    uint32_t bytes;                     /* payload bytes (excl. this header and padding) */
} ShwRec;

/* ---- record types: ADD-ONLY, one line per package ----------------------- *
 * Payload layouts of the lane-1 types are below; the other packages document
 * theirs in their own headers (named in the comment).                        */
enum {
    SHWR_NONE      = 0,
    /* lane 1: player core (show_api.h) */
    SHWR_LOOK      = 1,     /* DoomoLook[n] (48 B each; show_cast.c fills doomo_town.res[]) */
    SHWR_PLANE     = 2,     /* n planes: each a ShwPlane + kind payload, ShwPlane.bytes long */
    SHWR_BEAT      = 3,     /* BeatEv[n], sorted by (anchor, at)                 */
    SHWR_ARGS      = 4,     /* int32_t[n]: show-op argument table (BeatEv.b indexes it) */
    SHWR_STRINGS   = 5,     /* bytes of NUL-terminated strings; referenced by byte offset */
    SHWR_IMAGE     = 6,     /* n images: each ShwImage + pixels, ShwImage.bytes long */
    SHWR_SLUG      = 7,     /* ShwSlug (one)                                     */
    SHWR_AUDIO_K   = 8,     /* ShwAudioK[n]: audio events k due in this shot (P4) */
    SHWR_CLUT      = 9,     /* uint16_t[n] BGR555 (reserved)                     */
    /* lane 2: stage FX / text / camera (show_cam.h, show_fx.h, show_text.h) */
    SHWR_CAMSETUP  = 16,    /* ShowCamSetup (show_cam.h)                         */
    SHWR_CAMKEY    = 17,    /* ShowCamKey[n] (show_cam.h)                        */
    SHWR_STROKES   = 18,    /* show_strokes                                      */
    SHWR_TILEMAP   = 19,    /* show_tilemap                                      */
    SHWR_TEXT      = 20,    /* SUPERs, signs, cards (show_text.h)                */
    SHWR_FX        = 21,    /* particles, wipes, Split, dissolve (show_fx.h)     */
    SHWR_FONT      = 22,    /* .pxt glyph sets (show_text.h)                     */
    /* lane 2: actors and crowds (show_actor.h, show_crowd.h) */
    SHWR_ACTOR     = 32,    /* actors                                            */
    SHWR_TRACK     = 33,    /* per-frame MOUTH/EXPR/EYES/BLINK tracks (RLE)      */
    SHWR_CROWD     = 34,    /* crowds                                            */
    SHWR_PROP      = 35,    /* props / flipbooks                                 */
    /* lane 3: 3D and map hosts, HUD (show_hosts.h) */
    SHWR_OVERWORLD = 48,
    SHWR_ROOM      = 49,
    SHWR_VIEW3D    = 50,
    SHWR_HUD       = 51,
    SHWR_RAIL      = 52,
    /* private ranges for anything not foreseen: 0x100-0x1FF lane 2,
     * 0x200-0x2FF lane 3, 0x300-0x3FF lane 1. Promote to a named line when stable. */
    SHWR_PRIV_L2   = 0x100,
    SHWR_PRIV_L3   = 0x200,
    SHWR_PRIV_L1   = 0x300
};

/* ---- planes (SHWR_PLANE): drawn back to front by `order` ----------------- */
enum {                                  /* ShwPlane.kind: ADD-ONLY           */
    SHWP_NONE = 0,
    SHWP_BANDS = 1,                     /* lane 1: palette-step gradient (TILE bands) */
    SHWP_IMAGE = 2,                     /* lane 1: a baked plate (SHWR_IMAGE)  */
    SHWP_TILEMAP = 3,                   /* lane 2 */
    SHWP_STROKES = 4,                   /* lane 2 */
    SHWP_GRAIN = 5,                     /* lane 1: 64x64 two-tone paper tile   */
    SHWP_VIEW3D = 6,                    /* lane 3 */
    SHWP_OVERWORLD = 7,                 /* lane 3 */
    SHWP_ROOM = 8,                      /* lane 3 */
    SHWP_FILL = 9,                      /* lane 1: one flat colour             */
    SHWP_COUNT = 16                     /* table size; kinds < 16              */
};

enum {                                  /* ShwPlane.flags                    */
    SHWPF_NOSHAKE = 1,                  /* ignores the camera shake          */
    SHWPF_HIDDEN  = 2,                  /* starts hidden (a show op shows it) */
    SHWPF_TILE_X  = 4,                  /* repeats horizontally              */
    SHWPF_TILE_Y  = 8                   /* repeats vertically                */
};

typedef struct ShwPlane {               /* 24 B common header, then the kind payload */
    uint8_t  kind;                      /* SHWP_*                            */
    uint8_t  flags;                     /* SHWPF_*                           */
    uint16_t bytes;                     /* this header + payload (4-aligned)  */
    int16_t  order;                     /* draw order, lower first           */
    uint16_t id;                        /* plane id within the shot (show ops address it) */
    /* the camera fields: the same order as show_cam.h's ShowPlaneCam */
    int16_t  par_xn, par_xd;            /* x parallax = par_xn / par_xd (den > 0) */
    int16_t  par_yn, par_yd;
    int16_t  at_x, at_y;                /* plane origin, show px             */
    uint16_t spar_q8;                   /* scale parallax, Q8 (256 = with the camera) */
    uint16_t t_in;                      /* reserved (0)                      */
} ShwPlane;

typedef struct ShwBand { int16_t y; uint8_t color, pad; } ShwBand;
typedef struct ShwBands {               /* SHWP_BANDS payload                */
    uint16_t n;                         /* bands that follow                 */
    uint16_t pad;
    /* ShwBand band[n]: band i covers show y [band[i].y, band[i+1].y)
     * (the last to the picture bottom), in plane px (moves with the plane) */
} ShwBands;

typedef struct ShwFill { uint8_t color, pad[3]; } ShwFill;   /* SHWP_FILL payload */

typedef struct ShwImagePlane {          /* SHWP_IMAGE payload                */
    uint16_t image;                     /* index into the shot's SHWR_IMAGE list */
    uint8_t  scale;                     /* 0 = the shot's px, else 1..4      */
    uint8_t  flip;                      /* IMG_FLIPX                         */
} ShwImagePlane;

typedef struct ShwGrain {               /* SHWP_GRAIN payload                */
    uint8_t  color;                     /* the grain colour                  */
    uint8_t  density;                   /* dots per 64x64 tile / 16 (1..255) */
    uint8_t  boil;                      /* 0 = still; N = new seed every N frames */
    uint8_t  pad;
    uint32_t seed;
} ShwGrain;

/* ---- images (SHWR_IMAGE): 8bpp indexed, index 0 transparent -------------- */
enum { SHWI_RAW = 0, SHWI_RLE = 1 };    /* ShwImage.comp                     */
typedef struct ShwImage {               /* 12 B, then the pixel data         */
    uint16_t w, h;
    uint8_t  comp;                      /* SHWI_RAW: w*h bytes. SHWI_RLE: pairs
                                           (count 1..255, index), row-major  */
    uint8_t  pad;
    uint16_t id;                        /* the compiler's image id (logs)    */
    uint32_t bytes;                     /* this header + data (4-aligned)    */
} ShwImage;

/* ---- slug card (SHWR_SLUG) ----------------------------------------------- */
typedef struct ShwSlug {                /* 16 B; strings in SHWR_STRINGS     */
    uint16_t title;                     /* string offsets                    */
    uint16_t line1, line2;
    uint8_t  bg, fg;                    /* palette indices                   */
    uint32_t beat;                      /* beat number                       */
    uint16_t act;                       /* act number (0 cold open)          */
    uint16_t pad;
} ShwSlug;

/* ---- audio events due in the shot (SHWR_AUDIO_K), for the P4 log --------- */
typedef struct ShwAudioK { uint16_t frame; uint16_t pad; uint32_t k; } ShwAudioK;

/* ---- show ops: BeatEv { op = BOP_SHOT, a = SHOW_OP_FLAG | op, b = ArgTable index }
 * Ranges (ADD-ONLY inside your range; a module's ext hook claims its range):
 *   0x000-0x03F lane 1 core        0x040-0x05F lane 2 text
 *   0x060-0x07F lane 2 fx          0x080-0x09F lane 2 camera
 *   0x0A0-0x0BF lane 2 actors      0x0C0-0x0DF lane 2 crowds/props
 *   0x100-0x17F lane 3 hosts/HUD   0x180-0x1FF free                       */
#define SHOW_OP_FLAG    0x4000
enum {
    SOP_NOP        = 0x000,
    SOP_PLANE_SHOW = 0x001,             /* args[b]: plane id                 */
    SOP_PLANE_HIDE = 0x002,             /* args[b]: plane id                 */
    SOP_MARK       = 0x003,             /* args[b]: marker id -> P1 (tests)  */
    SOP_FADE_TO    = 0x004,             /* args[b..]: level16, frames        */
    SOP_PAL_TO     = 0x005,             /* args[b..]: pal_id, level16, frames */
    SOP_IRIS       = 0x006,             /* args[b..]: out(1)/in(0), cx, cy, r, heart, hold */
    SOP_TEXT_BASE  = 0x040, SOP_FX_BASE = 0x060, SOP_CAM_BASE = 0x080,
    SOP_ACTOR_BASE = 0x0A0, SOP_CROWD_BASE = 0x0C0, SOP_HOST_BASE = 0x100
};

#endif
