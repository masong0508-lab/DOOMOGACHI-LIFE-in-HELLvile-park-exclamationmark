/* vms.c - VMU files and the VMU file system, portable C (DC4; owner:
 * dreamcast). The layouts and the update order are explained in vms.h.
 *
 * Card layout (a standard 128 KB VMU; the root block gives the numbers):
 *   block 255       root: 16 x 0x55, ..., FAT at 0x46, FAT size 0x48,
 *                   directory at 0x4A, directory size 0x4C, user blocks 0x50
 *   block 254       FAT: 256 little-endian 16-bit entries, 0xFFFC free,
 *                   0xFFFA last block of a chain, else the next block
 *   blocks 253..241 directory, 16 entries of 32 bytes per block (block 253
 *                   holds entries 0..15): type (0x33 data), copy flag,
 *                   first block, 12-char name, BCD time, size in blocks,
 *                   header offset in blocks, 4 bytes 0
 *   blocks 0..199   files: data files from the top down
 * Everything is read and written with explicit little-endian byte access.
 */
#include <string.h>
#include "vms.h"

static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

const char *vmu_strerror(int err)
{
    switch (err) {
    case VMU_OK: return "ok";
    case VMU_REPLACED_IN_PLACE: return "ok (replaced in place)";
    case VMU_ERR_IO: return "VMU read/write error";
    case VMU_ERR_FORMAT: return "VMU not formatted";
    case VMU_ERR_FULL: return "VMU full";
    case VMU_ERR_DIRFULL: return "VMU directory full";
    case VMU_ERR_NOFILE: return "no such file";
    case VMU_ERR_DAMAGED: return "file damaged";
    case VMU_ERR_VERIFY: return "VMU verify failed";
    default: return "bad argument";
    }
}

/* ---- VMS files --------------------------------------------------------------------- */
uint16_t vms_crc16(const uint8_t *p, uint32_t n, uint16_t crc)
{
    /* the same as KallistiOS' net_crc16ccitt (CRC-16/XMODEM: poly 0x1021,
     * MSB first), which vmu_pkg_build and the BIOS use */
    while (n--) {
        uint16_t t = (uint16_t)((crc >> 8) ^ *p++);
        t ^= (uint16_t)(t >> 4);
        crc = (uint16_t)((crc << 8) ^ (t << 12) ^ (t << 5) ^ t);
    }
    return crc;
}

static uint32_t ec_bytes(int type)
{
    return type == VMS_EC_16COL ? VMS_EC16_SIZE : 0u;
}

uint32_t vms_header_bytes(const VmsDesc *d)
{
    return VMS_HDR_SIZE + VMS_ICON_SIZE * (uint32_t)d->icon_frames + ec_bytes(d->eyecatch);
}

static void put_text(uint8_t *dst, const char *s, int len, uint8_t pad)
{
    int i;
    for (i = 0; i < len; i++) dst[i] = pad;
    for (i = 0; s && s[i] && i < len; i++) dst[i] = (uint8_t)s[i];
}

uint32_t vms_build(uint8_t *out, uint32_t cap, const VmsDesc *d, const void *data, uint32_t n)
{
    uint32_t hb, total, padded, i;
    uint8_t *p;
    if (!out || !d || d->icon_frames < 1 || d->icon_frames > VMS_MAX_ICONS || !d->icon_pal || !d->icon)
        return 0;
    if (d->eyecatch != VMS_EC_NONE && (d->eyecatch != VMS_EC_16COL || !d->ec_pal || !d->ec))
        return 0;
    if (n && !data) return 0;
    hb = vms_header_bytes(d);
    total = hb + n;
    padded = (total + VMU_BLOCK - 1) / VMU_BLOCK * VMU_BLOCK;
    if (total < n || padded > cap) return 0;
    memset(out, 0, padded);
    put_text(out + 0x00, d->desc_short, 16, ' ');
    put_text(out + 0x10, d->desc_long, 32, ' ');
    put_text(out + 0x30, d->app_id, 16, 0);
    put16(out + 0x40, (uint32_t)d->icon_frames);
    put16(out + 0x42, (uint32_t)d->anim_speed);
    put16(out + 0x44, (uint32_t)d->eyecatch);
    put16(out + 0x46, 0);                /* CRC, below */
    put32(out + 0x48, n);
    for (i = 0; i < 16; i++) put16(out + 0x60 + i * 2, d->icon_pal[i]);
    p = out + VMS_HDR_SIZE;
    memcpy(p, d->icon, VMS_ICON_SIZE * (uint32_t)d->icon_frames);
    p += VMS_ICON_SIZE * (uint32_t)d->icon_frames;
    if (d->eyecatch == VMS_EC_16COL) {
        for (i = 0; i < 16; i++) put16(p + i * 2, d->ec_pal[i]);
        memcpy(p + 32, d->ec, VMS_EC16_SIZE - 32);
        p += VMS_EC16_SIZE;
    }
    if (n) memcpy(p, data, n);
    put16(out + 0x46, vms_crc16(out, total, 0));
    return padded;
}

static void get_text(char *dst, const uint8_t *src, int len)
{
    int i;
    for (i = 0; i < len; i++) dst[i] = (char)(src[i] ? src[i] : ' ');
    while (len > 0 && dst[len - 1] == ' ') len--;
    dst[len] = 0;
}

int vms_parse(const uint8_t *f, uint32_t have, int check_crc, const char *app_id, VmsInfo *out)
{
    VmsInfo in;
    uint32_t hb;
    if (!f || have < VMS_HDR_SIZE) return VMU_ERR_DAMAGED;
    memset(&in, 0, sizeof(in));
    get_text(in.desc_short, f + 0x00, 16);
    get_text(in.desc_long, f + 0x10, 32);
    get_text(in.app_id, f + 0x30, 16);
    in.icon_frames = get16(f + 0x40);
    in.anim_speed = get16(f + 0x42);
    in.eyecatch = get16(f + 0x44);
    in.crc = get16(f + 0x46);
    in.data_len = get32(f + 0x48);
    if (in.icon_frames < 1 || in.icon_frames > 64) return VMU_ERR_DAMAGED;
    if (in.eyecatch != VMS_EC_NONE && in.eyecatch != VMS_EC_16COL &&
        in.eyecatch != 1 && in.eyecatch != 2) return VMU_ERR_DAMAGED;
    hb = VMS_HDR_SIZE + VMS_ICON_SIZE * (uint32_t)in.icon_frames +
         (in.eyecatch == 1 ? 72u * 56u * 2u : in.eyecatch == 2 ? 512u + 72u * 56u : ec_bytes(in.eyecatch));
    in.data_off = hb;
    if (in.data_len > 256u * VMU_BLOCK) return VMU_ERR_DAMAGED;
    if (app_id) {
        char want[17];
        uint8_t tmp[16];
        put_text(tmp, app_id, 16, 0);
        get_text(want, tmp, 16);
        if (strcmp(want, in.app_id) != 0) return VMU_ERR_DAMAGED;
    }
    if (check_crc) {
        uint32_t total = hb + in.data_len;
        uint8_t zero[2] = { 0, 0 };
        uint16_t c;
        if (total > have) return VMU_ERR_DAMAGED;
        c = vms_crc16(f, 0x46, 0);
        c = vms_crc16(zero, 2, c);
        c = vms_crc16(f + 0x48, total - 0x48, c);
        if (c != in.crc) return VMU_ERR_DAMAGED;
    }
    if (out) *out = in;
    return VMU_OK;
}

/* ---- the card ------------------------------------------------------------------------- */
#define DIRENT 32
static uint8_t *dirent(VmuVol *v, int idx) { return v->dir + idx * DIRENT; }
static const uint8_t *dirent_c(const VmuVol *v, int idx) { return v->dir + idx * DIRENT; }
static int dir_entries(const VmuVol *v) { return v->dir_blocks * (int)(VMU_BLOCK / DIRENT); }

int vmu_mount(VmuVol *v, const VmuIo *io)
{
    int i;
    memset(v, 0, sizeof(*v));
    if (io->read(io->ctx, VMU_ROOT_BLOCK, v->root)) return VMU_ERR_IO;
    for (i = 0; i < 16; i++)
        if (v->root[i] != 0x55) return VMU_ERR_FORMAT;
    v->fat_loc = get16(v->root + 0x46);
    v->dir_loc = get16(v->root + 0x4A);
    v->dir_blocks = get16(v->root + 0x4C);
    v->user_blocks = get16(v->root + 0x50);
    if (get16(v->root + 0x48) != 1 || v->fat_loc >= 256 || v->dir_blocks < 1 ||
        v->dir_blocks > VMU_DIR_MAX || v->dir_loc >= 256 || v->dir_loc - v->dir_blocks + 1 < 0 ||
        v->user_blocks < 1 || v->user_blocks > 256)
        return VMU_ERR_FORMAT;
    {
        uint8_t blk[VMU_BLOCK];
        if (io->read(io->ctx, v->fat_loc, blk)) return VMU_ERR_IO;
        for (i = 0; i < 256; i++) v->fat[i] = get16(blk + i * 2);
    }
    for (i = 0; i < v->dir_blocks; i++)
        if (io->read(io->ctx, v->dir_loc - i, v->dir + i * VMU_BLOCK)) return VMU_ERR_IO;
    return VMU_OK;
}

int vmu_free_blocks(const VmuVol *v)
{
    int i, n = 0;
    for (i = 0; i < v->user_blocks; i++)
        if (v->fat[i] == VMU_FAT_FREE) n++;
    return n;
}

static int name_eq(const uint8_t *e, const char *name)
{
    int i;
    for (i = 0; i < VMU_NAME_LEN; i++) {
        uint8_t c = (uint8_t)name[i];
        uint8_t d = e[4 + i];
        if (!c) {                        /* the rest must be padding */
            for (; i < VMU_NAME_LEN; i++)
                if (e[4 + i] != 0 && e[4 + i] != ' ') return 0;
            return 1;
        }
        if (c != d) return 0;
    }
    return 1;
}

int vmu_find(const VmuVol *v, const char *name)
{
    int i;
    for (i = 0; i < dir_entries(v); i++) {
        const uint8_t *e = dirent_c(v, i);
        if (e[0] != 0 && name_eq(e, name)) return i;
    }
    return -1;
}

int vmu_file_blocks(const VmuVol *v, int idx)
{
    return idx < 0 ? 0 : get16(dirent_c(v, idx) + 0x18);
}

int vmu_read_range(const VmuVol *v, const VmuIo *io, int idx, uint8_t *buf, uint32_t cap, int from, int to)
{
    const uint8_t *e;
    int b, k, size;
    if (idx < 0 || !buf || from < 0) return VMU_ERR_ARG;
    e = dirent_c(v, idx);
    size = get16(e + 0x18);
    if (to > size) to = size;
    if ((uint32_t)to * VMU_BLOCK > cap) return VMU_ERR_ARG;
    b = get16(e + 0x02);
    for (k = 0; k < to; k++) {
        if (b >= v->user_blocks) return VMU_ERR_DAMAGED;
        if (k >= from) {
            if (io->read(io->ctx, b, buf + (uint32_t)k * VMU_BLOCK)) return VMU_ERR_IO;
            if (io->progress) io->progress(io->ctx, k + 1, to);
        }
        b = v->fat[b];
        if (k + 1 < size && (b == VMU_FAT_FREE || b == VMU_FAT_END)) return VMU_ERR_DAMAGED;
    }
    return VMU_OK;
}

int vmu_read_blocks(const VmuVol *v, const VmuIo *io, int idx, uint8_t *buf, uint32_t cap, int blocks)
{
    return vmu_read_range(v, io, idx, buf, cap, 0, blocks);
}

static int write_fat(VmuVol *v, const VmuIo *io)
{
    uint8_t blk[VMU_BLOCK];
    int i;
    for (i = 0; i < 256; i++) put16(blk + i * 2, v->fat[i]);
    return io->write(io->ctx, v->fat_loc, blk) ? VMU_ERR_IO : VMU_OK;
}

static int write_dir_block_of(VmuVol *v, const VmuIo *io, int idx)
{
    int b = idx / (int)(VMU_BLOCK / DIRENT);
    return io->write(io->ctx, v->dir_loc - b, v->dir + b * VMU_BLOCK) ? VMU_ERR_IO : VMU_OK;
}

static void set_name(uint8_t *e, const char *name)
{
    int i, end = 0;
    for (i = 0; i < VMU_NAME_LEN; i++) {
        if (!end && !name[i]) end = 1;
        e[4 + i] = end ? 0 : (uint8_t)name[i];
    }
}

int vmu_write_file(VmuVol *v, const VmuIo *io, const char *name, const uint8_t *data, int blocks,
                   const uint8_t *ts)
{
    uint16_t saved[256];
    int i, slot = -1, first = -1, prev = -1, k, r;
    uint8_t *e;
    if (!name || !name[0] || !data || blocks < 1 || blocks > v->user_blocks) return VMU_ERR_ARG;
    if (vmu_find(v, name) >= 0) return VMU_ERR_ARG;
    for (i = 0; i < dir_entries(v); i++)
        if (dirent(v, i)[0] == 0) { slot = i; break; }
    if (slot < 0) return VMU_ERR_DIRFULL;
    if (vmu_free_blocks(v) < blocks) return VMU_ERR_FULL;
    memcpy(saved, v->fat, sizeof(saved));
    /* data blocks, top down; the FAT only changes in memory until all are written */
    i = v->user_blocks - 1;
    for (k = 0; k < blocks; k++) {
        while (i >= 0 && v->fat[i] != VMU_FAT_FREE) i--;
        if (i < 0) { memcpy(v->fat, saved, sizeof(saved)); return VMU_ERR_FULL; }
        if (io->write(io->ctx, i, data + (uint32_t)k * VMU_BLOCK)) {
            memcpy(v->fat, saved, sizeof(saved));
            return VMU_ERR_IO;
        }
        if (io->progress) io->progress(io->ctx, k + 1, blocks);
        v->fat[i] = VMU_FAT_END;
        if (prev >= 0) v->fat[prev] = (uint16_t)i;
        else first = i;
        prev = i;
        i--;
    }
    if ((r = write_fat(v, io)) != VMU_OK) { memcpy(v->fat, saved, sizeof(saved)); return r; }
    e = dirent(v, slot);
    memset(e, 0, DIRENT);
    e[0] = VMU_TYPE_DATA;
    e[1] = 0x00;                         /* copyable */
    put16(e + 0x02, (uint32_t)first);
    set_name(e, name);
    if (ts) memcpy(e + 0x10, ts, 8);
    put16(e + 0x18, (uint32_t)blocks);
    put16(e + 0x1A, 0);                  /* the VMS header is in block 0 */
    if ((r = write_dir_block_of(v, io, slot)) != VMU_OK) {
        /* the FAT already holds the chain: those blocks leak until the
         * card is formatted, but no other file is touched */
        memset(e, 0, DIRENT);
        return r;
    }
    return VMU_OK;
}

int vmu_delete(VmuVol *v, const VmuIo *io, int idx)
{
    uint8_t *e, keep[DIRENT];
    int b, n = 0, r;
    if (idx < 0 || idx >= dir_entries(v)) return VMU_ERR_ARG;
    e = dirent(v, idx);
    if (!e[0]) return VMU_ERR_NOFILE;
    memcpy(keep, e, DIRENT);
    memset(e, 0, DIRENT);
    if ((r = write_dir_block_of(v, io, idx)) != VMU_OK) { memcpy(e, keep, DIRENT); return r; }
    /* the directory no longer names the file: now free its chain */
    b = get16(keep + 0x02);
    while (b < v->user_blocks && n < 256) {
        int next = v->fat[b];
        if (next == VMU_FAT_FREE) break;
        v->fat[b] = VMU_FAT_FREE;
        if (next == VMU_FAT_END) break;
        b = next;
        n++;
    }
    return write_fat(v, io);
}

int vmu_rename(VmuVol *v, const VmuIo *io, int idx, const char *name)
{
    uint8_t *e, keep[DIRENT];
    int r;
    if (idx < 0 || idx >= dir_entries(v) || !name || !name[0]) return VMU_ERR_ARG;
    e = dirent(v, idx);
    if (!e[0]) return VMU_ERR_NOFILE;
    memcpy(keep, e, DIRENT);
    set_name(e, name);
    if ((r = write_dir_block_of(v, io, idx)) != VMU_OK) memcpy(e, keep, DIRENT);
    return r;
}

static int verify(const VmuVol *v, const VmuIo *io, int idx, const uint8_t *file, int blocks,
                  uint8_t *check)
{
    int r;
    VmuIo quiet = *io;
    quiet.progress = NULL;
    if (vmu_file_blocks(v, idx) != blocks) return VMU_ERR_VERIFY;
    r = vmu_read_blocks(v, &quiet, idx, check, (uint32_t)blocks * VMU_BLOCK, blocks);
    if (r != VMU_OK) return r;
    return memcmp(check, file, (uint32_t)blocks * VMU_BLOCK) ? VMU_ERR_VERIFY : VMU_OK;
}

int vmu_replace(VmuVol *v, const VmuIo *io, const char *name, const char *tmp,
                const uint8_t *file, int blocks, const uint8_t *ts, uint8_t *check)
{
    int old, t, r, have;
    if (!file || !check || blocks < 1) return VMU_ERR_ARG;
    old = vmu_find(v, name);
    t = vmu_find(v, tmp);
    if (t >= 0) {
        if (old < 0) {
            /* the last swap was cut short after the old file went: the
             * temporary file is the newest save, give it its name first */
            if ((r = vmu_rename(v, io, t, name)) != VMU_OK) return r;
            old = t;
        } else if ((r = vmu_delete(v, io, t)) != VMU_OK) {
            return r;                    /* a half-made copy: drop it */
        }
    }
    have = vmu_free_blocks(v);
    if (have >= blocks) {
        /* atomic: new copy, read back, then the old one goes and the new
         * one takes its name (two directory writes) */
        if ((r = vmu_write_file(v, io, tmp, file, blocks, ts)) != VMU_OK) return r;
        t = vmu_find(v, tmp);
        if ((r = verify(v, io, t, file, blocks, check)) != VMU_OK) {
            vmu_delete(v, io, t);
            return r;
        }
        if (old >= 0 && (r = vmu_delete(v, io, old)) != VMU_OK) return r;
        return vmu_rename(v, io, t, name);
    }
    if (old >= 0 && have + vmu_file_blocks(v, old) >= blocks) {
        /* too full for two copies: replace in place */
        if ((r = vmu_delete(v, io, old)) != VMU_OK) return r;
        if ((r = vmu_write_file(v, io, name, file, blocks, ts)) != VMU_OK) return r;
        r = verify(v, io, vmu_find(v, name), file, blocks, check);
        return r == VMU_OK ? VMU_REPLACED_IN_PLACE : r;
    }
    return VMU_ERR_FULL;
}

static int load_one(VmuVol *v, const VmuIo *io, int idx, const char *app_id, uint8_t *buf,
                    uint32_t cap, uint32_t want, VmsInfo *info)
{
    int size = vmu_file_blocks(v, idx), r, need;
    VmsInfo in;
    VmuIo quiet = *io;
    if (size < 1) return VMU_ERR_DAMAGED;
    /* the first block says how big the header is */
    quiet.progress = NULL;
    if ((r = vmu_read_blocks(v, &quiet, idx, buf, cap, 1)) != VMU_OK) return r;
    if ((r = vms_parse(buf, VMU_BLOCK, 0, app_id, &in)) != VMU_OK) return r;
    if ((in.data_off + in.data_len + VMU_BLOCK - 1) / VMU_BLOCK > (uint32_t)size) return VMU_ERR_DAMAGED;
    if (want && want < in.data_len) {
        /* a peek: only the blocks holding payload bytes [0, want) (the
         * icons and the eyecatch in between are left out of buf) */
        int from = (int)(in.data_off / VMU_BLOCK);
        need = (int)((in.data_off + want + VMU_BLOCK - 1) / VMU_BLOCK);
        if (from < 1) from = 1;
        if (need > from && (r = vmu_read_range(v, &quiet, idx, buf, cap, from, need)) != VMU_OK) return r;
    } else {
        need = (int)((in.data_off + in.data_len + VMU_BLOCK - 1) / VMU_BLOCK);
        if ((uint32_t)need * VMU_BLOCK > cap) return VMU_ERR_ARG;
        if (need > 1 && (r = vmu_read_range(v, io, idx, buf, cap, 1, need)) != VMU_OK) return r;
        if ((r = vms_parse(buf, (uint32_t)need * VMU_BLOCK, 1, app_id, &in)) != VMU_OK) return r;
    }
    if (info) *info = in;
    return VMU_OK;
}

int vmu_load(VmuVol *v, const VmuIo *io, const char *name, const char *tmp, const char *app_id,
             uint8_t *buf, uint32_t cap, uint32_t want, VmsInfo *info)
{
    int a = vmu_find(v, name), b = tmp ? vmu_find(v, tmp) : -1, r = VMU_ERR_NOFILE;
    if (!buf || cap < VMU_BLOCK) return VMU_ERR_ARG;
    if (a >= 0) {
        r = load_one(v, io, a, app_id, buf, cap, want, info);
        if (r == VMU_OK || r == VMU_ERR_IO || r == VMU_ERR_ARG) return r;
    }
    /* a temporary file is only trusted alone: with both present it may be
     * a copy whose read-back never finished */
    if (b >= 0 && a < 0)
        r = load_one(v, io, b, app_id, buf, cap, want, info);
    return r;
}

int vmu_remove(VmuVol *v, const VmuIo *io, const char *name, const char *tmp)
{
    int r = VMU_OK, i;
    if ((i = vmu_find(v, name)) >= 0) r = vmu_delete(v, io, i);
    if (r == VMU_OK && tmp && (i = vmu_find(v, tmp)) >= 0) r = vmu_delete(v, io, i);
    return r;
}

/* ---- formatting --------------------------------------------------------------------- */
void vmu_format_image(uint8_t *img)
{
    uint8_t *root = img + VMU_ROOT_BLOCK * VMU_BLOCK, *fat = img + 254 * VMU_BLOCK;
    int i;
    memset(img, 0, 256 * VMU_BLOCK);
    memset(root, 0x55, 16);
    root[0x10] = 0x01;                   /* custom colour on: white */
    root[0x11] = root[0x12] = root[0x13] = root[0x14] = 0xFF;
    /* timestamp 2026-01-01 00:00:00 (BCD), a Thursday */
    root[0x30] = 0x20; root[0x31] = 0x26; root[0x32] = 0x01; root[0x33] = 0x01;
    root[0x37] = 0x03;
    root[0x40] = 0xFF; root[0x44] = 0xFF;
    put16(root + 0x46, 254);             /* FAT */
    put16(root + 0x48, 1);
    put16(root + 0x4A, 253);             /* directory */
    put16(root + 0x4C, 13);
    put16(root + 0x4E, 0);               /* icon shape */
    put16(root + 0x50, 200);             /* user blocks */
    put16(root + 0x52, 31);              /* as a factory VMU: save area */
    put16(root + 0x56, 128);
    for (i = 0; i < 256; i++) put16(fat + i * 2, VMU_FAT_FREE);
    for (i = 242; i <= 253; i++) put16(fat + i * 2, (uint32_t)(i - 1));   /* directory chain */
    put16(fat + 241 * 2, VMU_FAT_END);
    put16(fat + 254 * 2, VMU_FAT_END);
    put16(fat + 255 * 2, VMU_FAT_END);
}
