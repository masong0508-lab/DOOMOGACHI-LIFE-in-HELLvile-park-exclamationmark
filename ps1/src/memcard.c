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
 * existing file has is written to a temporary file first; only once that
 * succeeded is the old file erased and the new one renamed - an old save is
 * never destroyed by a failed write. Every error (no card, unformatted,
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

int bios_rename(const char *from, const char *to);

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
    int i;
    ChangeClearPAD(0);   /* leave the vblank IRQ acknowledge to psxgpu */
    for (i = 0; i < 4; i++)
        VSync(0);
    dbg("card: bu_init\n");
    _bu_init();
    s_card_ok = 1;
}

static void bios_io_begin(void) { DrawSync(0); }
static void bios_io_end(void) { }

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

static void slot_name(char *out, int slot, int tmp)
{
    strcpy(out, "bu00:BASLUS-DOOMO0");
    out[17] = (char)('0' + (slot & 1));
    if (tmp) { out[18] = 'T'; out[19] = 0; }
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
static uint8_t s_tail[FRAME] __attribute__((aligned(4)));

static int write_file(const char *path, const uint8_t *data, uint32_t size)
{
    int fd = open(path, O_WRONLY_);
    uint32_t body = size & ~(uint32_t)(FRAME - 1);
    int ok = 1;
    if (fd < 0)
        return 0;
    if (write(fd, s_hdr, HDR_BYTES) != HDR_BYTES)
        ok = 0;
    if (ok && body) {
        if (((uintptr_t)data & 3) == 0) {
            if (write(fd, data, body) != (int)body)
                ok = 0;
        } else {
            uint32_t o;
            for (o = 0; ok && o < body; o += FRAME) {
                memcpy(s_tail, data + o, FRAME);
                if (write(fd, s_tail, FRAME) != FRAME)
                    ok = 0;
            }
        }
    }
    if (ok && size > body) {
        memset(s_tail, 0, FRAME);
        memcpy(s_tail, data + body, size - body);
        if (write(fd, s_tail, FRAME) != FRAME)
            ok = 0;
    }
    close(fd);
    return ok;
}

static int save_write(int slot, const void *data, uint32_t size)
{
    char name[24], tmp[24];
    uint32_t total, crc;
    int blocks, have;
    if (slot < 0 || slot > 1 || !data || !s_card_ok)
        return 0;
    if (size > (slot ? DOOMO_SAVE_SLOT1_MAX : DOOMO_SAVE_SLOT0_MAX))
        return 0;
    total = HDR_BYTES + ((size + FRAME - 1) & ~(uint32_t)(FRAME - 1));
    blocks = (int)((total + BLOCK - 1) / BLOCK);
    crc = crc32((const uint8_t *)data, size);
    build_header(s_hdr, slot, size, crc, blocks);
    slot_name(name, slot, 0);
    slot_name(tmp, slot, 1);
    have = file_size(name);
    if (have >= blocks * BLOCK) {
        int ok = write_file(name, (const uint8_t *)data, size);
        dbg("memcard: wrote %s (%u bytes, %d blocks, in place) -> %d\n", name,
            (unsigned)size, have / BLOCK, ok);
        return ok;
    }
    /* needs a (bigger) new file: write a temp file, then swap */
    if (file_size(tmp) >= 0)
        erase(tmp);
    {
        int fd = open(tmp, O_CREAT_ | (blocks << 16));
        if (fd < 0) {
            dbg("memcard: cannot create %s (%d blocks): no card / card full\n", tmp, blocks);
            return 0;
        }
        close(fd);
    }
    if (!write_file(tmp, (const uint8_t *)data, size)) {
        erase(tmp);
        dbg("memcard: write error on %s\n", tmp);
        return 0;
    }
    if (have >= 0)
        erase(name);
    if (!bios_rename(tmp, name)) {
        /* BIOS rename returns 1 on success; on failure keep the temp file
         * (plat_save_read falls back to it) */
        dbg("memcard: rename %s -> %s failed\n", tmp, name);
    }
    dbg("memcard: wrote %s (%u bytes, %d blocks)\n", name, (unsigned)size, blocks);
    return 1;
}

static int read_file(const char *path, void *data, uint32_t cap, uint32_t *out_size)
{
    int fd = open(path, O_RDONLY_);
    uint32_t v[4], size, body;
    int ok = 0;
    if (fd < 0)
        return 0;
    if (read(fd, s_hdr, HDR_BYTES) != HDR_BYTES)
        goto done;
    memcpy(v, s_hdr + FRAME * 4, sizeof(v));
    if (s_hdr[0] != 'S' || s_hdr[1] != 'C' || v[0] != INFO_MAGIC)
        goto done;
    size = v[1];
    if (size > cap || size > DOOMO_SAVE_SLOT1_MAX)
        goto done;
    body = size & ~(uint32_t)(FRAME - 1);
    if (body) {
        if (((uintptr_t)data & 3) == 0) {
            if (read(fd, data, body) != (int)body)
                goto done;
        } else {
            uint32_t o;
            for (o = 0; o < body; o += FRAME) {
                if (read(fd, s_tail, FRAME) != FRAME)
                    goto done;
                memcpy((uint8_t *)data + o, s_tail, FRAME);
            }
        }
    }
    if (size > body) {
        if (read(fd, s_tail, FRAME) != FRAME)
            goto done;
        memcpy((uint8_t *)data + body, s_tail, size - body);
    }
    if (crc32((const uint8_t *)data, size) != v[2]) {
        dbg("memcard: CRC mismatch in %s\n", path);
        goto done;
    }
    *out_size = size;
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
    slot_name(name, slot, 0);
    if (!read_file(name, data, cap, &sz)) {
        /* an interrupted swap leaves the new save in the temp file */
        slot_name(name, slot, 1);
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
    int ok = 0;
    if (slot < 0 || slot > 1 || !s_card_ok)
        return 0;
    slot_name(name, slot, 0);
    if (file_size(name) >= 0)
        ok = erase(name) ? 1 : 0;
    slot_name(name, slot, 1);
    if (file_size(name) >= 0)
        erase(name);
    return ok;
}

int plat_save_write(int slot, const void *data, uint32_t size)
{
    int r;
    bios_io_begin();
    r = save_write(slot, data, size);
    bios_io_end();
    return r;
}

int plat_save_read(int slot, void *data, uint32_t cap, uint32_t *size)
{
    int r;
    bios_io_begin();
    r = save_read(slot, data, cap, size);
    bios_io_end();
    return r;
}

int plat_save_erase(int slot)
{
    int r;
    bios_io_begin();
    r = save_erase(slot);
    bios_io_end();
    return r;
}
