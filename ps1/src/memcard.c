/* memcard.c - saves on the memory card in slot 1 through the BIOS file API.
 *
 * One file per save slot (platform.h): bu00:BASLUS-DOOMO0 (story) and
 * bu00:BASLUS-DOOMO1 (free play). File layout, 128-byte frames:
 *
 *   frame 0      title frame: "SC", icon flag 0x13 (3 frames), block count,
 *                title in Shift-JIS, 16-colour icon CLUT
 *   frames 1-3   the 16x16 4bpp icon, 3 animation frames (a pink
 *                Doomogachi egg whose critter blinks and grows horns)
 *   frame 4      our info frame: magic "DMPS", payload size, CRC32
 *   frames 5..   payload (save.c's stream), zero padded to 128 bytes
 *
 * The BIOS cannot resize a file, so a save that needs more blocks than the
 * existing file has is written as a second copy (BASLUS-DOOMO<n>B) first;
 * only once that succeeded is the old file erased - an old save is never
 * destroyed by a failed write (unless the card is too full for two copies,
 * then the old save is replaced). All transfers are whole 8 KB blocks. Every error (no card, unformatted,
 * card full, I/O error) returns 0 to the core, which shows a message.
 */
#include <string.h>
#include <stdio.h>
#include <psxapi.h>
#include <psxetc.h>
#include <psxgpu.h>
#include <hwregs_c.h>
#include "ps1.h"

#define O_RDONLY_  0x0001
#define O_WRONLY_  0x0002
#define O_CREAT_   0x0200
#define FRAME      128
#define BLOCK      8192
#define HDR_FRAMES 5
#define HDR_BYTES  (HDR_FRAMES * FRAME)
#define INFO_MAGIC 0x53504d44u   /* "DMPS" */


static int s_card_ok;

/* Boot order matters (verified on OpenBIOS / PCSX-Redux):
 *   InitPAD -> InitCARD -> StartPAD -> StartCARD   (memcard_early, before
 *   ResetGraph installs the PSn00bSDK IRQ hook), then ResetGraph, then
 *   ChangeClearPAD(0) and _bu_init() with interrupts enabled (memcard_init).
 * Initialising the BIOS SIO0 driver after the SDK hook is installed, or
 * suspending the hook with StopCallback() around card I/O, makes the BIOS
 * card driver stall. */
void memcard_early(void)
{
    InitCARD(1);
    StartPAD();
    StartCARD();
}

void memcard_init(void)
{
    ChangeClearPAD(0);   /* leave the vblank IRQ acknowledge to psxgpu */
    _bu_init();
    s_card_ok = 1;
}

/* bumped around every BIOS card call: a save blocks the game loop for
 * seconds (about one second per 8 KB block on hardware), the test harness
 * treats this as a heartbeat so a long save is not reported as a hang */
volatile uint32_t g_io_beat;

static void bios_io_begin(void) { DrawSync(0); g_io_beat++; }
static void bios_io_end(void) { g_io_beat++; }

/* ---- Shift-JIS title ------------------------------------------------------- */
static int sjis_put(uint8_t *d, char c)
{
    uint16_t code;
    if (c >= 'A' && c <= 'Z')      code = (uint16_t)(0x8260 + (c - 'A'));
    else if (c >= 'a' && c <= 'z') code = (uint16_t)(0x8281 + (c - 'a'));
    else if (c >= '0' && c <= '9') code = (uint16_t)(0x824f + (c - '0'));
    else switch (c) {
        case '!': code = 0x8149; break;
        case '-': code = 0x817c; break;
        case '.': code = 0x8144; break;
        case ':': code = 0x8146; break;
        case '(': code = 0x8169; break;
        case ')': code = 0x816a; break;
        case '?': code = 0x8148; break;
        case '&': code = 0x8195; break;
        case '/': code = 0x815e; break;
        default:  code = 0x8140; break;   /* full-width space */
    }
    d[0] = (uint8_t)(code >> 8);
    d[1] = (uint8_t)code;
    return 2;
}

/* ---- icon -------------------------------------------------------------------- */
/* palette: . transparent  o outline  p pink  l light pink  w white
 *          g LCD green  d dark green  k ink  h horn red  b blush  s shine */
static const char *const k_icon[3][16] = { {
    "......oooo......",
    "....oolllpoo....",
    "...olllwlpppo...",
    "..ollwlpppppbo..",
    "..olpgggggggpo..",
    ".olpggddgddggpo.",
    ".olpgdkgdkgdgpo.",
    ".olpgddgddggggpo",
    ".olpgdgkkgddgpo.",
    ".olpgggdddgggpo.",
    "..olpgggggggpo..",
    "..olppppppppbo..",
    "...oplpoplpppo..",
    "...ooppppppoo...",
    ".....oooooo.....",
    "................" }, {
    "......oooo......",
    "....oolllpoo....",
    "...olllwlpppo...",
    "..ollwlpppppbo..",
    "..olpgggggggpo..",
    ".olpggddgddggpo.",
    ".olpgkkgkkgdgpo.",
    ".olpgddgddggggpo",
    ".olpgdgkkgddgpo.",
    ".olpgggdddgggpo.",
    "..olpgggggggpo..",
    "..olppppppppbo..",
    "...oplpoplpppo..",
    "...ooppppppoo...",
    ".....oooooo.....",
    "................" }, {
    "......oooo......",
    "....oolllpoo....",
    "...olllwlpppo...",
    "..ollwlpppppbo..",
    "..olpghggghgpo..",
    ".olpghhdgdhhgpo.",
    ".olpgdhgdhgdgpo.",
    ".olpgddgddggggpo",
    ".olpgdgkkgddgpo.",
    ".olpgggdddgggpo.",
    "..olpgggggggpo..",
    "..olppppppppbo..",
    "...oplpoplpppo..",
    "...ooppppppoo...",
    ".....oooooo.....",
    "................" } };

static int icon_index(char c)
{
    switch (c) {
    case 'o': return 1; case 'p': return 2; case 'l': return 3; case 'w': return 4;
    case 'g': return 5; case 'd': return 6; case 'k': return 7; case 'h': return 8;
    case 'b': return 9; case 's': return 10; default: return 0;
    }
}

static uint16_t rgb15(int r, int g, int b)
{
    return (uint16_t)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10));
}

static void build_header(uint8_t *h, int slot, uint32_t size, uint32_t crc, int blocks)
{
    static const char *const titles[2] = { "DOOMOGACHI LIFE Story", "DOOMOGACHI LIFE Free Play" };
    const char *t = titles[slot & 1];
    uint16_t *clut;
    int f, x, y, n = 0;
    memset(h, 0, HDR_BYTES);
    h[0] = 'S'; h[1] = 'C';
    h[2] = 0x13;                    /* 3 icon frames, animated */
    h[3] = (uint8_t)blocks;
    while (*t && n < 62)
        n += sjis_put(h + 4 + n, *t++);
    clut = (uint16_t *)(h + 0x60);
    clut[0] = 0;
    clut[1] = rgb15(110, 40, 90);   /* plum outline */
    clut[2] = rgb15(255, 150, 200); /* pink */
    clut[3] = rgb15(255, 205, 230); /* light pink */
    clut[4] = rgb15(255, 255, 255);
    clut[5] = rgb15(170, 215, 120); /* LCD */
    clut[6] = rgb15(90, 140, 70);
    clut[7] = rgb15(30, 50, 30);
    clut[8] = rgb15(230, 40, 50);   /* horns */
    clut[9] = rgb15(255, 110, 140); /* blush */
    clut[10] = rgb15(255, 255, 200);
    for (f = 0; f < 3; f++) {
        uint8_t *d = h + FRAME * (1 + f);
        for (y = 0; y < 16; y++)
            for (x = 0; x < 16; x += 2) {
                int a = icon_index(k_icon[f][y][x]), b = icon_index(k_icon[f][y][x + 1]);
                d[y * 8 + x / 2] = (uint8_t)(a | (b << 4));
            }
    }
    {
        uint8_t *info = h + FRAME * 4;
        uint32_t v[4] = { INFO_MAGIC, size, crc, (uint32_t)slot };
        memcpy(info, v, sizeof(v));
        memcpy(info + 16, "DOOMOGACHI LIFE in HELLvile Park! PS1 save", 43);
    }
}

static uint32_t crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xffffffffu;
    while (n--) {
        int k;
        c ^= *p++;
        for (k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
    }
    return ~c;
}

static void slot_name(char *out, int slot, int alt)
{
    strcpy(out, "bu00:BASLUS-DOOMO0");
    out[17] = (char)('0' + (slot & 1));
    if (alt) { out[18] = 'B'; out[19] = 0; }
}

/* size in bytes of an existing file, or -1 */
static int file_size(const char *path)
{
    struct DIRENTRY e;
    if (!firstfile(path, &e))
        return -1;
    return e.size;
}

static uint8_t s_hdr[HDR_BYTES] __attribute__((aligned(4)));
/* Card I/O is done in whole, block-aligned 8 KB chunks: some BIOSes
 * (OpenBIOS, used by emulators) mis-handle a transfer that starts in the
 * middle of a block and crosses into the next one (it writes to the
 * physically adjacent block instead of following the file's block chain). */
static uint8_t s_blk[BLOCK] __attribute__((aligned(4)));

/* the file image byte at offset o is header (o < HDR_BYTES) or payload */
static void stage_block(int k, const uint8_t *data, uint32_t size)
{
    uint32_t o0 = (uint32_t)k * BLOCK, o1 = o0 + BLOCK, h, d0, d1;
    memset(s_blk, 0, BLOCK);
    if (o0 < HDR_BYTES) {
        h = HDR_BYTES - o0 < BLOCK ? HDR_BYTES - o0 : BLOCK;
        memcpy(s_blk, s_hdr + o0, h);
    }
    d0 = o0 > HDR_BYTES ? o0 - HDR_BYTES : 0;           /* payload range */
    d1 = o1 - HDR_BYTES;
    if (d1 > size) d1 = size;
    if (d1 > d0)
        memcpy(s_blk + (d0 + HDR_BYTES - o0), data + d0, d1 - d0);
}

static int write_file(const char *path, const uint8_t *data, uint32_t size, int blocks)
{
    int fd = open(path, O_WRONLY_), k, ok = 1;
    if (fd < 0)
        return 0;
    for (k = 0; ok && k < blocks; k++) {
        stage_block(k, data, size);
        if (write(fd, s_blk, BLOCK) != BLOCK)
            ok = 0;
        g_io_beat++;
    }
    close(fd);
    return ok;
}

/* blocks used by all files on the card (directory walk) */
static int used_blocks(void)
{
    struct DIRENTRY e;
    int used = 0;
    if (firstfile("bu00:*", &e)) {
        do {
            used += (e.size + BLOCK - 1) / BLOCK;
        } while (nextfile(&e));
    }
    return used;
}

static int create_file(const char *path, int blocks)
{
    int fd = open(path, O_CREAT_ | (blocks << 16));
    if (fd < 0)
        return 0;
    close(fd);
    return 1;
}

/* A save lives in bu00:BASLUS-DOOMO<slot> or, after it had to grow,
 * bu00:BASLUS-DOOMO<slot>B (the BIOS cannot resize files and its rename is
 * not reliable on every BIOS). The newest valid copy wins on load; the old
 * copy is only erased after the new one was written completely. */
static int save_write(int slot, const void *data, uint32_t size)
{
    char name[24], alt[24];
    const char *cur, *other;
    uint32_t total, crc;
    int blocks, have_n, have_a, have, used, ok;
    if (slot < 0 || slot > 1 || !data || !s_card_ok)
        return 0;
    if (size > (slot ? DOOMO_SAVE_SLOT1_MAX : DOOMO_SAVE_SLOT0_MAX))
        return 0;
    total = HDR_BYTES + ((size + FRAME - 1) & ~(uint32_t)(FRAME - 1));
    blocks = (int)((total + BLOCK - 1) / BLOCK);
    crc = crc32((const uint8_t *)data, size);
    build_header(s_hdr, slot, size, crc, blocks);
    slot_name(name, slot, 0);
    slot_name(alt, slot, 1);
    have_n = file_size(name);
    have_a = file_size(alt);
    /* the copy currently in use (if both exist, rewrite the bigger one) */
    if (have_a > have_n) { cur = alt; other = name; have = have_a; }
    else { cur = name; other = alt; have = have_n; }
    if (have >= blocks * BLOCK) {
        ok = write_file(cur, (const uint8_t *)data, size, have / BLOCK);
        if (ok && file_size(other) >= 0)
            erase(other);
        dbg("memcard: wrote %s (%u bytes, in place) -> %d\n", cur, (unsigned)size, ok);
        return ok;
    }
    used = used_blocks();
    if (used + blocks <= 15) {
        /* room for a second copy: write it, then drop the old one */
        if (file_size(other) >= 0) {
            erase(other);
            used = used_blocks();
        }
        if (!create_file(other, blocks)) {
            dbg("memcard: cannot create %s (%d blocks)\n", other, blocks);
            return 0;
        }
        ok = write_file(other, (const uint8_t *)data, size, blocks);
        if (!ok) {
            erase(other);
            dbg("memcard: write error on %s\n", other);
            return 0;
        }
        if (have >= 0)
            erase(cur);
        dbg("memcard: wrote %s (%u bytes, %d blocks, new file)\n", other, (unsigned)size, blocks);
        return 1;
    }
    if (have >= 0 && used - (have + BLOCK - 1) / BLOCK + blocks <= 15) {
        /* only fits by replacing the old save */
        erase(cur);
        if (!create_file(cur, blocks) || !write_file(cur, (const uint8_t *)data, size, blocks)) {
            dbg("memcard: replacing %s failed\n", cur);
            return 0;
        }
        dbg("memcard: wrote %s (%u bytes, %d blocks, replaced)\n", cur, (unsigned)size, blocks);
        return 1;
    }
    dbg("memcard: card full (%d of 15 blocks used, need %d)\n", used, blocks);
    return 0;
}

/* Reads the save in `path`. A buffer smaller than the save gets just its
 * first `cap` bytes (*out_size = cap, no CRC check): the core reads the
 * 48-byte save header this way for its menus (save_peek), which then
 * costs one block instead of the whole file. */
static int read_file(const char *path, void *data, uint32_t cap, uint32_t *out_size)
{
    int fd = open(path, O_RDONLY_), k;
    uint32_t v[4], size = 0, want = 0, o = 0;
    int ok = 0;
    if (fd < 0)
        return 0;
    for (k = 0; ; k++) {
        uint32_t o0 = (uint32_t)k * BLOCK, from, n;
        g_io_beat++;
        if (read(fd, s_blk, BLOCK) != BLOCK)
            goto done;
        if (k == 0) {
            memcpy(s_hdr, s_blk, HDR_BYTES);
            memcpy(v, s_hdr + FRAME * 4, sizeof(v));
            if (s_hdr[0] != 'S' || s_hdr[1] != 'C' || v[0] != INFO_MAGIC)
                goto done;
            size = v[1];
            if (size > DOOMO_SAVE_SLOT1_MAX)
                goto done;
            want = size < cap ? size : cap;
            if (!want)
                break;
        }
        /* payload bytes contained in this block */
        from = o0 < HDR_BYTES ? HDR_BYTES - o0 : 0;
        n = BLOCK - from;
        if (n > want - o) n = want - o;
        memcpy((uint8_t *)data + o, s_blk + from, n);
        o += n;
        if (o >= want)
            break;
    }
    if (want == size && crc32((const uint8_t *)data, size) != v[2]) {
        dbg("memcard: CRC mismatch in %s\n", path);
        goto done;
    }
    *out_size = want;
    ok = 1;
done:
    close(fd);
    return ok;
}

static int save_read(int slot, void *data, uint32_t cap, uint32_t *size)
{
    char name[24];
    uint32_t sz = 0;
    if (slot < 0 || slot > 1 || !data || !s_card_ok)
        return 0;
    /* normally only one of the two names exists (see save_write) */
    slot_name(name, slot, 1);
    if (!read_file(name, data, cap, &sz)) {
        slot_name(name, slot, 0);
        if (!read_file(name, data, cap, &sz))
            return 0;
    }
    if (size)
        *size = sz;
    return 1;
}

static int save_erase(int slot)
{
    char name[24];
    int k, existed = 0;
    if (slot < 0 || slot > 1 || !s_card_ok)
        return 0;
    for (k = 0; k < 2; k++) {
        slot_name(name, slot, k);
        if (file_size(name) >= 0) {
            existed = 1;
            erase(name);
            if (file_size(name) >= 0)
                return 0;       /* still there: I/O error */
        }
    }
    return existed;
}

/* A card inserted (or swapped) after boot is only seen once the BIOS has
 * re-read its directory: if a save fails, re-scan once and retry. */
int plat_save_write(int slot, const void *data, uint32_t size)
{
    int r;
    gfx_busy_banner("Saving... do not remove MEMORY CARD");
    bios_io_begin();
    r = save_write(slot, data, size);
    if (!r) {
        _bu_init();
        r = save_write(slot, data, size);
    }
    bios_io_end();
    gfx_busy_end();
    return r;
}

int plat_save_read(int slot, void *data, uint32_t cap, uint32_t *size)
{
    int r;
    gfx_busy_banner(cap > 2048 ? "Loading MEMORY CARD..." : "Checking MEMORY CARD...");
    bios_io_begin();
    r = save_read(slot, data, cap, size);    /* no re-scan: a missing save is normal */
    bios_io_end();
    gfx_busy_end();
    return r;
}

int plat_save_erase(int slot)
{
    int r;
    gfx_busy_banner("Erasing... do not remove MEMORY CARD");
    bios_io_begin();
    r = save_erase(slot);
    bios_io_end();
    gfx_busy_end();
    return r;
}
