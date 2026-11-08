/* vms.h - VMU files and the VMU file system, portable C (DC4; owner:
 * dreamcast).
 *
 * Two parts, both without KallistiOS so the host test (dc/tests/vmu) runs
 * them against a simulated 128 KB VMU, pulls the card out at every block
 * write and checks what is left:
 *
 * 1. VMS files: the header every VMU data file starts with (descriptions,
 *    application id, an animated 16-colour 32x32 icon, an optional 72x56
 *    eyecatch, the payload length and a CRC-16 of the whole file), the same
 *    layout and CRC as KallistiOS' vmu_pkg_build (dc/vmu_pkg.h), built into
 *    a caller's buffer instead of a malloc'ed one.
 *
 * 2. The file system on the card (root block 255, one FAT block, the
 *    directory below it, 200 user blocks), through a block read/write
 *    callback (VmuIo): on the console that is KallistiOS' vmu_block_read /
 *    vmu_block_write (vmusave.c), on the host an array. Data files are
 *    allocated from the top block down, as the VMU BIOS and KallistiOS'
 *    vmufs do, and every update writes the data blocks first, then the FAT,
 *    then the directory, so a card pulled out half way loses at most some
 *    free blocks (allocated in the FAT, in no file), never another file.
 *
 * vmu_replace() is the atomic save: the new file goes in under a temporary
 * name, is read back and compared, and only then does the old file go
 * (one directory write) and the new one take its name (a second one). A
 * card pulled out at any point keeps either the old save or the new one,
 * and vmu_load() finds the new one under its temporary name if the swap
 * was cut short. Only when the card is too full for two copies is the old
 * file replaced in place (deleted, then written), and then a pull loses
 * the save; vmu_replace() says so in its result (VMU_REPLACED_IN_PLACE).
 */
#ifndef DC_VMS_H
#define DC_VMS_H

#include <stdint.h>

#define VMU_BLOCK        512u
#define VMU_NAME_LEN     12              /* no terminator on the card            */
#define VMU_ROOT_BLOCK   255
#define VMU_DIR_MAX      16              /* directory blocks we accept (13 standard) */
#define VMU_FAT_FREE     0xFFFCu
#define VMU_FAT_END      0xFFFAu
#define VMU_TYPE_DATA    0x33u

/* ---- results ------------------------------------------------------------- */
enum {
    VMU_OK = 0,
    VMU_ERR_IO = -1,          /* a block read or write failed (card pulled?) */
    VMU_ERR_FORMAT = -2,      /* not a formatted VMU (root block)            */
    VMU_ERR_FULL = -3,        /* not enough free blocks                      */
    VMU_ERR_DIRFULL = -4,     /* no free directory entry                     */
    VMU_ERR_NOFILE = -5,      /* no such file                                */
    VMU_ERR_DAMAGED = -6,     /* the file's header or CRC is wrong            */
    VMU_ERR_VERIFY = -7,      /* read back differs from what was written     */
    VMU_ERR_ARG = -8          /* bad argument / buffer too small             */
};
#define VMU_REPLACED_IN_PLACE 1  /* vmu_replace: OK, but not atomically       */

const char *vmu_strerror(int err);

/* ---- block I/O -------------------------------------------------------------- */
typedef struct VmuIo {
    int  (*read)(void *ctx, int block, uint8_t *buf);          /* 0 = ok */
    int  (*write)(void *ctx, int block, const uint8_t *buf);   /* 0 = ok */
    void (*progress)(void *ctx, int done, int total);          /* may be NULL */
    void *ctx;
} VmuIo;

/* ---- VMS files ---------------------------------------------------------------- */
#define VMS_HDR_SIZE     128u
#define VMS_ICON_SIZE    512u            /* one 32x32 4bpp frame                */
#define VMS_EC_NONE      0
#define VMS_EC_16COL     3               /* 16-colour palette + 72x56 4bpp      */
#define VMS_EC16_SIZE    (32u + 72u * 56u / 2u)
#define VMS_MAX_ICONS    3

typedef struct VmsDesc {
    const char     *desc_short;          /* <= 16 chars (space padded)          */
    const char     *desc_long;           /* <= 32 chars (space padded)          */
    const char     *app_id;              /* <= 16 chars (NUL padded)            */
    int             icon_frames;         /* 1..VMS_MAX_ICONS                    */
    int             anim_speed;          /* frames of 1/30 s per icon frame     */
    const uint16_t *icon_pal;            /* 16 ARGB4444                         */
    const uint8_t  *icon;                /* icon_frames * 512 bytes             */
    int             eyecatch;            /* VMS_EC_NONE or VMS_EC_16COL         */
    const uint16_t *ec_pal;              /* 16 ARGB4444                         */
    const uint8_t  *ec;                  /* 72 * 56 / 2 bytes                   */
} VmsDesc;

typedef struct VmsInfo {
    char     desc_short[17], desc_long[33], app_id[17];
    int      icon_frames, anim_speed, eyecatch;
    uint32_t data_off;                   /* payload offset in the file          */
    uint32_t data_len;                   /* payload bytes                       */
    uint16_t crc;                        /* as stored                           */
} VmsInfo;

uint16_t vms_crc16(const uint8_t *p, uint32_t n, uint16_t crc);  /* CRC-16/XMODEM */
uint32_t vms_header_bytes(const VmsDesc *d);   /* header + icons + eyecatch     */
/* The whole file into out (zero padded to whole blocks); returns its size
 * in bytes (a multiple of VMU_BLOCK), 0 when it does not fit cap. */
uint32_t vms_build(uint8_t *out, uint32_t cap, const VmsDesc *d, const void *data, uint32_t n);
/* Parse the header of a file of which `have` bytes are at f. check_crc:
 * the whole file must be there (header + payload) and its CRC right.
 * VMU_OK, or VMU_ERR_DAMAGED. app_id (may be NULL) must match. */
int vms_parse(const uint8_t *f, uint32_t have, int check_crc, const char *app_id, VmsInfo *out);

/* ---- the card ---------------------------------------------------------------- */
typedef struct VmuVol {
    uint8_t  root[VMU_BLOCK];
    uint16_t fat[256];
    uint8_t  dir[VMU_DIR_MAX * VMU_BLOCK];
    int      fat_loc, dir_loc, dir_blocks, user_blocks;
} VmuVol;

/* Read the root, FAT and directory. VMU_OK, VMU_ERR_IO or VMU_ERR_FORMAT. */
int vmu_mount(VmuVol *v, const VmuIo *io);
int vmu_free_blocks(const VmuVol *v);
int vmu_find(const VmuVol *v, const char *name);          /* dir index or -1 */
int vmu_file_blocks(const VmuVol *v, int idx);
/* The first `blocks` blocks of file idx into buf (cap bytes); _range:
 * only its blocks [from, to), each at its own place in buf. */
int vmu_read_blocks(const VmuVol *v, const VmuIo *io, int idx, uint8_t *buf, uint32_t cap, int blocks);
int vmu_read_range(const VmuVol *v, const VmuIo *io, int idx, uint8_t *buf, uint32_t cap, int from, int to);
/* A new data file (name must not exist). ts: 8 BCD bytes (century, year,
 * month, day, hour, minute, second, weekday) or NULL for zeros. */
int vmu_write_file(VmuVol *v, const VmuIo *io, const char *name, const uint8_t *data, int blocks,
                   const uint8_t *ts);
int vmu_delete(VmuVol *v, const VmuIo *io, int idx);     /* dir, then FAT  */
int vmu_rename(VmuVol *v, const VmuIo *io, int idx, const char *name);

/* The atomic save (see the top of this file). file: a whole VMS file of
 * `blocks` blocks; check: a buffer of at least blocks * 512 bytes for the
 * read-back. VMU_OK, VMU_REPLACED_IN_PLACE or an error. */
int vmu_replace(VmuVol *v, const VmuIo *io, const char *name, const char *tmp,
                const uint8_t *file, int blocks, const uint8_t *ts, uint8_t *check);
/* Read file `name` (or, when the last save's swap was cut short, `tmp`)
 * and parse it. want: payload bytes wanted (0 = all): with want < the
 * payload only the blocks holding the header and the first `want` bytes
 * are read and the CRC is not checked (a header peek). On VMU_OK buf holds
 * the file from its start and *info describes it. Without a good `name`
 * file, a good `tmp` file is used. */
int vmu_load(VmuVol *v, const VmuIo *io, const char *name, const char *tmp, const char *app_id,
             uint8_t *buf, uint32_t cap, uint32_t want, VmsInfo *info);
/* Remove `name` and `tmp`. VMU_OK also when there was nothing to remove. */
int vmu_remove(VmuVol *v, const VmuIo *io, const char *name, const char *tmp);

/* ---- formatting (host tests and tools only) ---------------------------------- */
/* A blank, formatted 128 KB card image (256 blocks), as a VMU leaves the
 * factory: root at 255, FAT at 254, 13 directory blocks 253..241, 200
 * user blocks. */
void vmu_format_image(uint8_t *img);

#endif
