/* vmusave.c - saves and the machine config record on the VMU (DC4; owner:
 * dreamcast).
 *
 * The core's save calls (platform.h) and plat_config_read/write
 * (plat_ext.h) on the first VMU Maple finds (A1, then A2, B1, ...), one
 * VMS file each (vmusave.h has the names). The file system work is the
 * portable vms.c over KallistiOS' vmu_block_read / vmu_block_write, under
 * KallistiOS' VMU file system lock:
 *   write   the whole VMS file (header, the pet's icon and eyecatch,
 *           the core's save) is built in RAM, then vmu_replace(): written
 *           under the .NEW name, read back and compared, then the old file
 *           goes and the new one takes its name. A VMU pulled out half way
 *           keeps the old save or the new one, never half of one. Only a
 *           card too full for two copies is rewritten in place.
 *   read    a peek (the core asks for the 48-byte header: menus, the
 *           title) reads the directory and the file's first blocks, no
 *           CRC; a load reads it all and checks the VMS CRC (the core then
 *           checks its own).
 * The Dreamcast has 16 MB, so the core gets a static whole-image scratch
 * (DOOMO_SAVE_SCRATCH_MIN) and never streams.
 *
 * Every VMU block read costs about a 60th of a second, so what is known is
 * kept: the card's root, FAT and directory (s_vol), and for each of the
 * three files whether it is there, its size and its first 64 payload
 * bytes (the core's save header, the whole config record). dc_vmu_init()
 * fills that at boot; the title screen's and menus' peeks then never touch
 * the card, and a missing save costs nothing. Maple's attach and detach
 * callbacks (a VMU pulled out, put in, swapped) and any error forget it
 * all, so the next call reads the card again.
 *
 * While a save, a load or an erase runs the tick is blocked (about 1-2 s
 * for 20 blocks on a real VMU), so a card is shown in its place: "Saving
 * your game...", "Do not remove the VMU or switch off the power", and a
 * bar that moves with every block. A failed write shows the reason for a
 * second (no VMU, full, not formatted) once per kind of error, and
 * returns 0: the core shows its own message.
 */
#include <kos.h>
#include <time.h>
#include <dc/maple.h>
#include <dc/maple/vmu.h>
#include <dc/vmufs.h>
#include <dc/vblank.h>
#include <dc/video.h>
#include "dc_plat.h"
#include "dc2d.h"
#include "vmusave.h"
#include "doomo/save.h"
#include "doomo/plat_ext.h"
#include "doomo/palette.h"
#include "doomo/font.h"
#include "doomo/scene.h"

/* the biggest file: header, 3 icon frames, the eyecatch and slot 1 */
#define FILE_MAX (((VMS_HDR_SIZE + DC_VMU_FRAMES * VMS_ICON_SIZE + VMS_EC16_SIZE + \
                    DOOMO_SAVE_SLOT1_MAX) + VMU_BLOCK - 1) / VMU_BLOCK * VMU_BLOCK)

static uint8_t  s_file[FILE_MAX] __attribute__((aligned(32)));
static uint8_t  s_check[FILE_MAX] __attribute__((aligned(32)));
static uint8_t  save_scratch[DOOMO_SAVE_SCRATCH_MIN] __attribute__((aligned(32)));
static VmuVol   s_vol;
static uint16_t s_icon_pal[16], s_ec_pal[16];
static uint8_t  s_icon[DC_VMU_FRAMES * VMS_ICON_SIZE];
static uint8_t  s_ec[VMS_EC16_SIZE - 32];

static const char *const k_name[3] = { "DOOMO0.SYS", "DOOMO1.SYS", "DOOMOCFG.SYS" };
static const char *const k_tmp[3]  = { "DOOMO0.NEW", "DOOMO1.NEW", "DOOMOCFG.NEW" };

/* ---- the card on screen ------------------------------------------------------------- */
static const char *card_title, *card_line;
static uint32_t card_vbl;
static int shown_err;                   /* the last error shown (once per kind) */

static void card_text(int y, const char *s, uint8_t col)
{
    int n = 0;
    while (s[n]) n++;
    plat_text_outline((DC2D_LW - n * FONT_ADVANCE) / 2, y, s, col, PAL_WHITE);
}

static void card_draw(int done, int total)
{
    const int bx = 60, by = 128, bw = 200, bh = 14;
    dc2d_side_begin();
    plat_clear(PAL_UI_PINK);
    plat_rect(0, 58, DC2D_LW, 2, PAL_UI_PINK_D);
    plat_rect(0, 180, DC2D_LW, 2, PAL_UI_PINK_D);
    card_text(86, card_title, PAL_BLACK);
    if (total > 0) {
        int fill = done >= total ? bw : bw * done / total;
        plat_rect(bx - 2, by - 2, bw + 4, bh + 4, PAL_UI_PINK_D);
        plat_rect(bx, by, bw, bh, PAL_WHITE);
        if (fill > 0) plat_rect(bx, by, fill, bh, PAL_UI_RED);
    }
    if (card_line) card_text(158, card_line, PAL_UI_PINK_D);
    dc2d_side_end();
    card_vbl = dc_vblanks;
}

static void card_progress(void *ctx, int done, int total)
{
    (void)ctx;
    /* one picture a vblank at most: the PVR shows no more */
    if (card_title && (dc_vblanks != card_vbl || done >= total)) card_draw(done, total);
}

static void card_open(const char *title, const char *line)
{
    card_title = title;
    card_line = line;
    card_draw(0, 0);
}

static void card_close(void) { card_title = NULL; }

/* a failed write: its reason for a second, once per kind of error */
static void card_error(int err, const char *what)
{
    uint32_t until;
    const char *why = err == VMU_ERR_NOFILE ? "No VMU found in a controller" :
                      err == VMU_ERR_FULL ? "The VMU is full" :
                      err == VMU_ERR_DIRFULL ? "The VMU has too many files" :
                      err == VMU_ERR_FORMAT ? "The VMU is not formatted" :
                      "The VMU could not be written";
    if (err == shown_err) return;
    shown_err = err;
    card_title = what;
    card_line = why;
    card_draw(0, 0);
    until = dc_vblanks + 60;
    while ((int32_t)(dc_vblanks - until) < 0) vid_waitvbl();
    card_close();
}

/* ---- the device ---------------------------------------------------------------------- */
static int io_read(void *ctx, int block, uint8_t *buf)
{
    return vmu_block_read((maple_device_t *)ctx, (uint16_t)block, buf) != 0;
}

static int io_write(void *ctx, int block, const uint8_t *buf)
{
    return vmu_block_write((maple_device_t *)ctx, (uint16_t)block, buf) != 0;
}

/* ---- what is known about the card ------------------------------------------------------ */
enum { HC_UNKNOWN, HC_PRESENT, HC_ABSENT };
#define HC_BYTES 64
typedef struct HdrCache {
    uint8_t  state;
    uint8_t  n;                          /* payload bytes in head[]              */
    uint32_t len;                        /* the payload's size                   */
    uint8_t  head[HC_BYTES];
} HdrCache;
static HdrCache s_hc[3];
static volatile uint32_t s_plug;        /* bumped by Maple's attach / detach    */
static uint32_t s_plug_seen;
static maple_device_t *s_vol_dev;
static int s_vol_ok;

/* KallistiOS keeps ONE attach and ONE detach callback per Maple driver, and
 * the VMU's driver serves the memory card, the LCD and the clock: a later
 * package that needs to hear about VMUs (X1, the VMU screen) must call
 * this from its own callback instead of replacing it. */
static void on_plug(maple_device_t *dev, void *user)
{
    (void)dev; (void)user;
    s_plug++;                           /* IRQ context: only note it */
}

static void forget(void)
{
    s_vol_ok = 0;
    memset(s_hc, 0, sizeof(s_hc));
}

/* the first VMU (A1, A2, B1, ...), or NULL; forgets what is known when it
 * is another one or anything was plugged or pulled since */
static maple_device_t *first_vmu(void)
{
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
    if (dev != s_vol_dev || s_plug != s_plug_seen) {
        forget();
        s_vol_dev = dev;
        s_plug_seen = s_plug;
    }
    return dev;
}

static void hc_set(int which, const uint8_t *payload, uint32_t have, uint32_t len)
{
    HdrCache *h = &s_hc[which];
    h->state = HC_PRESENT;
    h->len = len;
    h->n = (uint8_t)(have < HC_BYTES ? have : HC_BYTES);
    memcpy(h->head, payload, h->n);
}

/* the first VMU, its file system known (mounted once, then kept); *io set
 * up. VMU_OK, VMU_ERR_NOFILE (no VMU) or the mount's error. Locks the VMU
 * file system: vmu_end() after. */
static int vmu_begin(VmuIo *io, int show)
{
    maple_device_t *dev = first_vmu();
    int r = VMU_OK;
    io->read = io_read;
    io->write = io_write;
    io->progress = show ? card_progress : NULL;
    io->ctx = dev;
    if (!dev) return VMU_ERR_NOFILE;
    dc_audio_stall(1);                  /* DC5: the card blocks the tick; not an underrun */
    vmufs_mutex_lock();
    if (!s_vol_ok) {
        r = vmu_mount(&s_vol, io);
        s_vol_ok = r == VMU_OK;
        if (r != VMU_OK) vmufs_mutex_unlock();
    }
    return r;
}

/* r: how it went. Anything but a clean result (or a plain "not there" /
 * "no room") may have left the card and our copy of it apart: forget it. */
static void vmu_end(int r)
{
    if (r < VMU_OK && r != VMU_ERR_NOFILE && r != VMU_ERR_FULL && r != VMU_ERR_DIRFULL) forget();
    vmufs_mutex_unlock();
    dc_pacing_reset();                  /* the tick was blocked: no catch-up */
}

static void timestamp(uint8_t ts[8])
{
    vmu_dir_t d;
    memset(&d, 0, sizeof(d));
    vmufs_dir_fill_time(&d);
    memcpy(ts, &d.timestamp, 8);
}

static const char *port_name(const VmuIo *io)
{
    static char nm[3];
    const maple_device_t *dev = (const maple_device_t *)io->ctx;
    nm[0] = (char)('A' + dev->port);
    nm[1] = (char)('0' + dev->unit);
    nm[2] = 0;
    return nm;
}

/* one file: build it and replace the one on the VMU */
static int write_file(int which, const void *data, uint32_t size, const char *title)
{
    VmsDesc d;
    VmuIo io;
    uint8_t ts[8];
    uint32_t bytes;
    int r, form = dc_vmu_pet_form(), blocks;
    uint32_t t0 = dc_us();
    memset(&d, 0, sizeof(d));
    d.desc_short = "BOREagotchi";
    d.desc_long = dc_vmu_desc_long(which);
    d.app_id = DC_VMU_APP_ID;
    dc_vmu_icon(form, s_icon_pal, s_icon);
    d.icon_pal = s_icon_pal;
    d.icon = s_icon;
    if (which == DC_VMU_CONFIG) {
        d.icon_frames = 1;              /* 2 blocks in all */
        d.eyecatch = VMS_EC_NONE;
    } else {
        d.icon_frames = DC_VMU_FRAMES;
        d.anim_speed = DC_VMU_ANIM;
        dc_vmu_eyecatch(form, s_ec_pal, s_ec);
        d.eyecatch = VMS_EC_16COL;
        d.ec_pal = s_ec_pal;
        d.ec = s_ec;
    }
    bytes = vms_build(s_file, sizeof(s_file), &d, data, size);
    if (!bytes) return VMU_ERR_ARG;
    blocks = (int)(bytes / VMU_BLOCK);
    card_open(title, "Do not remove the VMU or switch off the power");
    r = vmu_begin(&io, 1);
    if (r == VMU_OK) {
        timestamp(ts);
        r = vmu_replace(&s_vol, &io, k_name[which], k_tmp[which], s_file, blocks, ts, s_check);
        dc_logf("DC: vmu %s write %s %d blocks -> %s (%u free, %u us)", port_name(&io), k_name[which],
                blocks, vmu_strerror(r), (unsigned)vmu_free_blocks(&s_vol), (unsigned)(dc_us() - t0));
        vmu_end(r);
        if (r >= VMU_OK) hc_set(which, (const uint8_t *)data, size, size);
    } else {
        dc_logf("DC: vmu write %s: %s", k_name[which], r == VMU_ERR_NOFILE ? "no VMU" : vmu_strerror(r));
        dc_pacing_reset();
    }
    card_close();
    if (r >= VMU_OK) shown_err = 0;
    return r;
}

/* one file: the payload (or its first cap bytes) into data. big: a load
 * (the card is shown while it reads); else a peek. */
static int read_file(int which, void *data, uint32_t cap, uint32_t *size, int big)
{
    VmuIo io;
    VmsInfo info;
    HdrCache *h = &s_hc[which];
    uint32_t n, t0 = dc_us();
    int r, shown = 0;
    if (!first_vmu()) return 0;
    /* known: not there, or the bytes asked for are in the cache */
    if (h->state == HC_ABSENT) return 0;
    if (h->state == HC_PRESENT && (cap <= h->n || h->len <= h->n)) {
        n = h->len < cap ? h->len : cap;
        memcpy(data, h->head, n);
        if (size) *size = n;
        return 1;
    }
    r = vmu_begin(&io, 0);
    if (r == VMU_OK) {
        if (vmu_find(&s_vol, k_name[which]) < 0 && vmu_find(&s_vol, k_tmp[which]) < 0) {
            r = VMU_ERR_NOFILE;
        } else {
            if (big) {
                card_open("Loading your game...", "Do not remove the VMU or switch off the power");
                io.progress = card_progress;
                shown = 1;
            }
            r = vmu_load(&s_vol, &io, k_name[which], k_tmp[which], DC_VMU_APP_ID, s_file, sizeof(s_file),
                         cap, &info);
        }
        vmu_end(r);
    } else {
        dc_pacing_reset();
    }
    if (shown) card_close();
    if (r == VMU_ERR_NOFILE && s_vol_ok) h->state = HC_ABSENT;
    if (r != VMU_OK) {
        if (r != VMU_ERR_NOFILE) dc_logf("DC: vmu read %s: %s", k_name[which], vmu_strerror(r));
        return 0;
    }
    n = info.data_len < cap ? info.data_len : cap;
    memcpy(data, s_file + info.data_off, n);
    hc_set(which, s_file + info.data_off, n, info.data_len);
    if (size) *size = n;
    if (big) dc_logf("DC: vmu read %s %u bytes (%u us)", k_name[which], (unsigned)n, (unsigned)(dc_us() - t0));
    return 1;
}

/* At boot (main.c, before doomo_init): watch for VMUs coming and going, and
 * learn the card once, so the title screen's peeks cost nothing. */
void dc_vmu_init(void)
{
    uint8_t tmp[HC_BYTES];
    uint32_t n, t0 = dc_us();
    int i, have[3];
    maple_attach_callback(MAPLE_FUNC_MEMCARD, on_plug, NULL);
    maple_detach_callback(MAPLE_FUNC_MEMCARD, on_plug, NULL);
    if (!first_vmu()) {
        dc_log("DC: vmu: none found (saves need a VMU in a controller)");
        return;
    }
    for (i = 0; i < 3; i++) have[i] = read_file(i, tmp, i == DC_VMU_CONFIG ? PLAT_CONFIG_MAX : 48, &n, 0);
    dc_logf("DC: vmu %c%c: %d blocks free, story %s, free play %s, settings %s (%u us)",
            'A' + s_vol_dev->port, '0' + s_vol_dev->unit, s_vol_ok ? vmu_free_blocks(&s_vol) : -1,
            have[0] ? "saved" : "-", have[1] ? "saved" : "-", have[2] ? "saved" : "-",
            (unsigned)(dc_us() - t0));
}

/* ---- platform.h ----------------------------------------------------------------------- */
int plat_save_write(int slot, const void *data, uint32_t size)
{
    int r;
    if (slot < 0 || slot > 1 || !data || !size || size > save_slot_max(slot)) return 0;
    r = write_file(slot, data, size, "Saving your game...");
    if (r < VMU_OK) card_error(r, "Couldn't save");
    return r >= VMU_OK;
}

int plat_save_read(int slot, void *data, uint32_t cap, uint32_t *size)
{
    if (slot < 0 || slot > 1 || !data || !cap) return 0;
    /* a menu peek (the 48-byte header) keeps the screen; a load shows the card */
    return read_file(slot, data, cap, size, cap > HC_BYTES);
}

int plat_save_erase(int slot)
{
    VmuIo io;
    int r;
    if (slot < 0 || slot > 1) return 0;
    card_open("Erasing...", "Do not remove the VMU or switch off the power");
    r = vmu_begin(&io, 0);
    if (r == VMU_OK) {
        r = vmu_remove(&s_vol, &io, k_name[slot], k_tmp[slot]);
        vmu_end(r);
        if (r == VMU_OK) s_hc[slot].state = HC_ABSENT;
    } else {
        dc_pacing_reset();
    }
    card_close();
    dc_logf("DC: vmu erase %s -> %s", k_name[slot], vmu_strerror(r));
    return r == VMU_OK;
}

void *plat_save_scratch(uint32_t *cap)
{
    *cap = sizeof(save_scratch);
    return save_scratch;
}

/* ---- plat_ext.h: the machine config record ---------------------------------------------
 * The core writes the record on every change of a machine-wide option (each
 * press on a volume slider, each step through the language list). A VMU
 * write takes a few tenths of a second, so the record is written behind:
 * plat_config_write keeps it (reads see it at once) and dc_vmu_poll(),
 * after every tick, writes it when the scene has changed (leaving the
 * options, the language picker) or 5 seconds after the last change. */
#define CFG_LATE_TICKS 150
static uint8_t  s_cfg[PLAT_CONFIG_MAX];
static uint32_t s_cfg_len, s_cfg_tick;
static int      s_cfg_dirty;
static const DoomoScene *s_cfg_scene;

int plat_config_write(const uint8_t *src, uint32_t len)
{
    if (!src || !len || len > PLAT_CONFIG_MAX) return 0;
    if (!first_vmu()) return 0;
    memcpy(s_cfg, src, len);
    s_cfg_len = len;
    s_cfg_dirty = 1;
    s_cfg_scene = scene_top();
    s_cfg_tick = dc_frames;
    hc_set(DC_VMU_CONFIG, src, len, len);
    return 1;
}

void dc_vmu_poll(void)
{
    int r;
    if (!s_cfg_dirty || (scene_top() == s_cfg_scene && dc_frames - s_cfg_tick < CFG_LATE_TICKS)) return;
    s_cfg_dirty = 0;
    r = write_file(DC_VMU_CONFIG, s_cfg, s_cfg_len, "Saving settings...");
    if (r < VMU_OK) forget();           /* the cache said "written": it was not */
}

int plat_config_read(uint8_t *dst, uint32_t cap)
{
    uint8_t tmp[PLAT_CONFIG_MAX];
    uint32_t n = 0;
    if (!dst || !cap) return 0;
    if (!read_file(DC_VMU_CONFIG, tmp, sizeof(tmp), &n, 0) || !n) return 0;
    if (n > cap) n = cap;
    memcpy(dst, tmp, n);
    return (int)n;
}
