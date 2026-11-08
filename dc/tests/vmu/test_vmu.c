/* test_vmu.c - host tests of DC4's portable parts (owner: dreamcast):
 * the VMS file format and the VMU file system (dc/src/vms.c) on a
 * simulated 128 KB VMU, including a card pulled out at every single block
 * write of a save; the icon and eyecatch (save_icon.c); the controller and
 * keyboard mapping (padmap.c). No KallistiOS, no emulator.
 *
 *   test_vmu [OUTDIR]   OUTDIR gets icons.png (every pet form's icon frames
 *                       and eyecatch, 4x) and card.bin (a VMU image with a
 *                       story save, a free play save and a config record,
 *                       for dc/harness/vmu_check.py)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vms.h"
#include "vmusave.h"
#include "dcinput.h"
#include "png.h"
#include "doomo/input.h"
#include "doomo/save.h"
#include "doomo/pet.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* ---- a simulated VMU ------------------------------------------------------------------- */
typedef struct Card {
    uint8_t img[256 * VMU_BLOCK];
    int writes, reads;           /* counters                                       */
    int pull_at;                 /* the card is pulled before this write (-1: never) */
    int pulled;
} Card;

static int c_read(void *ctx, int b, uint8_t *buf)
{
    Card *c = ctx;
    if (c->pulled || b < 0 || b > 255) return 1;
    memcpy(buf, c->img + b * VMU_BLOCK, VMU_BLOCK);
    c->reads++;
    return 0;
}

static int c_write(void *ctx, int b, const uint8_t *buf)
{
    Card *c = ctx;
    if (c->pulled || b < 0 || b > 255) return 1;
    if (c->pull_at >= 0 && c->writes >= c->pull_at) { c->pulled = 1; return 1; }
    memcpy(c->img + b * VMU_BLOCK, buf, VMU_BLOCK);
    c->writes++;
    return 0;
}

static VmuIo io_of(Card *c)
{
    VmuIo io;
    io.read = c_read;
    io.write = c_write;
    io.progress = NULL;
    io.ctx = c;
    return io;
}

static void card_new(Card *c)
{
    memset(c, 0, sizeof(*c));
    vmu_format_image(c->img);
    c->pull_at = -1;
}

/* The card's consistency: every file's chain stays inside the user blocks,
 * has exactly its size, and no block is in two files. Returns the number of
 * leaked blocks (allocated, in no file), or -1 if inconsistent. */
static int fsck(Card *c)
{
    VmuVol v;
    VmuIo io = io_of(c);
    uint8_t owner[256];
    int i, used = 0, leaked = 0;
    if (vmu_mount(&v, &io) != VMU_OK) return -1;
    memset(owner, 0, sizeof(owner));
    for (i = 0; i < v.dir_blocks * 16; i++) {
        const uint8_t *e = v.dir + i * 32;
        int b, n, size;
        if (!e[0]) continue;
        b = e[2] | (e[3] << 8);
        size = e[0x18] | (e[0x19] << 8);
        for (n = 0; n < size; n++) {
            if (b >= v.user_blocks || owner[b]) return -1;
            owner[b] = 1;
            used++;
            if (n + 1 < size) b = v.fat[b];
            else if (v.fat[b] != VMU_FAT_END) return -1;
        }
    }
    for (i = 0; i < v.user_blocks; i++)
        if (v.fat[i] != VMU_FAT_FREE && !owner[i]) leaked++;
    return leaked;
}

/* ---- VMS ----------------------------------------------------------------------------- */
static uint16_t crc_ref(const uint8_t *p, uint32_t n)     /* bit by bit */
{
    uint16_t c = 0;
    uint32_t i;
    int b;
    for (i = 0; i < n; i++) {
        c ^= (uint16_t)(p[i] << 8);
        for (b = 0; b < 8; b++) c = (uint16_t)((c & 0x8000) ? (c << 1) ^ 0x1021 : c << 1);
    }
    return c;
}

static uint16_t pal16[16];
static uint8_t icon[DC_VMU_FRAMES * VMS_ICON_SIZE], ec[VMS_EC16_SIZE - 32];
static uint16_t ec_pal[16];

static uint32_t make_file(uint8_t *out, uint32_t cap, int slot, const uint8_t *data, uint32_t n)
{
    VmsDesc d;
    memset(&d, 0, sizeof(d));
    d.desc_short = "BOREagotchi";
    d.desc_long = dc_vmu_desc_long(slot);
    d.app_id = DC_VMU_APP_ID;
    dc_vmu_icon(-1, pal16, icon);
    d.icon_pal = pal16;
    d.icon = icon;
    d.icon_frames = DC_VMU_FRAMES;
    d.anim_speed = DC_VMU_ANIM;
    dc_vmu_eyecatch(-1, ec_pal, ec);
    d.eyecatch = VMS_EC_16COL;
    d.ec_pal = ec_pal;
    d.ec = ec;
    return vms_build(out, cap, &d, data, n);
}

static void fill(uint8_t *p, uint32_t n, uint32_t seed)
{
    uint32_t i;
    for (i = 0; i < n; i++) { seed = seed * 1103515245u + 12345u; p[i] = (uint8_t)(seed >> 16); }
}

#define FILE_CAP (96u * 1024u)
static uint8_t fbuf[FILE_CAP], fbuf2[FILE_CAP], chk[FILE_CAP], rbuf[FILE_CAP];

static void test_vms(void)
{
    static const uint8_t check9[] = "123456789";
    uint8_t data[1000];
    uint32_t n;
    VmsInfo in;
    CHECK(vms_crc16(check9, 9, 0) == 0x31C3);            /* CRC-16/XMODEM check value */
    fill(data, sizeof(data), 7);
    CHECK(vms_crc16(data, sizeof(data), 0) == crc_ref(data, sizeof(data)));
    n = make_file(fbuf, sizeof(fbuf), 0, data, sizeof(data));
    CHECK(n == 10 * VMU_BLOCK);                           /* 3712 + 1000 = 4712 -> 10 blocks */
    CHECK(vms_parse(fbuf, n, 1, DC_VMU_APP_ID, &in) == VMU_OK);
    CHECK(in.data_off == 128 + 3 * 512 + 2048);
    CHECK(in.data_len == sizeof(data));
    CHECK(in.icon_frames == 3 && in.eyecatch == VMS_EC_16COL);
    CHECK(!strcmp(in.desc_short, "BOREagotchi"));
    CHECK(!strcmp(in.desc_long, "BOREagotchi LIFE! - story"));
    CHECK(!memcmp(fbuf + in.data_off, data, sizeof(data)));
    /* the stored CRC is the KallistiOS one: over the file with the CRC field 0 */
    {
        uint8_t save0 = fbuf[0x46], save1 = fbuf[0x47];
        fbuf[0x46] = fbuf[0x47] = 0;
        CHECK(crc_ref(fbuf, in.data_off + in.data_len) == in.crc);
        fbuf[0x46] = save0; fbuf[0x47] = save1;
    }
    CHECK(vms_parse(fbuf, n, 1, "SOMEONE ELSE", &in) == VMU_ERR_DAMAGED);
    fbuf[in.data_off + 5] ^= 1;
    CHECK(vms_parse(fbuf, n, 1, DC_VMU_APP_ID, &in) == VMU_ERR_DAMAGED);
    CHECK(vms_parse(fbuf, n, 0, DC_VMU_APP_ID, &in) == VMU_OK);  /* a peek has no CRC */
    CHECK(make_file(fbuf, 4096, 0, data, sizeof(data)) == 0);       /* does not fit */
}

/* ---- the file system ------------------------------------------------------------------ */
static Card card, card2;

static int load(Card *c, int slot, uint32_t want, uint8_t *out, uint32_t *len)
{
    VmuVol v;
    VmuIo io = io_of(c);
    VmsInfo in;
    static const char *const nm[2] = { "DOOMO0.SYS", "DOOMO1.SYS" }, *const tm[2] = { "DOOMO0.NEW", "DOOMO1.NEW" };
    int r = vmu_mount(&v, &io);
    if (r) return r;
    r = vmu_load(&v, &io, nm[slot], tm[slot], DC_VMU_APP_ID, rbuf, sizeof(rbuf), want, &in);
    if (r) return r;
    if (out) memcpy(out, rbuf + in.data_off, want && want < in.data_len ? want : in.data_len);
    if (len) *len = in.data_len;
    return VMU_OK;
}

static int save(Card *c, int slot, const uint8_t *data, uint32_t n)
{
    VmuVol v;
    VmuIo io = io_of(c);
    static const char *const nm[2] = { "DOOMO0.SYS", "DOOMO1.SYS" }, *const tm[2] = { "DOOMO0.NEW", "DOOMO1.NEW" };
    static const uint8_t ts[8] = { 0x20, 0x26, 0x11, 0x08, 0x12, 0x34, 0x56, 0x06 };
    uint32_t bytes = make_file(fbuf2, sizeof(fbuf2), slot, data, n);
    int r = vmu_mount(&v, &io);
    if (r) return r;
    if (!bytes) return VMU_ERR_ARG;
    return vmu_replace(&v, &io, nm[slot], tm[slot], fbuf2, (int)(bytes / VMU_BLOCK), ts, chk);
}

static uint8_t old_data[DOOMO_SAVE_SLOT0_MAX], new_data[DOOMO_SAVE_SLOT0_MAX], got[DOOMO_SAVE_SLOT1_MAX];

static void test_fs(void)
{
    VmuVol v;
    VmuIo io;
    uint32_t len = 0;
    int r, idx, before;
    card_new(&card);
    io = io_of(&card);
    CHECK(vmu_mount(&v, &io) == VMU_OK);
    CHECK(vmu_free_blocks(&v) == 200);
    CHECK(fsck(&card) == 0);
    /* a fresh card has no save */
    CHECK(load(&card, 0, 0, got, &len) == VMU_ERR_NOFILE);
    /* save, load back */
    fill(old_data, sizeof(old_data), 1);
    CHECK(save(&card, 0, old_data, 10000) == VMU_OK);
    CHECK(fsck(&card) == 0);
    CHECK(load(&card, 0, 0, got, &len) == VMU_OK && len == 10000 && !memcmp(got, old_data, 10000));
    CHECK(vmu_mount(&v, &io) == VMU_OK);
    idx = vmu_find(&v, "DOOMO0.SYS");
    CHECK(idx >= 0 && vmu_file_blocks(&v, idx) == 27);    /* (3712 + 10000) / 512 -> 27 */
    CHECK(vmu_find(&v, "DOOMO0.NEW") < 0);
    CHECK(vmu_free_blocks(&v) == 173);
    /* data files go from the top down: block 199 first */
    CHECK(v.dir[idx * 32 + 2] == 199 && v.dir[idx * 32 + 3] == 0);
    /* a peek reads the directory and only the blocks the header needs */
    card.reads = 0;
    CHECK(load(&card, 0, 48, got, &len) == VMU_OK && !memcmp(got, old_data, 48));
    CHECK(card.reads == 1 + 1 + 13 + 1 + 1);              /* root, FAT, dir, block 0, block 7 */
    /* the other slot beside it, then overwrite slot 0 */
    fill(new_data, sizeof(new_data), 2);
    CHECK(save(&card, 1, new_data, 30000) == VMU_OK);
    CHECK(save(&card, 0, new_data, 12345) == VMU_OK);
    CHECK(fsck(&card) == 0);
    CHECK(load(&card, 0, 0, got, &len) == VMU_OK && len == 12345 && !memcmp(got, new_data, 12345));
    CHECK(load(&card, 1, 0, got, &len) == VMU_OK && len == 30000 && !memcmp(got, new_data, 30000));
    /* rename, delete */
    CHECK(vmu_mount(&v, &io) == VMU_OK);
    before = vmu_free_blocks(&v);
    CHECK(vmu_remove(&v, &io, "DOOMO1.SYS", "DOOMO1.NEW") == VMU_OK);
    CHECK(vmu_free_blocks(&v) == before + 66);            /* (3712 + 30000) / 512 -> 66 */
    CHECK(vmu_remove(&v, &io, "DOOMO1.SYS", "DOOMO1.NEW") == VMU_OK);   /* nothing left: ok */
    CHECK(fsck(&card) == 0);
    /* a corrupted block: the CRC catches it */
    CHECK(vmu_mount(&v, &io) == VMU_OK);
    idx = vmu_find(&v, "DOOMO0.SYS");
    card.img[(v.dir[idx * 32 + 2] - 10) * VMU_BLOCK + 100] ^= 0x40;   /* 11th block of the file */
    CHECK(load(&card, 0, 0, got, &len) == VMU_ERR_DAMAGED);
    CHECK(load(&card, 0, 48, got, &len) == VMU_OK);       /* the header block is fine */
    /* not a VMU */
    memset(card.img + 255 * VMU_BLOCK, 0, 16);
    CHECK(vmu_mount(&v, &io) == VMU_ERR_FORMAT);
    /* no card at all */
    card_new(&card);
    card.pulled = 1;
    CHECK(vmu_mount(&v, &io) == VMU_ERR_IO);
    (void)r;
}

/* The card is pulled out before write number k, for every k a save makes;
 * then it is put back. The old save or the new one must load, whole. */
static void test_pull(void)
{
    int k, writes, old_ok = 0, new_ok = 0, leaks = 0, worst_leak = 0;
    uint32_t len;
    card_new(&card);
    fill(old_data, sizeof(old_data), 11);
    fill(new_data, sizeof(new_data), 12);
    CHECK(save(&card, 1, old_data, 20000) == VMU_OK);
    /* how many writes does a save make? */
    card2 = card;
    card2.writes = 0;
    CHECK(save(&card2, 1, new_data, 25000) == VMU_OK);
    writes = card2.writes;
    CHECK(writes == 57 + 1 + 1 + 1 + 1 + 1);              /* 57 data, FAT, dir; delete: dir, FAT; rename: dir */
    for (k = 0; k <= writes; k++) {
        int l, r;
        card2 = card;
        card2.writes = 0;
        card2.pull_at = k;
        r = save(&card2, 1, new_data, 25000);
        CHECK((r == VMU_OK) == (k == writes));
        card2.pulled = 0;                                  /* put back */
        card2.pull_at = -1;
        l = fsck(&card2);
        CHECK(l >= 0);
        if (l > 0) { leaks++; if (l > worst_leak) worst_leak = l; }
        r = load(&card2, 1, 0, got, &len);
        CHECK(r == VMU_OK);
        if (r == VMU_OK && len == 20000 && !memcmp(got, old_data, 20000)) old_ok++;
        else if (r == VMU_OK && len == 25000 && !memcmp(got, new_data, 25000)) new_ok++;
        else CHECK(!"neither the old nor the new save loads");
        /* and the next save works and cleans up */
        r = save(&card2, 1, old_data, 1000);
        CHECK(r == VMU_OK);
        CHECK(load(&card2, 1, 0, got, &len) == VMU_OK && len == 1000 && !memcmp(got, old_data, 1000));
        {
            VmuVol v;
            VmuIo io = io_of(&card2);
            CHECK(vmu_mount(&v, &io) == VMU_OK && vmu_find(&v, "DOOMO1.NEW") < 0);
        }
    }
    printf("pull test: %d writes a save; pulled at each of %d points: old save %d, new save %d, "
           "leaked blocks on %d (worst %d)\n", writes, writes + 1, old_ok, new_ok, leaks, worst_leak);
    CHECK(old_ok + new_ok == writes + 1);
    CHECK(new_ok >= 2);                                   /* after the delete: the .NEW file is found */
}

/* a card too full for two copies; and one too full for any */
static void test_full(void)
{
    VmuVol v;
    VmuIo io;
    uint8_t blob[512 * 120];
    uint32_t len;
    card_new(&card);
    io = io_of(&card);
    fill(blob, sizeof(blob), 5);
    fill(old_data, sizeof(old_data), 21);
    fill(new_data, sizeof(new_data), 22);
    CHECK(vmu_mount(&v, &io) == VMU_OK);
    CHECK(vmu_write_file(&v, &io, "OTHERGAME.01", blob, 120, NULL) == VMU_OK);   /* another game: 120 */
    CHECK(save(&card, 0, old_data, 30000) == VMU_OK);     /* 66 blocks; 14 free */
    /* 66 more would not fit beside it: replaced in place */
    CHECK(save(&card, 0, new_data, 30000) == VMU_REPLACED_IN_PLACE);
    CHECK(load(&card, 0, 0, got, &len) == VMU_OK && len == 30000 && !memcmp(got, new_data, 30000));
    CHECK(fsck(&card) == 0);
    /* 40000 bytes (86 blocks) fit neither beside it (14 free) nor in its
     * place (14 + 66): refused, the old save kept */
    CHECK(save(&card, 1, old_data, 40000) == VMU_ERR_FULL);
    CHECK(save(&card, 0, old_data, 40000) == VMU_ERR_FULL);
    CHECK(load(&card, 0, 0, got, &len) == VMU_OK && len == 30000 && !memcmp(got, new_data, 30000));
    /* the other game's file is untouched */
    CHECK(vmu_mount(&v, &io) == VMU_OK);
    {
        int i = vmu_find(&v, "OTHERGAME.01");
        CHECK(i >= 0 && vmu_read_blocks(&v, &io, i, rbuf, sizeof(rbuf), 120) == VMU_OK);
        CHECK(!memcmp(rbuf, blob, sizeof(blob)));
    }
    CHECK(fsck(&card) == 0);
    /* sizes against the 200 blocks of a VMU */
    {
        uint32_t s0 = make_file(fbuf, sizeof(fbuf), 0, old_data, DOOMO_SAVE_SLOT0_MAX);
        uint32_t s1 = 0;
        static uint8_t big[DOOMO_SAVE_SLOT1_MAX];
        s1 = make_file(fbuf, sizeof(fbuf), 1, big, DOOMO_SAVE_SLOT1_MAX);
        printf("sizes: slot 0 at most %u blocks, slot 1 at most %u blocks (VMU: 200)\n",
               (unsigned)(s0 / VMU_BLOCK), (unsigned)(s1 / VMU_BLOCK));
        CHECK(s0 / VMU_BLOCK == 88 && s1 / VMU_BLOCK == 184);
    }
}

/* 200 one-block files fill the card (the directory has room for 208) */
static void test_many(void)
{
    VmuVol v;
    VmuIo io;
    uint8_t blk[512];
    char nm[13];
    int i;
    card_new(&card);
    io = io_of(&card);
    CHECK(vmu_mount(&v, &io) == VMU_OK);
    memset(blk, 1, sizeof(blk));
    for (i = 0; i < 200; i++) {
        snprintf(nm, sizeof(nm), "F%03d", i);
        CHECK(vmu_write_file(&v, &io, nm, blk, 1, NULL) == VMU_OK);
    }
    CHECK(vmu_write_file(&v, &io, "ONE.MORE", blk, 1, NULL) == VMU_ERR_FULL);
    CHECK(fsck(&card) == 0);
    CHECK(vmu_mount(&v, &io) == VMU_OK && vmu_find(&v, "F123") >= 0 && vmu_find(&v, "F12") < 0);
}

/* ---- icons ------------------------------------------------------------------------------- */
static void put_px(uint8_t *rgb, int W, int x, int y, uint16_t argb, int zoom)
{
    int a = (argb >> 12) & 15, r = ((argb >> 8) & 15) * 17, g = ((argb >> 4) & 15) * 17, b = (argb & 15) * 17;
    int i, j;
    if (!a) r = g = b = 40;                               /* transparent: dark grey */
    for (j = 0; j < zoom; j++)
        for (i = 0; i < zoom; i++) {
            uint8_t *p = rgb + ((y * zoom + j) * W + x * zoom + i) * 3;
            p[0] = (uint8_t)r; p[1] = (uint8_t)g; p[2] = (uint8_t)b;
        }
}

static int nib(const uint8_t *bits, int stride, int x, int y)
{
    uint8_t b = bits[y * (stride / 2) + x / 2];
    return (x & 1) ? b & 15 : b >> 4;
}

static void test_icons(const char *outdir)
{
    enum { Z = 3, CELL_W = 32 * 3 + 72 + 8, CELL_H = 64, COLS = 3, ROWS = 6 };
    const int W = COLS * CELL_W * Z, H = ROWS * CELL_H * Z;
    uint8_t *rgb = calloc((size_t)W * H * 3, 1);
    int f, k, x, y, n = 0;
    for (f = -2; f < PET_FORM_COUNT; f++) {
        int cx = (n % COLS) * CELL_W, cy = (n / COLS) * CELL_H, differs = 0, used = 0;
        dc_vmu_icon(f, pal16, icon);
        dc_vmu_eyecatch(f, ec_pal, ec);
        CHECK(pal16[0] == 0 && (pal16[1] & 0xF000) == 0xF000);
        for (k = 0; k < DC_VMU_FRAMES; k++)
            for (y = 0; y < 32; y++)
                for (x = 0; x < 32; x++) {
                    int c = nib(icon + k * 512, 32, x, y);
                    CHECK(c < 6);
                    if (c == 3 || c == 2) used = 1;
                    if (k && c != nib(icon, 32, x, y)) differs = 1;
                    put_px(rgb, W, cx + k * 33 + x, cy + y, pal16[c], Z);
                }
        CHECK(nib(icon, 32, 0, 0) == 0 && nib(icon, 32, 31, 31) == 0);   /* round corners */
        CHECK(used);                                       /* the pet is drawn */
        CHECK(differs);                                    /* and it moves */
        for (y = 0; y < 56; y++)
            for (x = 0; x < 72; x++) {
                int c = nib(ec, 72, x, y);
                CHECK(c >= 1 && c < 6);                    /* an eyecatch is opaque */
                put_px(rgb, W, cx + 3 * 33 + x, cy + y, ec_pal[c], Z);
            }
        n++;
    }
    if (outdir) {
        char path[512];
        snprintf(path, sizeof(path), "%s/icons.png", outdir);
        CHECK(png_write_rgb(path, rgb, W, H));
        printf("icons: %s (egg, ghost and the 16 forms: 3 icon frames + eyecatch, %dx)\n", path, Z);
    }
    free(rgb);
}

/* ---- input --------------------------------------------------------------------------------- */
static void test_pad(void)
{
    DcPadRaw r;
    DcPadOut o;
    uint8_t keys[256];
    memset(&r, 0, sizeof(r));
    dc_pad_map(&r, &o);
    CHECK(o.btn == 0 && o.analog == 0 && o.stick_mask == 0);
    r.buttons = DCP_A | DCP_B | DCP_X | DCP_Y | DCP_UP | DCP_RIGHT;
    dc_pad_map(&r, &o);
    CHECK(o.btn == (uint32_t)(BTN_A | BTN_B | BTN_X | BTN_Y | BTN_UP | BTN_RIGHT));
    r.buttons = DCP_START;
    dc_pad_map(&r, &o);
    CHECK(o.btn == BTN_START);
    r.buttons = DCP_START | DCP_Y;                         /* SELECT, and neither of the two */
    dc_pad_map(&r, &o);
    CHECK(o.btn == BTN_SELECT);
    r.buttons = DCP_D | DCP_C | DCP_Z | DCP_LEFT2 | DCP_RIGHT2;
    dc_pad_map(&r, &o);
    CHECK(o.btn == (uint32_t)(BTN_SELECT | BTN_L | BTN_R | BTN_L2 | BTN_R2));
    r.buttons = 0;
    r.ltrig = 127; r.rtrig = 128;                          /* past half: > 127 */
    dc_pad_map(&r, &o);
    CHECK(o.btn == BTN_R);
    r.ltrig = r.rtrig = 0;
    r.analog = 1;
    r.joyx = -49; r.joyy = 48;
    dc_pad_map(&r, &o);
    CHECK(o.analog && o.lx == -49 && o.ly == 48 && o.stick_mask == BTN_LEFT && o.btn == BTN_LEFT);
    r.joyx = 127; r.joyy = -128;
    dc_pad_map(&r, &o);
    CHECK(o.stick_mask == (uint32_t)(BTN_RIGHT | BTN_UP) && o.rx == 0);
    r.analog2 = 1; r.joy2x = 30; r.joy2y = -40; r.joyx = r.joyy = 0;
    dc_pad_map(&r, &o);
    CHECK(o.rx == 30 && o.ry == -40 && o.stick_mask == 0);
    CHECK(o.caps == INPUT_CAP_RSTICK);
    /* ST1: a standard pad (one stick + analog triggers) */
    r.analog2 = 0; r.triggers = 1; r.ltrig = 40; r.rtrig = 300;
    dc_pad_map(&r, &o);
    CHECK(o.caps == INPUT_CAP_TRIGGERS && o.ltrig == 40 && o.rtrig == 255 && o.rx == 0);
    r.triggers = 0; r.ltrig = r.rtrig = 0; r.analog2 = 1;
    r.analog = 0;                                          /* a digital pad: no stick at all */
    dc_pad_map(&r, &o);
    CHECK(!o.analog && o.lx == 0 && o.rx == 0);
    /* keyboard */
    memset(keys, 0, sizeof(keys));
    CHECK(dc_kbd_buttons(keys, 256, 0) == 0);
    keys[0x52] = keys[0x50] = 1;                           /* up, left */
    keys[0x1D] = 1;                                        /* Z */
    keys[0x29] = 1;                                        /* Escape */
    CHECK(dc_kbd_buttons(keys, 256, 0) == (uint32_t)(BTN_UP | BTN_LEFT | BTN_A | BTN_B));
    /* the on-screen keyboard is open: Z types, the arrows and Escape still act */
    CHECK(dc_kbd_buttons(keys, 256, 1) == (uint32_t)(BTN_UP | BTN_LEFT | BTN_B));
    memset(keys, 0, sizeof(keys));
    keys[0x13] = keys[0x2B] = keys[0x14] = keys[0x1A] = keys[0x1E] = keys[0x1F] = keys[0x06] = keys[0x19] = 1;
    CHECK(dc_kbd_buttons(keys, 256, 0) ==
          (uint32_t)(BTN_START | BTN_SELECT | BTN_L | BTN_R | BTN_L2 | BTN_R2 | BTN_X | BTN_Y));
    CHECK(dc_kbd_buttons(keys, 0x20, 0) == (uint32_t)(BTN_START | BTN_L | BTN_R | BTN_L2 | BTN_R2 | BTN_X | BTN_Y));
    CHECK(dc_kbd_text_char('a') == 'a' && dc_kbd_text_char('~') == '~' && dc_kbd_text_char(' ') == ' ');
    CHECK(dc_kbd_text_char(13) == '\n' && dc_kbd_text_char(10) == '\n' && dc_kbd_text_char(8) == '\b');
    CHECK(dc_kbd_text_char(27) == -1 && dc_kbd_text_char(9) == -1 && dc_kbd_text_char(0x52 << 8) == -1);
    CHECK(dc_kbd_text_char(-23) == 0xE9);                  /* e acute as a signed char */
}

/* ---- a card image for vmu_check.py ---------------------------------------------------------- */
static void write_card(const char *outdir)
{
    char path[512];
    FILE *f;
    uint8_t cfg[48];
    VmuVol v;
    VmuIo io;
    VmsDesc d;
    uint32_t n;
    card_new(&card);
    io = io_of(&card);
    fill(old_data, sizeof(old_data), 31);
    CHECK(save(&card, 0, old_data, 9876) == VMU_OK);
    CHECK(save(&card, 1, old_data, 23456) == VMU_OK);
    memset(&d, 0, sizeof(d));
    memset(cfg, 0x5A, sizeof(cfg));
    d.desc_short = "BOREagotchi";
    d.desc_long = dc_vmu_desc_long(DC_VMU_CONFIG);
    d.app_id = DC_VMU_APP_ID;
    dc_vmu_icon(-1, pal16, icon);
    d.icon_pal = pal16; d.icon = icon; d.icon_frames = 1;
    n = vms_build(fbuf, sizeof(fbuf), &d, cfg, sizeof(cfg));
    CHECK(n == 2 * VMU_BLOCK);
    CHECK(vmu_mount(&v, &io) == VMU_OK);
    CHECK(vmu_replace(&v, &io, "DOOMOCFG.SYS", "DOOMOCFG.NEW", fbuf, 2, NULL, chk) == VMU_OK);
    snprintf(path, sizeof(path), "%s/card.bin", outdir);
    f = fopen(path, "wb");
    CHECK(f && fwrite(card.img, 1, sizeof(card.img), f) == sizeof(card.img));
    if (f) fclose(f);
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : NULL;
    test_vms();
    test_fs();
    test_pull();
    test_full();
    test_many();
    test_icons(out);
    test_pad();
    if (out) write_card(out);
    printf("test_vmu: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
