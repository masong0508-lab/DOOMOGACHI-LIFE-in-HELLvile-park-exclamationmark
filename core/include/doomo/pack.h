/* pack.h - the user content pack format (DMPK v1), the mature scene slot
 * numbers and the pack API (owner: E0 contract, LIFESIM_PLAN E0, E2, E7;
 * DECISIONS "MATURE ART OVERRIDE", "RESERVED SLOTS"; add-only after E0).
 *
 * A content pack is user-supplied: art and scene scripts that fill the
 * mature scene slots (SCENE_MSLOT, E2) when OPT_MATURE is "On + pack".
 * It is built and validated by tools/mpack/mpack.py (E7) from this
 * header's constants, read by core/src/pack.c (E2), and reaches the core
 * through the platform hooks plat_pack_blob / plat_pack_read (plat_ext.h):
 * a GBA ROM blob (E8), a PS1/DC disc file (E9, E10), a native file (E2).
 * Custom art shows ONLY inside a mature context (answer 22); a pack's
 * speakers and subjects must pass mature_adult_ok() (ending.h). The
 * repository and the tests carry only the SFW example pack (E7); the game
 * never authors explicit content.
 *
 * Until E2 lands, the API is weak in core/src/weak_endings.c: no pack
 * (pack_open() returns 0), so every slot plays its fade-to-black.
 *
 * THE FILE (little endian; offsets from the pack start; 4-aligned)
 *    0  DoomoPackHeader (64 B)
 *   64  DoomoPackEntry[n_entries] (16 B each), sorted by uid (pack_art
 *       binary-searches it)
 *  ...  entry payloads, each 4-aligned
 * crc32 (IEEE, reflected, init/xorout 0xFFFFFFFF) covers every byte after
 * the header's crc field (offsets 16 .. total_size - 1), so a pack checks
 * once at open.
 */
#ifndef DOOMO_PACK_H
#define DOOMO_PACK_H

#include "types.h"
#include "plat_ext.h"
#include "story_vm.h"

#define PACK_MAGIC        0x4B504D44u   /* "DMPK" */
#define PACK_VERSION      1u
#define PACK_HEADER_SIZE  64u
#define PACK_ENTRY_SIZE   16u
#define PACK_NAME_LEN     20u           /* NUL-padded ASCII                     */
#define PACK_AUTHOR_LEN   20u

/* Ports a pack is built for (DoomoPackHeader.port): its art's size and
 * palette format follow the port's screen. */
enum { PK_PORT_GBA, PK_PORT_PS1, PK_PORT_DC, PK_PORT_NATIVE, PK_PORT_COUNT };
/* Entry kinds. */
enum { PK_KIND_CG,         /* a full-screen still (GBA 240x160, else 320x240)   */
       PK_KIND_PORTRAIT,   /* a dialogue portrait override (portrait.h size)    */
       PK_KIND_FIGURE,     /* a standing figure override (bodyspr size)         */
       PK_KIND_SCRIPT,     /* a TXF_RAW text pack (textpack.py --user) holding a
                              slot's scene scripts; uid = PACK_UID_SCRIPT(slot) */
       PK_KIND_COUNT };
/* Payload codecs. */
enum { PK_CODEC_RAW, PK_CODEC_LZ, PK_CODEC_COUNT };
/* Header flags. */
#define PACK_F_SFW_EXAMPLE 1u           /* the repository's example pack        */
#define PACK_F_ATTESTED    2u           /* the builder confirmed the 18+ rules  */

typedef struct DoomoPackHeader {
    uint32_t magic;                     /* PACK_MAGIC                           */
    uint16_t version;                   /* PACK_VERSION                         */
    uint16_t header_size;               /* PACK_HEADER_SIZE                     */
    uint32_t total_size;                /* bytes, the whole pack                */
    uint32_t crc32;                     /* of bytes [16, total_size)            */
    uint16_t n_entries;
    uint8_t  port;                      /* PK_PORT_*                            */
    uint8_t  flags;                     /* PACK_F_*                             */
    uint32_t dir_off;                   /* = PACK_HEADER_SIZE in v1             */
    char     name[PACK_NAME_LEN];
    char     author[PACK_AUTHOR_LEN];
} DoomoPackHeader;
DOOMO_STATIC_ASSERT(sizeof(DoomoPackHeader) == 64, pack_header_is_64_bytes);

typedef struct DoomoPackEntry {
    uint32_t uid;                       /* PACK_UID_BASE + ...                   */
    uint32_t off;                       /* payload offset from the pack start    */
    uint32_t size;                      /* payload bytes (as stored)             */
    uint8_t  kind;                      /* PK_KIND_*                             */
    uint8_t  codec;                     /* PK_CODEC_*                            */
    uint16_t arg;                       /* CG: the slot it belongs to; PORTRAIT /
                                           FIGURE: the resident id; SCRIPT: the
                                           text pack id (TXT_PACK_USER_FIRST..)  */
} DoomoPackEntry;
DOOMO_STATIC_ASSERT(sizeof(DoomoPackEntry) == 16, pack_entry_is_16_bytes);

/* Uids: 0x00B00000 + kind << 16 + index. They never collide with a
 * built-in art id, so a lookup can never alias the game's own art. */
#define PACK_UID_BASE        0x00B00000u
#define PACK_UID(kind, idx)  (PACK_UID_BASE + ((uint32_t)(kind) << 16) + (uint32_t)(idx))
#define PACK_UID_SCRIPT(slot) PACK_UID(PK_KIND_SCRIPT, (slot))

/* Caps per port (mpack.py validate and pack.c's open refuse a pack over
 * them). GBA: the mature ROM variant is an 8 MB game (answer 25) on a
 * 32 MB cartridge: the default cap keeps the variant within 8 MB, the
 * hard cap is what pack.c accepts at all. PS1: one entry is read whole
 * into the mslot overlay's buffer. */
#define PACK_CAP_GBA_DEFAULT (4u * 1024u * 1024u)
#define PACK_CAP_GBA_HARD    (8u * 1024u * 1024u)
#define PACK_CAP_DISC        (32u * 1024u * 1024u)   /* PS1 / DC / native       */
#define PACK_ENTRY_CAP_PS1   (96u * 1024u)
/* E9: a streamed pack's slot script is read whole into the port's script buffer
 * (plat_mslot_script_buf, plat_ext.h). The PS1 buffer is this big, so mpack.py refuses a
 * PS1 pack with a script over it. */
#define PACK_SCRIPT_CAP_PS1  (8u * 1024u)
/* E10: the Dreamcast reads a streamed script into a 32 KB buffer (16 MB of RAM). */
#define PACK_SCRIPT_CAP_DC   (32u * 1024u)
#define PACK_ENTRIES_MAX     1024u
#define PACK_CG_W_GBA  240
#define PACK_CG_H_GBA  160
#define PACK_CG_W      320                           /* PS1 / DC / native       */
#define PACK_CG_H      240
/* E8: the still size of THIS build (the GBA's screen is smaller). mslot.c and the GBA ROM
 * variant use these; the GBA has no scratch to decode into, so its stills are stored RAW and
 * shown in place from the ROM blob (mpack.py validate enforces both for port gba). */
#if defined(DOOMO_PLATFORM_GBA)
#define PACK_CG_W_THIS PACK_CG_W_GBA
#define PACK_CG_H_THIS PACK_CG_H_GBA
#else
#define PACK_CG_W_THIS PACK_CG_W
#define PACK_CG_H_THIS PACK_CG_H
#endif

/* ---- mature scene slots (E2: core/data/story/mslots.txt) ---------------
 * `push mslot N` in a built-in script (push_args.txt). E2 numbers about 32
 * of them (per route: first kiss, date night, stay-over; the wedding
 * night; the sexy endings; the hot tub; 8 free post-game slots). */
#define MSLOT_MAX 64

/* ---- what a pack's scripts may do (E2 runtime, E7 validate, textpack
 * --user) --------------------------------------------------------------
 * A pack script talks, waits, plays sound and screen effects, shows the
 * pack's art and branches on its own vars, on story flags (read only) and
 * on the Mature level. It never changes the game: no flags, items, money,
 * residents, quests, time, scene pushes, host callbacks or calls into a
 * built-in script. OP_DATA rides along with the ops that take it. Bit
 * `op` of PACK_OPS_LO (ops 0..31) / PACK_OPS_HI (ops 32..63, bit op-32).
 * Each mask is one line: textpack.py evaluates them. */
#define PACK_OPS_LO_TEXT ((1u << OP_END) | (1u << OP_SAY) | (1u << OP_SAY_X) | (1u << OP_CHOICE) | (1u << OP_PLAYER_LINE))
#define PACK_OPS_LO_FLOW ((1u << OP_JIF) | (1u << OP_JIFN) | (1u << OP_JLT) | (1u << OP_JGE) | (1u << OP_SETV) | (1u << OP_ADDV))
#define PACK_OPS_LO_TIME ((1u << OP_WAIT) | (1u << OP_HOLD) | (1u << OP_WAIT_A))
#define PACK_OPS_LO_SOUND ((1u << OP_SFX) | (1u << OP_MUSIC) | (1u << OP_MUTE) | (1u << OP_DUCK))
#define PACK_OPS_LO (PACK_OPS_LO_TEXT | PACK_OPS_LO_FLOW | PACK_OPS_LO_TIME | PACK_OPS_LO_SOUND)
#define PACK_OPS_HI_FX ((1u << (OP_TAPESTOP - 32)) | (1u << (OP_RESUME_ON_BEAT - 32)) | (1u << (OP_SHAKE - 32)) | (1u << (OP_PUNCH - 32)) | (1u << (OP_FLASH - 32)) | (1u << (OP_FADE - 32)) | (1u << (OP_IRIS - 32)) | (1u << (OP_TINT - 32)) | (1u << (OP_PAL_LERP - 32)))
#define PACK_OPS_HI_FLOW ((1u << (OP_CALL - 32)) | (1u << (OP_RET - 32)) | (1u << (OP_RNG - 32)) | (1u << (OP_DATA - 32)) | (1u << (OP_JMP - 32)) | (1u << (OP_JMAT - 32)) | (1u << (OP_SHOW - 32)))
#define PACK_OPS_HI (PACK_OPS_HI_FX | PACK_OPS_HI_FLOW)
/* 1 if a pack script may use op `op` (OP_CALL: only into its own pack,
 * which pack.c checks). */
#define PACK_OP_OK(op) ((unsigned)(op) < 32u ? (PACK_OPS_LO >> (unsigned)(op)) & 1u \
                       : (unsigned)(op) < 64u ? (PACK_OPS_HI >> ((unsigned)(op) - 32u)) & 1u : 0u)

/* ---- the API (E2: pack.c; weak defaults in weak_endings.c) -------------- */
typedef struct DoomoPackInfo {
    char     name[PACK_NAME_LEN + 1];
    char     author[PACK_AUTHOR_LEN + 1];
    uint8_t  port;                      /* PK_PORT_*                            */
    uint8_t  flags;                     /* PACK_F_*                             */
    uint16_t n_cg, n_portrait, n_figure, n_script;
    uint32_t total_size;
} DoomoPackInfo;
/* Find and validate the platform's pack (header, caps, CRC, entries) once;
 * 1 when a valid pack is present. Weak: 0. */
int  pack_open(void);
/* The open pack's summary; 0 when there is none (out is zeroed). */
int  pack_info(DoomoPackInfo *out);
/* An art entry's payload: a pointer (GBA / native blob) or NULL when there
 * is no such entry or the pack streams (then pack_art_read copies it). */
const uint8_t *pack_art(int kind, uint32_t uid, uint32_t *size);
uint32_t pack_art_read(int kind, uint32_t uid, void *dst, uint32_t cap);
/* The script id of slot `slot`'s scene in the pack (registered with
 * txt_user_set), or 0 when the pack has none (the fallback plays). */
int  pack_script(int slot);

/* ---- E2 additions (pack.c) ---------------------------------------------
 * pack_open() asks the platform (plat_pack_blob, else plat_pack_read);
 * these let a host that already holds the bytes (the native runner's pack
 * file, the tests) hand them over. Same validation, same result.
 * pack_close() forgets the open pack (and its registered scripts).
 * pack_art() returns a pointer only for a RAW entry of an addressable
 * pack; an LZ entry, or a streamed pack, is read with pack_art_read(),
 * which decodes into dst (returns the decoded bytes, 0 if it does not
 * fit in cap). Scripts are stored RAW (mpack.py refuses otherwise):
 * pack_open() registers each with txt_user_set() when the pack is
 * addressable; a streamed pack's scripts are loaded by the host with
 * pack_script_bind() into its own buffer (>= the entry's size). */
int  pack_open_image(const uint8_t *blob, uint32_t size);
void pack_close(void);
int  pack_script_bind(int slot, void *buf, uint32_t cap);
/* E9 (streamed packs: the PS1 / DC disc file). pack_streamed() is 1 when the open pack is read
 * through plat_pack_read, so its scripts are not addressable. The mslot scene binds the slot's
 * script into plat_mslot_script_buf() when it starts; for a streamed pack pack_script_bind()
 * first drops the previous binding (one buffer), and pack_script_release() unregisters every
 * script bound that way (the scene calls it when it leaves: the buffer is only the scene's while
 * it runs). Both do nothing for an addressable pack, whose scripts stay registered in place. */
int  pack_streamed(void);
void pack_script_release(void);

/* ---- E2: the mature scene slot host (core/src/mslot.c) -----------------
 * `push mslot N` pushes SCENE_MSLOT with arg = N. With OPT_MATURE = "On + pack", a valid open
 * pack and a script for slot N, the scene runs that pack's script (entry: script 1 of the pack's
 * text pack; index 0 is the empty script every pack starts with) on the story VM, which it saves
 * and restores around it, and shows the pack's art (`show cg N`). Anything else plays the safe
 * fade-to-black. mslot_result() is what the pushing script finds in var 0: 1 when the pack's
 * scene ran, 0 for the fallback. Art that is LZ or streamed needs a buffer from the port
 * (plat_mslot_buf, plat_ext.h); without one the art is skipped and the script still plays. */
int  mslot_result(void);

#endif
